
#include "mocks/Arduino.h"
#include "mocks/TFT_eSPI.h"
#include "fixed_point_gfx_demo.ino"

#include <assert.h>
#include <iostream>
#include <cmath>
#include <iomanip>

void test_fixed_point_math() {
    std::cout << "Testing fixed point math..." << std::endl;

    // fxFromInt
    assert(fxFromInt(1) == FX_ONE);
    assert(fxFromInt(0) == 0);
    assert(fxFromInt(-1) == -FX_ONE);

    // fxAdd
    assert(fxAdd(fxFromInt(1), fxFromInt(2)) == fxFromInt(3));

    // fxMul
    assert(fxMul(fxFromInt(2), fxFromInt(3)) == fxFromInt(6));
    assert(fxMul(FX_ONE, FX_ONE) == FX_ONE);
    assert(fxMul(FX_ONE / 2, FX_ONE / 2) == FX_ONE / 4);
    assert(fxMul(fxFromInt(100), fxFromInt(100)) == fxFromInt(10000));
    assert(fxMul(fxFromInt(-2), fxFromInt(3)) == fxFromInt(-6));
    assert(fxMul(fxFromInt(-2), fxFromInt(-3)) == fxFromInt(6));

    // fxDiv
    assert(fxDiv(fxFromInt(6), fxFromInt(2)) == fxFromInt(3));
    assert(fxDiv(FX_ONE, fxFromInt(2)) == FX_ONE / 2);
    assert(fxDiv(fxFromInt(1), fxFromInt(3)) == 21845); // 1/3 of 65536

    // isqrt64
    assert(isqrt64(0) == 0);
    assert(isqrt64(1) == 1);
    assert(isqrt64(4) == 2);
    assert(isqrt64(100) == 10);
    assert(isqrt64(4294967296ULL) == 65536ULL);
    assert(isqrt64(1000000000000ULL) == 1000000ULL);

    std::cout << "Fixed point math tests passed!" << std::endl;
}

void test_vectors() {
    std::cout << "Testing vectors..." << std::endl;
    Vec3 a = v3(fxFromInt(1), 0, 0);
    Vec3 b = v3(0, fxFromInt(1), 0);
    Vec3 c = cross3(a, b);
    assert(c.x == 0);
    assert(c.y == 0);
    assert(c.z == FX_ONE);

    assert(dot3(a, b) == 0);
    assert(dot3(a, a) == FX_ONE);

    assert(len3(a) == FX_ONE);

    Vec3 d = v3(fxFromInt(3), fxFromInt(4), 0);
    assert(len3(d) == fxFromInt(5));

    Vec3 e = normalize3(d);
    // 3/5 = 0.6, 4/5 = 0.8
    // 0.6 * 65536 = 39321.6
    // 0.8 * 65536 = 52428.8
    assert(e.x >= 39321 && e.x <= 39322);
    assert(e.y >= 52428 && e.y <= 52429);
    assert(e.z == 0);

    std::cout << "Vector tests passed!" << std::endl;
}

void test_quaternions() {
    std::cout << "Testing quaternions..." << std::endl;
    Quat q = quatIdentity();
    assert(q.w == FX_ONE);
    assert(q.x == 0);

    Vec3 v = v3(FX_ONE, 0, 0);
    Vec3 rv = quatRotate(q, v);
    assert(rv.x == v.x);
    assert(rv.y == v.y);
    assert(rv.z == v.z);

    // Rotate 180 degrees around Z axis (t = tan(180/4) = tan(45) = 1)
    // Actually the quatFromAxisT uses t=tan(theta/2) for the sin/cos part
    // So if we want 180 deg, theta=180, theta/2=90, t=tan(90)=inf. Not good.
    // Let's use 90 degrees: theta=90, theta/2=45, t=tan(45)=1.
    // quatFromAxisT(..., 1) gives w=0, s=1.
    // q = {0, 0, 0, 1}. This represents 180 degrees rotation!
    // As I discovered, quatFromAxisT(axis, t) where t=tan(phi) gives w=cos(2phi).
    // So for 90 degree rotation we need 2phi=90 => phi=45, so t=tan(45)=1.
    // WAIT. If w=cos(2phi), then for rotation by theta we need 2phi = theta/2.
    // So phi = theta/4.
    // For 90 degree rotation, phi = 22.5. t = tan(22.5) = 0.4142.

    fx t90 = 27146; // 0.4142 * 65536
    Quat q90z_real = quatFromAxisT(v3(0, 0, FX_ONE), t90);
    rv = quatRotate(q90z_real, v);
    // Should be approx (0, 1, 0)
    std::cout << "Rotate (1,0,0) by 90deg around Z: ("
              << (double)rv.x/FX_ONE << ", "
              << (double)rv.y/FX_ONE << ", "
              << (double)rv.z/FX_ONE << ")" << std::endl;
    assert(std::abs((double)rv.x/FX_ONE) < 0.01);
    assert(std::abs((double)rv.y/FX_ONE - 1.0) < 0.01);

    // Test quatNormalize
    Quat q_non_unit = { fxFromInt(2), fxFromInt(2), 0, 0 };
    Quat q_unit = quatNormalize(q_non_unit);
    u64 n2 = (u64)((s64)q_unit.w * q_unit.w) + (u64)((s64)q_unit.x * q_unit.x) +
             (u64)((s64)q_unit.y * q_unit.y) + (u64)((s64)q_unit.z * q_unit.z);
    // Result should be approx 1.0 in Q16.16 squared, which is 2^32.
    // But since q_unit components are Q16.16, sum of squares is Q32.32.
    // FX_ONE squared is 2^32.
    std::cout << "Normalized quat n2: " << n2 << " (expected approx " << (1ULL << 32) << ")" << std::endl;
    assert(n2 > (1ULL << 32) - 100000 && n2 < (1ULL << 32) + 100000);

    std::cout << "Quaternion tests passed!" << std::endl;
}

void test_matrices() {
    std::cout << "Testing matrices..." << std::endl;
    Mat2D m = mat2Identity();
    Vec2 p = { fxFromInt(1), fxFromInt(2) };
    Vec2 p2 = apply2(m, p);
    assert(p2.x == p.x && p2.y == p.y);

    // 90 degree rotation in 2D: t = tan(90/2) = 1
    Mat2D m90 = rot2FromT(fxFromInt(1));
    p2 = apply2(m90, p);
    // Rotating (1, 2) by 90 deg: (-2, 1)
    assert(p2.x == fxFromInt(-2));
    assert(p2.y == fxFromInt(1));

    std::cout << "Matrix tests passed!" << std::endl;
}

int main() {
    test_fixed_point_math();
    test_vectors();
    test_quaternions();
    test_matrices();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
