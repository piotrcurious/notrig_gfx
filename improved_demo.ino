#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

// --- EXACT ARITHMETIC: Q(sqrt(3)) Field Extension ---
// Represents a number: a + b*sqrt(3)
struct Surd3 {
    int32_t a;
    int32_t b;
};

struct ExactVec2 {
    Surd3 x, y;
};

// Rotates 60 degrees exactly.
// Requires initial coordinates to be even to avoid precision loss in division by 2.
inline void rotate60Exact(ExactVec2* v) {
    int32_t new_x_a = (v->x.a - 3 * v->y.b) / 2;
    int32_t new_x_b = (v->x.b - v->y.a) / 2;
    int32_t new_y_a = (3 * v->x.b + v->y.a) / 2;
    int32_t new_y_b = (v->x.a + v->y.b) / 2;

    v->x.a = new_x_a; v->x.b = new_x_b;
    v->y.a = new_y_a; v->y.b = new_y_b;
}

// --- RATIONAL 3D ENGINE ---
#define SCALE 256
#define TO_FIXED(x) ((int32_t)((x) * SCALE))

struct Vec3 {
    int32_t x, y, z;
};

struct RationalQuat {
    int32_t w, x, y, z;
};

class ExactEngine3D {
public:
    int32_t matrix[3][3];

    void updateRotation(RationalQuat q) {
        int64_t w2 = (int64_t)q.w * q.w;
        int64_t x2 = (int64_t)q.x * q.x;
        int64_t y2 = (int64_t)q.y * q.y;
        int64_t z2 = (int64_t)q.z * q.z;
        int64_t norm = w2 + x2 + y2 + z2;

        if (norm == 0) return;

        matrix[0][0] = (int32_t)(((w2 + x2 - y2 - z2) * SCALE) / norm);
        matrix[0][1] = (int32_t)((2 * ((int64_t)q.x * q.y - (int64_t)q.w * q.z) * SCALE) / norm);
        matrix[0][2] = (int32_t)((2 * ((int64_t)q.x * q.z + (int64_t)q.w * q.y) * SCALE) / norm);

        matrix[1][0] = (int32_t)((2 * ((int64_t)q.x * q.y + (int64_t)q.w * q.z) * SCALE) / norm);
        matrix[1][1] = (int32_t)(((w2 - x2 + y2 - z2) * SCALE) / norm);
        matrix[1][2] = (int32_t)((2 * ((int64_t)q.y * q.z - (int64_t)q.w * q.x) * SCALE) / norm);

        matrix[2][0] = (int32_t)((2 * ((int64_t)q.x * q.z - (int64_t)q.w * q.y) * SCALE) / norm);
        matrix[2][1] = (int32_t)((2 * ((int64_t)q.y * q.z + (int64_t)q.w * q.x) * SCALE) / norm);
        matrix[2][2] = (int32_t)(((w2 - x2 - y2 + z2) * SCALE) / norm);
    }

    void project(Vec3 p, int16_t &outX, int16_t &outY, int32_t focalLength, int32_t cx, int32_t cy) {
        int32_t rx = (p.x * matrix[0][0] + p.y * matrix[0][1] + p.z * matrix[0][2]) / SCALE;
        int32_t ry = (p.x * matrix[1][0] + p.y * matrix[1][1] + p.z * matrix[1][2]) / SCALE;
        int32_t rz = (p.x * matrix[2][0] + p.y * matrix[2][1] + p.z * matrix[2][2]) / SCALE;

        rz += TO_FIXED(5);

        if (rz > 0) {
            outX = cx + (rx * focalLength) / rz;
            outY = cy - (ry * focalLength) / rz;
        } else {
            outX = -999; outY = -999;
        }
    }
};

ExactEngine3D engine;
Vec3 tetrahedron[4] = {
    {TO_FIXED(1), TO_FIXED(1), TO_FIXED(1)},
    {TO_FIXED(-1), TO_FIXED(-1), TO_FIXED(1)},
    {TO_FIXED(-1), TO_FIXED(1), TO_FIXED(-1)},
    {TO_FIXED(1), TO_FIXED(-1), TO_FIXED(-1)}
};

uint8_t tetraEdges[6][2] = {
    {0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}
};

ExactVec2 hex[6];
RationalQuat q = {40, 1, 2, 3}; // Initial rotation

void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Initialize exact hexagon with even coordinates for exact division
    hex[0] = {{64, 0}, {0, 0}};
    for(int i=1; i<6; i++) {
        hex[i] = hex[i-1];
        rotate60Exact(&hex[i]);
    }
}

void loop() {
    tft.fillScreen(TFT_BLACK);

    // 2D Exact Rotation Demo
    int32_t cx2d = 60, cy2d = 60;
    float sqrt3 = 1.73205081f;
    for(int i=0; i<6; i++) {
        int next = (i+1)%6;
        int32_t x1 = cx2d + hex[i].x.a + (int32_t)(hex[i].x.b * sqrt3);
        int32_t y1 = cy2d + hex[i].y.a + (int32_t)(hex[i].y.b * sqrt3);
        int32_t x2 = cx2d + hex[next].x.a + (int32_t)(hex[next].x.b * sqrt3);
        int32_t y2 = cy2d + hex[next].y.a + (int32_t)(hex[next].y.b * sqrt3);
        tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
    }
    // Rotate the hexagon
    for(int i=0; i<6; i++) rotate60Exact(&hex[i]);

    // 3D Rational Rotation Demo
    engine.updateRotation(q);
    int16_t sx[4], sy[4];
    for(int i=0; i<4; i++) {
        engine.project(tetrahedron[i], sx[i], sy[i], 120, 200, 120);
    }
    for(int i=0; i<6; i++) {
        tft.drawLine(sx[tetraEdges[i][0]], sy[tetraEdges[i][0]],
                     sx[tetraEdges[i][1]], sy[tetraEdges[i][1]], TFT_WHITE);
    }

    // Benchmark info
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 200);
    tft.print("Exact Hexagon + Rational 3D");

    // Update quaternion incrementally without sin/cos
    // q_new = q * dq. dq = {1, 0.01, 0.02, 0.03} approx.
    // In integers: dq = {100, 1, 2, 1}
    RationalQuat dq = {100, 1, 2, 1};
    int32_t nw = (q.w * dq.w - q.x * dq.x - q.y * dq.y - q.z * dq.z) / 100;
    int32_t nx = (q.w * dq.x + q.x * dq.w + q.y * dq.z - q.z * dq.y) / 100;
    int32_t ny = (q.w * dq.y - q.x * dq.z + q.y * dq.w + q.z * dq.x) / 100;
    int32_t nz = (q.w * dq.z + q.x * dq.y - q.y * dq.x + q.z * dq.w) / 100;
    q = {nw, nx, ny, nz};

    // Prevent overflow
    if (abs(q.w) > 1000) {
        q.w /= 2; q.x /= 2; q.y /= 2; q.z /= 2;
    }

    delay(20);
}
