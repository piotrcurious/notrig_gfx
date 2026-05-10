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
    int16_t sx, sy;
    engine.project(p, sx, sy, 100, 0, 0);

    // rz = 0 + 5 = 5 (TO_FIXED(5))
    // rx = 1 * SCALE / SCALE = 1 (TO_FIXED(1))
    // sx = 0 + (TO_FIXED(1) * 100) / TO_FIXED(5) = 100 / 5 = 20
    assert(sx == 20);
    assert(sy == 0);

    std::cout << "Rational 3D tests passed!" << std::endl;
}

int main() {
    test_surd3();
    test_rational_3d();
    std::cout << "All improved tests passed!" << std::endl;
    return 0;
}
