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

// --- Surd3 Operators ---
inline Surd3 sAdd(Surd3 u, Surd3 v) { return {u.a + v.a, u.b + v.b}; }
inline Surd3 sSub(Surd3 u, Surd3 v) { return {u.a - v.a, u.b - v.b}; }
inline Surd3 sDiv3(Surd3 u) { return {u.a / 3, u.b / 3}; }

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

// Rotates -60 degrees exactly.
inline void rotateNeg60Exact(ExactVec2* v) {
    int32_t new_x_a = (v->x.a + 3 * v->y.b) / 2;
    int32_t new_x_b = (v->x.b + v->y.a) / 2;
    int32_t new_y_a = (-3 * v->x.b + v->y.a) / 2;
    int32_t new_y_b = (v->x.a - v->y.b) / 2;

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

    Vec3 transform(Vec3 p) {
        return {
            (p.x * matrix[0][0] + p.y * matrix[0][1] + p.z * matrix[0][2]) / SCALE,
            (p.x * matrix[1][0] + p.y * matrix[1][1] + p.z * matrix[1][2]) / SCALE,
            (p.x * matrix[2][0] + p.y * matrix[2][1] + p.z * matrix[2][2]) / SCALE + TO_FIXED(5)
        };
    }

    bool project(Vec3 p, int16_t &outX, int16_t &outY, int32_t focalLength, int32_t cx, int32_t cy) {
        if (p.z <= 0) return false;
        outX = cx + (p.x * focalLength) / p.z;
        outY = cy - (p.y * focalLength) / p.z;
        return true;
    }

    void drawClippedLine(Vec3 a, Vec3 b, int32_t focalLength, int32_t cx, int32_t cy, uint16_t color) {
        int32_t nearZ = 10; // small positive epsilon
        if (a.z < nearZ && b.z < nearZ) return;

        if (a.z < nearZ) {
            int32_t t = ((nearZ - a.z) << 8) / (b.z - a.z);
            a.x = a.x + ((b.x - a.x) * t >> 8);
            a.y = a.y + ((b.y - a.y) * t >> 8);
            a.z = nearZ;
        } else if (b.z < nearZ) {
            int32_t t = ((nearZ - b.z) << 8) / (a.z - b.z);
            b.x = b.x + ((a.x - b.x) * t >> 8);
            b.y = b.y + ((a.y - b.y) * t >> 8);
            b.z = nearZ;
        }

        int16_t x0, y0, x1, y1;
        if (project(a, x0, y0, focalLength, cx, cy) && project(b, x1, y1, focalLength, cx, cy)) {
            tft.drawLine(x0, y0, x1, y1, color);
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

void drawKoch(ExactVec2 p1, ExactVec2 p2, int level, int32_t cx, int32_t cy, float sqrt3) {
    if (level == 0) {
        int32_t x1 = cx + p1.x.a + (int32_t)(p1.x.b * sqrt3);
        int32_t y1 = cy + p1.y.a + (int32_t)(p1.y.b * sqrt3);
        int32_t x2 = cx + p2.x.a + (int32_t)(p2.x.b * sqrt3);
        int32_t y2 = cy + p2.y.a + (int32_t)(p2.y.b * sqrt3);
        tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
        return;
    }

    // p1, p2, p3, p4, p5
    ExactVec2 delta = {{sSub(p2.x, p1.x)}, {sSub(p2.y, p1.y)}};
    ExactVec2 third = {{sDiv3(delta.x)}, {sDiv3(delta.y)}};

    ExactVec2 p3 = {{sAdd(p1.x, third.x)}, {sAdd(p1.y, third.y)}};
    ExactVec2 p5 = {{sSub(p2.x, third.x)}, {sSub(p2.y, third.y)}};

    ExactVec2 p4_rel = third;
    rotateNeg60Exact(&p4_rel);
    ExactVec2 p4 = {{sAdd(p3.x, p4_rel.x)}, {sAdd(p3.y, p4_rel.y)}};

    drawKoch(p1, p3, level - 1, cx, cy, sqrt3);
    drawKoch(p3, p4, level - 1, cx, cy, sqrt3);
    drawKoch(p4, p5, level - 1, cx, cy, sqrt3);
    drawKoch(p5, p2, level - 1, cx, cy, sqrt3);
}

void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Initialize exact hexagon with coordinates divisible by 3^N to allow exact Koch divisions
    // For level 2, we need 3*3 = 9. 162 is divisible by 2 and 9.
    hex[0] = {{162, 0}, {0, 0}};
    for(int i=1; i<6; i++) {
        hex[i] = hex[i-1];
        rotate60Exact(&hex[i]);
    }
}

void loop() {
    tft.fillScreen(TFT_BLACK);

    // 2D Exact Rotation Demo: Koch Snowflake
    int32_t cx2d = 80, cy2d = 80;
    float sqrt3 = 1.73205081f;
    for(int i=0; i<6; i++) {
        int next = (i+1)%6;
        drawKoch(hex[i], hex[next], 2, cx2d, cy2d, sqrt3);
    }
    // Rotate the hexagon base exactly
    for(int i=0; i<6; i++) rotate60Exact(&hex[i]);

    // 3D Rational Rotation Demo
    engine.updateRotation(q);
    Vec3 tv[4];
    for(int i=0; i<4; i++) {
        tv[i] = engine.transform(tetrahedron[i]);
    }
    for(int i=0; i<6; i++) {
        engine.drawClippedLine(tv[tetraEdges[i][0]], tv[tetraEdges[i][1]], 120, 200, 120, TFT_WHITE);
    }

    // Benchmark info
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 200);
    tft.print("Exact Hexagon + Rational 3D");

    // Update quaternion incrementally without sin/cos
    // q_new = q * dq. dq = {1, 0.01, 0.02, 0.03} approx.
    // In integers: dq = {200, 1, 2, 1}
    RationalQuat dq = {200, 1, 2, 1};
    int64_t nw = ((int64_t)q.w * dq.w - (int64_t)q.x * dq.x - (int64_t)q.y * dq.y - (int64_t)q.z * dq.z) / 200;
    int64_t nx = ((int64_t)q.w * dq.x + (int64_t)q.x * dq.w + (int64_t)q.y * dq.z - (int64_t)q.z * dq.y) / 200;
    int64_t ny = ((int64_t)q.w * dq.y - (int64_t)q.x * dq.z + (int64_t)q.y * dq.w + (int64_t)q.z * dq.x) / 200;
    int64_t nz = ((int64_t)q.w * dq.z + (int64_t)q.x * dq.y - (int64_t)q.y * dq.x + (int64_t)q.z * dq.w) / 200;
    q = {(int32_t)nw, (int32_t)nx, (int32_t)ny, (int32_t)nz};

    // Prevent overflow and maintain precision
    if (abs(q.w) > 4000) {
        q.w /= 4; q.x /= 4; q.y /= 4; q.z /= 4;
    }

    delay(20);
}
