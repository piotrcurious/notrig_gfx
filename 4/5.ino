#include <Arduino.h>

#define SCALE 1024

// ---------------------------------------------------------
// 1. BASE FIELD (Level 0): Rational / Fixed Point
// ---------------------------------------------------------
struct FixedQ {
    int32_t val;

    // Base case for collapse: just return the integer value
    int32_t collapse(const int32_t* cos_vals, const int32_t* sin_vals, int depth) const {
        return val;
    }
};

// ---------------------------------------------------------
// 2. TOWER EXTENSION (Level N): E_n = E_{n-1}[c, s]
// ---------------------------------------------------------
template <typename SubField>
struct TowerExt {
    SubField r; // Constant term (relative to this extension)
    SubField c; // Coefficient for cos(theta_n)
    SubField s; // Coefficient for sin(theta_n)

    // Recursively collapses the tower from the outside in
    int32_t collapse(const int32_t* cos_vals, const int32_t* sin_vals, int depth) const {
        // Evaluate the subfields
        int32_t base_r = r.collapse(cos_vals, sin_vals, depth - 1);
        int32_t base_c = c.collapse(cos_vals, sin_vals, depth - 1);
        int32_t base_s = s.collapse(cos_vals, sin_vals, depth - 1);

        // Apply the current extension's trigonometric residuals
        return base_r + ((base_c * cos_vals[depth]) / SCALE) + ((base_s * sin_vals[depth]) / SCALE);
    }
};

// ---------------------------------------------------------
// 3. 3D VECTOR IN ARBITRARY FIELD
// ---------------------------------------------------------
template <typename Field>
struct Vec3 {
    Field x, y, z;
};

// ---------------------------------------------------------
// 4. ALGEBRAIC EMBEDDING (The Rotation Morphism)
// ---------------------------------------------------------
class NestedEngine3D {
public:
    // Rotates a vector around the Z-axis, lifting it from field F to field TowerExt<F>
    // Notice: There is ZERO math here. It is a pure structural permutation.
    template <typename F>
    Vec3<TowerExt<F>> rotateZ(const Vec3<F>& v) {
        Vec3<TowerExt<F>> out;
        
        // X' = x*cos - y*sin
        out.x.r = {0}; out.x.c = v.x; out.x.s = negateField(v.y);
        
        // Y' = x*sin + y*cos
        out.y.r = {0}; out.y.c = v.y; out.y.s = v.x;
        
        // Z' = z
        out.z.r = v.z; out.z.c = {0}; out.z.s = {0};
        
        return out;
    }

    template <typename F>
    Vec3<TowerExt<F>> rotateX(const Vec3<F>& v) {
        Vec3<TowerExt<F>> out;
        out.x.r = v.x; out.x.c = {0}; out.x.s = {0};
        out.y.r = {0}; out.y.c = v.y; out.y.s = negateField(v.z);
        out.z.r = {0}; out.z.c = v.z; out.z.s = v.y;
        return out;
    }

private:
    // Helper to negate a field exactly
    FixedQ negateField(const FixedQ& f) { return {-f.val}; }
    
    template <typename F>
    TowerExt<F> negateField(const TowerExt<F>& f) {
        return {negateField(f.r), negateField(f.c), negateField(f.s)};
    }
};

// ---------------------------------------------------------
// 5. DEMONSTRATION OF ARBITRARY NESTING
// ---------------------------------------------------------
NestedEngine3D engine;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- Compile-Time Nested Field Engine ---");

    // Define starting geometry strictly in the Rational base field (Level 0)
    Vec3<FixedQ> vertex = { {100 * SCALE}, {0}, {0} };

    // ROTATION 1: Lift to Level 1 (e.g., Yaw by 12.34 degrees)
    Vec3<TowerExt<FixedQ>> v_level1 = engine.rotateZ(vertex);

    // ROTATION 2: Lift to Level 2 (e.g., Pitch by 0.75 degrees)
    Vec3<TowerExt<TowerExt<FixedQ>>> v_level2 = engine.rotateX(v_level1);

    // Provide the arbitrary residuals ONLY at the final evaluation step
    // Index 0: Base field (unused)
    // Index 1: Z-rotation (12.34 deg)
    // Index 2: X-rotation (0.75 deg)
    int32_t cos_residuals[3] = {0, 1000, 1023}; // Pre-scaled cos values
    int32_t sin_residuals[3] = {0, 218, 13};    // Pre-scaled sin values

    // Collapse the entire tower directly to a screen coordinate
    int32_t finalX = v_level2.x.collapse(cos_residuals, sin_residuals, 2);
    int32_t finalY = v_level2.y.collapse(cos_residuals, sin_residuals, 2);
    int32_t finalZ = v_level2.z.collapse(cos_residuals, sin_residuals, 2);

    Serial.println("Mathematical State (v_level2) is a deeply nested 3D polynomial.");
    Serial.printf("Collapsed Pixel Coordinates: X:%d, Y:%d, Z:%d\n", 
                  finalX / SCALE, finalY / SCALE, finalZ / SCALE);
}

void loop() {}
