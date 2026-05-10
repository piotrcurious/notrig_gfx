#include "mocks/Arduino.h"
#include "mocks/TFT_eSPI.h"
#include "symbolic_demo.ino"

#include <assert.h>
#include <iostream>
#include <vector>

void test_symbolic_math() {
    std::cout << "Testing symbolic math..." << std::endl;
    SymbolicEngine eng;

    // Test multiplication: 1 * 1
    AlgebraicNum one = {1024, 0};
    AlgebraicNum res = eng.mul(one, one);
    assert(res.r == 1024);
    assert(res.i == 0);

    // Test multiplication: sqrt(3) * sqrt(3) = 3
    AlgebraicNum sqrt3 = {0, 1024};
    res = eng.mul(sqrt3, sqrt3);
    // (0 + 1*sqrt(3)) * (0 + 1*sqrt(3)) = 3 + 0*sqrt(3)
    assert(res.r == 3 * 1024);
    assert(res.i == 0);

    // Test 30 degree rotation
    // After 12 rotations of 30 degrees, we should be back to start.
    Vector3E p = {{100 << 10, 0}, {0, 0}, {0, 0}};
    Vector3E start = p;

    std::cout << "Starting 12x 30deg rotations..." << std::endl;
    for(int i = 0; i < 12; i++) {
        p = eng.rotateZ(p, ROT_30);
        std::cout << "Step " << i+1 << ": [" << (double)p.x.r/1024 << " + " << (double)p.x.i/1024 << "sqrt(3), "
                  << (double)p.y.r/1024 << " + " << (double)p.y.i/1024 << "sqrt(3)]" << std::endl;
    }

    // Check drift
    // Due to rounding in mul ((r+512)>>10), there might be tiny drift,
    // but algebraic engines usually aim for ZERO drift if possible.
    // Let's see how much we have.
    std::cout << "Final: [" << (double)p.x.r/1024 << ", " << (double)p.x.i/1024 << "]" << std::endl;

    // With 10-bit precision and rounding, let's see.
    assert(std::abs(p.x.r - start.x.r) < 100); // Allow some small error for now
    assert(std::abs(p.x.i - start.x.i) < 100);

    std::cout << "Symbolic math tests passed!" << std::endl;
}

void test_rendering() {
    std::cout << "Testing rendering..." << std::endl;
    tft.enable_framebuffer(true);
    setup();
    // Run a few loops
    for(int i=0; i<11; i++) {
        loop();
    }
    tft.save_ppm("symbolic_cube.ppm");
    std::cout << "Rendered frame saved to symbolic_cube.ppm" << std::endl;
}

int main() {
    test_symbolic_math();
    test_rendering();
    return 0;
}
