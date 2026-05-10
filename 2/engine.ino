#include <Arduino.h>

// Fixed-point scaling (8 bits for fractional part)
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

    // Generates a rotation matrix from rational parameters
    // This avoids sin/cos and PI entirely.
    void updateRotation(RationalQuat q) {
        int32_t w2 = (q.w * q.w);
        int32_t x2 = (q.x * q.x);
        int32_t y2 = (q.y * q.y);
        int32_t z2 = (q.z * q.z);
        int32_t norm = w2 + x2 + y2 + z2;

        if (norm == 0) return;

        // Row 0
        matrix[0][0] = ((w2 + x2 - y2 - z2) * SCALE) / norm;
        matrix[0][1] = (2 * (q.x * q.y - q.w * q.z) * SCALE) / norm;
        matrix[0][2] = (2 * (q.x * q.z + q.w * q.y) * SCALE) / norm;
        // Row 1
        matrix[1][0] = (2 * (q.x * q.y + q.w * q.z) * SCALE) / norm;
        matrix[1][1] = ((w2 - x2 + y2 - z2) * SCALE) / norm;
        matrix[1][2] = (2 * (q.y * q.z - q.w * q.x) * SCALE) / norm;
        // Row 2
        matrix[2][0] = (2 * (q.x * q.z - q.w * q.y) * SCALE) / norm;
        matrix[2][1] = (2 * (q.y * q.z + q.w * q.x) * SCALE) / norm;
        matrix[2][2] = ((w2 - x2 - y2 + z2) * SCALE) / norm;
    }

    // Exact Transformation: Rotation + Projection
    // Projection uses similar triangles: x' = (x * focal_length) / z
    void project(Vec3 p, int16_t &outX, int16_t &outY, int32_t focalLength) {
        // Rotate
        int32_t rx = (p.x * matrix[0][0] + p.y * matrix[0][1] + p.z * matrix[0][2]) / SCALE;
        int32_t ry = (p.x * matrix[1][0] + p.y * matrix[1][1] + p.z * matrix[1][2]) / SCALE;
        int32_t rz = (p.x * matrix[2][0] + p.y * matrix[2][1] + p.z * matrix[2][2]) / SCALE;

        // Offset Z to prevent division by zero (camera distance)
        rz += TO_FIXED(5); 

        // Project to 2D
        if (rz != 0) {
            outX = (rx * focalLength) / rz;
            outY = (ry * focalLength) / rz;
        }
    }
};

ExactEngine3D engine;
Vec3 cube[4] = {
    {TO_FIXED(1), TO_FIXED(1), TO_FIXED(0)},
    {TO_FIXED(-1), TO_FIXED(1), TO_FIXED(0)},
    {TO_FIXED(-1), TO_FIXED(-1), TO_FIXED(0)},
    {TO_FIXED(1), TO_FIXED(-1), TO_FIXED(0)}
};

void setup() {
    Serial.begin(115200);
    
    // Example: A rotation defined by rational quaternion (2, 1, 0, 0)
    // This corresponds to a specific, exact angle without using PI.
    RationalQuat rotation = {2, 1, 0, 0}; 
    engine.updateRotation(rotation);

    Serial.println("Projected Coordinates:");
    for(int i=0; i<4; i++) {
        int16_t x2d, y2d;
        engine.project(cube[i], x2d, y2d, 120); // 120 = focal length
        Serial.printf("Point %d: [%d, %d]\n", i, x2d, y2d);
    }
}

void loop() {}
