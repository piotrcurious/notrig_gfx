#include <Arduino.h>
#include <TFT_eSPI.h>

// ---------------------------------------------------------
// SYMBOLIC FIELD EXTENSION ENGINE: Q(sqrt(3))
// Represents numbers as (r + i*sqrt(3)) / 1024
// ---------------------------------------------------------

struct AlgebraicNum {
    int32_t r; // Rational part * 1024
    int32_t i; // Irrational part * 1024
};

struct Vector3E {
    AlgebraicNum x, y, z;
};

// Symbolic Morphism: encodes a rotation or transformation
struct SymRotation {
    AlgebraicNum cos_val;
    AlgebraicNum sin_val;
};

// LUT for exact rotations in Q(sqrt(3))
// 30 degrees: cos = sqrt(3)/2, sin = 1/2
// In our (r + i*sqrt(3))/1024 format:
// cos = (0 + 512*sqrt(3))/1024
// sin = (512 + 0*sqrt(3))/1024
const SymRotation ROT_30 = {{0, 512}, {512, 0}};
// 60 degrees: cos = 1/2, sin = sqrt(3)/2
const SymRotation ROT_60 = {{512, 0}, {0, 512}};
// 90 degrees: cos = 0, sin = 1
const SymRotation ROT_90 = {{0, 0}, {1024, 0}};

class SymbolicEngine {
public:
    // Multiply (r1 + i1*sqrt(3))/1024 * (r2 + i2*sqrt(3))/1024
    // Result is ( (r1*r2 + 3*i1*i2) + (r1*i2 + i1*r2)*sqrt(3) ) / 1024^2
    // We scale back to / 1024
    AlgebraicNum mul(AlgebraicNum a, AlgebraicNum b) {
        int64_t r = (int64_t)a.r * b.r + 3LL * a.i * b.i;
        int64_t i = (int64_t)a.r * b.i + (int64_t)a.i * b.r;
        return {
            (int32_t)((r + 512) >> 10),
            (int32_t)((i + 512) >> 10)
        };
    }

    // Rotate vector around Z axis
    Vector3E rotateZ(Vector3E v, SymRotation rot) {
        // x' = x*cos - y*sin
        // y' = x*sin + y*cos
        AlgebraicNum x_cos = mul(v.x, rot.cos_val);
        AlgebraicNum y_sin = mul(v.y, rot.sin_val);
        AlgebraicNum x_sin = mul(v.x, rot.sin_val);
        AlgebraicNum y_cos = mul(v.y, rot.cos_val);

        return {
            {x_cos.r - y_sin.r, x_cos.i - y_sin.i},
            {x_sin.r + y_cos.r, x_sin.i + y_cos.i},
            v.z
        };
    }

    // Rotate vector around Y axis
    Vector3E rotateY(Vector3E v, SymRotation rot) {
        // x' = x*cos + z*sin
        // z' = -x*sin + z*cos
        AlgebraicNum x_cos = mul(v.x, rot.cos_val);
        AlgebraicNum z_sin = mul(v.z, rot.sin_val);
        AlgebraicNum x_sin = mul(v.x, rot.sin_val);
        AlgebraicNum z_cos = mul(v.z, rot.cos_val);

        return {
            {x_cos.r + z_sin.r, x_cos.i + z_sin.i},
            v.y,
            {-x_sin.r + z_cos.r, -x_sin.i + z_cos.i}
        };
    }

    // Rotate vector around X axis
    Vector3E rotateX(Vector3E v, SymRotation rot) {
        // y' = y*cos - z*sin
        // z' = y*sin + z*cos
        AlgebraicNum y_cos = mul(v.y, rot.cos_val);
        AlgebraicNum z_sin = mul(v.z, rot.sin_val);
        AlgebraicNum y_sin = mul(v.y, rot.sin_val);
        AlgebraicNum z_cos = mul(v.z, rot.cos_val);

        return {
            v.x,
            {y_cos.r - z_sin.r, y_cos.i - z_sin.i},
            {y_sin.r + z_cos.r, y_sin.i + z_cos.i}
        };
    }

    void project(Vector3E p, int16_t &sx, int16_t &sy, int width, int height) {
        // sqrt(3) approx 1.73205... * 1024 = 1773.6 -> 1774 or 1773.
        // Let's use 1773 as in engine.ino
        int32_t x = (p.x.r + ((p.x.i * 1773) >> 10));
        int32_t y = (p.y.r + ((p.y.i * 1773) >> 10));
        int32_t z = (p.z.r + ((p.z.i * 1773) >> 10)) + (200 << 10); // Offset Z by 200

        // Fixed point projection
        // x, y, z are all scaled by 1024
        // To project, we need x_screen = focal * (x_world / z_world)
        // Let focal = 256.
        // x and z are both scaled by 1024, so (x/1024) / (z/1024) = x/z.
        if (z < 1024) z = 1024; // Simple near-plane clipping
        sx = width / 2 + (int32_t)((int64_t)x * 256 / z);
        sy = height / 2 + (int32_t)((int64_t)y * 256 / z);
    }
};

SymbolicEngine engine;
TFT_eSPI tft;

Vector3E cubeVertices[8] = {
    {{-50<<10, 0}, {-50<<10, 0}, {-50<<10, 0}},
    {{ 50<<10, 0}, {-50<<10, 0}, {-50<<10, 0}},
    {{ 50<<10, 0}, { 50<<10, 0}, {-50<<10, 0}},
    {{-50<<10, 0}, { 50<<10, 0}, {-50<<10, 0}},
    {{-50<<10, 0}, {-50<<10, 0}, { 50<<10, 0}},
    {{ 50<<10, 0}, {-50<<10, 0}, { 50<<10, 0}},
    {{ 50<<10, 0}, { 50<<10, 0}, { 50<<10, 0}},
    {{-50<<10, 0}, { 50<<10, 0}, { 50<<10, 0}}
};

int cubeEdges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
};

void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    static int frame = 0;
    tft.fillScreen(TFT_BLACK);

    // Every 10 frames, rotate by 30 degrees on different axes
    if (frame % 10 == 0) {
        for(int i=0; i<8; i++) {
            cubeVertices[i] = engine.rotateX(cubeVertices[i], ROT_30);
            cubeVertices[i] = engine.rotateY(cubeVertices[i], ROT_30);
            cubeVertices[i] = engine.rotateZ(cubeVertices[i], ROT_30);
        }
    }

    // Draw edges
    for(int i=0; i<12; i++) {
        int16_t x0, y0, x1, y1;
        engine.project(cubeVertices[cubeEdges[i][0]], x0, y0, tft.width(), tft.height());
        engine.project(cubeVertices[cubeEdges[i][1]], x1, y1, tft.width(), tft.height());
        tft.drawLine(x0, y0, x1, y1, TFT_GREEN);
    }

    frame++;
    delay(50);
}
