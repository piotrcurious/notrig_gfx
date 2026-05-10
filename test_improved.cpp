#include "mocks/Arduino.h"
#include "mocks/TFT_eSPI.h"
#include "improved_demo.ino"

#include <assert.h>
#include <iostream>
#include <cmath>

void test_surd3() {
    std::cout << "Testing Surd3 exact rotations..." << std::endl;
    ExactVec2 v = {{2, 0}, {0, 0}}; // X=2, Y=0

    // Rotate 60 degrees 6 times = 360 degrees
    for(int i=0; i<6; i++) {
        rotate60Exact(&v);
    }

    // Should be exactly back to {{2, 0}, {0, 0}}
    assert(v.x.a == 2 && v.x.b == 0);
    assert(v.y.a == 0 && v.y.b == 0);
    std::cout << "Surd3 tests passed!" << std::endl;
}

void test_rational_3d() {
    std::cout << "Testing rational 3D engine..." << std::endl;
    ExactEngine3D engine;
    RationalQuat q = {1, 0, 0, 0}; // Identity
    engine.updateRotation(q);

    // Identity matrix
    assert(engine.matrix[0][0] == SCALE);
    assert(engine.matrix[1][1] == SCALE);
    assert(engine.matrix[2][2] == SCALE);
    assert(engine.matrix[0][1] == 0);

    Vec3 p = {TO_FIXED(1), 0, 0};
    Vec3 tp = engine.transform(p);
    int16_t sx, sy;
    engine.project(tp, sx, sy, 100, 0, 0);

    // rx = 1 * SCALE / SCALE = 1 (TO_FIXED(1))
    // rz = 0 + TO_FIXED(5) = 1280
    // sx = 0 + (256 * 100) / 1280 = 25600 / 1280 = 20
    assert(sx == 20);
    assert(sy == 0);

    std::cout << "Rational 3D tests passed!" << std::endl;
}

void test_clipping() {
    std::cout << "Testing line clipping..." << std::endl;
    ExactEngine3D engine;
    engine.updateRotation({1, 0, 0, 0});

    // a is behind nearZ=10, b is in front
    Vec3 a = {0, 0, -100};
    Vec3 b = {0, 0, 100};

    tft.clear_mock_data();
    engine.drawClippedLine(a, b, 100, 0, 0, TFT_WHITE);

    assert(tft.lines.size() == 1);
    // Clipped line should start at nearZ=10
    // x0 = (0 * 100) / 10 = 0
    // y0 = (0 * 100) / 10 = 0
    // x1 = (0 * 100) / 100 = 0
    // y1 = (0 * 100) / 100 = 0
    // Well, it's a vertical line at origin. Let's try offset.

    a = {TO_FIXED(-10), 0, -TO_FIXED(10)};
    b = {TO_FIXED(10), 0, TO_FIXED(10)};
    // nearZ = 10. transform() adds TO_FIXED(5) to Z.
    // wait, drawClippedLine expects ALREADY transformed coordinates.

    Vec3 ta = { -100, 0, -100 };
    Vec3 tb = { 100, 0, 100 };
    tft.clear_mock_data();
    engine.drawClippedLine(ta, tb, 100, 0, 0, TFT_WHITE);

    // nearZ = 10. t = ((10 - (-100)) << 8) / (100 - (-100)) = (110 << 8) / 200 = 140
    // t_normalized = 140 / 256 approx 0.546875
    // a'.x = -100 + (200 * 140 >> 8) = -100 + 109 = 9
    // a'.z = 10
    // project(a') -> sx = (9 * 100) / 10 = 90
    // project(b)  -> sx = (100 * 100) / 100 = 100

    std::cout << "Clipped x0: " << tft.lines[0].x0 << " x1: " << tft.lines[0].x1 << std::endl;
    assert(tft.lines.size() == 1);
    assert(tft.lines[0].x0 == 90);
    assert(tft.lines[0].x1 == 100);

    std::cout << "Clipping tests passed!" << std::endl;
}

int main() {
    test_surd3();
    test_rational_3d();
    test_clipping();
    std::cout << "All improved tests passed!" << std::endl;
    return 0;
}
