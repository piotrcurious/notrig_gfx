#include <Arduino.h>

// ---------------------------------------------------------
// EXACT GEOMETRY ENGINE: Q(sqrt(3)) Field Extension
// ---------------------------------------------------------

// Represents a number in the form: a + b*sqrt(3)
// We pre-scale our initial geometry by 2 to allow exact integer division
struct Surd3 {
    int32_t a; // Rational component
    int32_t b; // Irrational residual component (coefficient of sqrt(3))
};

// 2D Vector in the exact field
struct ExactVector {
    Surd3 x;
    Surd3 y;
};

// Rotates a vector by exactly 60 degrees.
// Mathematically: X' = X*cos(60) - Y*sin(60) | Y' = X*sin(60) + Y*cos(60)
// Using Q(sqrt(3)), this reduces strictly to integer arithmetic.
// The compiler will optimize "/ 2" into a fast arithmetic bit-shift (>> 1).
inline void rotate60Exact(ExactVector* v) {
    // X' = (Xa - 3Yb)/2 + (Xb - Ya)/2 * sqrt(3)
    int32_t new_x_a = (v->x.a - 3 * v->y.b) / 2;
    int32_t new_x_b = (v->x.b - v->y.a) / 2;
    
    // Y' = (3Xb + Ya)/2 + (Xa + Yb)/2 * sqrt(3)
    int32_t new_y_a = (3 * v->x.b + v->y.a) / 2;
    int32_t new_y_b = (v->x.a + v->y.b) / 2;

    v->x.a = new_x_a;
    v->x.b = new_x_b;
    v->y.a = new_y_a;
    v->y.b = new_y_b;
}

// ---------------------------------------------------------
// STANDARD FPU ENGINE (For Baseline Comparison)
// ---------------------------------------------------------
struct FloatVector {
    float x;
    float y;
};

inline void rotate60Float(FloatVector* v) {
    // Pre-calculated to simulate standard matrix multiplication
    static const float c = 0.5f; 
    static const float s = 0.86602540378f; // approx sqrt(3)/2

    float new_x = v->x * c - v->y * s;
    float new_y = v->x * s + v->y * c;

    v->x = new_x;
    v->y = new_y;
}

// ---------------------------------------------------------
// BENCHMARKING SUITE
// ---------------------------------------------------------
const int ITERATIONS = 1200000; // 200,000 full 360-degree cycles

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("--- ESP32 Exact Geometric Computation Engine ---");
    Serial.printf("Executing %d rotations...\n\n", ITERATIONS);

    // 1. RUN EXACT INTEGER ENGINE
    // Starting vector: X = 2 + 0*sqrt(3), Y = 0 + 0*sqrt(3)
    ExactVector exactVec = {{2, 0}, {0, 0}};
    
    uint32_t startCyclesExact = ESP.getCycleCount();
    for(int i = 0; i < ITERATIONS; i++) {
        rotate60Exact(&exactVec);
    }
    uint32_t endCyclesExact = ESP.getCycleCount();

    // 2. RUN STANDARD FLOAT ENGINE
    FloatVector floatVec = {2.0f, 0.0f};

    uint32_t startCyclesFloat = ESP.getCycleCount();
    for(int i = 0; i < ITERATIONS; i++) {
        rotate60Float(&floatVec);
    }
    uint32_t endCyclesFloat = ESP.getCycleCount();

    // 3. OUTPUT RESULTS
    Serial.println("[ EXACT ENGINE (Q(sqrt(3)) Field Extension) ]");
    Serial.printf("Final Vector : X=(%d + %d*sqrt(3)), Y=(%d + %d*sqrt(3))\n", 
                  exactVec.x.a, exactVec.x.b, exactVec.y.a, exactVec.y.b);
    Serial.println("Error / Drift: ABSOLUTE ZERO");
    Serial.printf("CPU Cycles   : %u\n\n", (endCyclesExact - startCyclesExact));

    Serial.println("[ STANDARD ENGINE (32-bit Float) ]");
    Serial.printf("Final Vector : X=%.6f, Y=%.6f\n", floatVec.x, floatVec.y);
    Serial.printf("Error / Drift: X off by %.6f\n", abs(2.0f - floatVec.x));
    Serial.printf("CPU Cycles   : %u\n\n", (endCyclesFloat - startCyclesFloat));
}

void loop() {
    // Single execution run
}
