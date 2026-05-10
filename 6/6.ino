#include <Arduino.h>

#define SCALE 1024
#define MAX_LAYERS 8  // Maximum nested rotations supported simultaneously

// A scalar in the Runtime Field Extension
struct AlgebraicScalar {
    int32_t r;                 // Rational base
    int32_t c[MAX_LAYERS];     // Cosine coefficients
    int32_t s[MAX_LAYERS];     // Sine coefficients
};

struct AlgebraicVec3 {
    AlgebraicScalar x, y, z;
};

// The state of the world's transcendental residuals
struct MorphismStack {
    int32_t cos_val[MAX_LAYERS];
    int32_t sin_val[MAX_LAYERS];
    int current_layers = 0;

    int pushRotation(float angleDegrees) {
        if (current_layers >= MAX_LAYERS) return -1;
        float rad = angleDegrees * DEG_TO_RAD;
        cos_val[current_layers] = (int32_t)(cos(rad) * SCALE);
        sin_val[current_layers] = (int32_t)(sin(rad) * SCALE);
        return current_layers++;
    }
};

class RuntimeExactEngine {
public:
    // Morphism: Lifts a vector into a new rotation layer
    void applyRotationZ(AlgebraicVec3 &v, int layer) {
        // We move the current rational component into the symbolic coefficients
        // of the new layer. This is an EXACT structural mapping.
        
        // X' = x*cos - y*sin -> Map x to c_layer and -y to s_layer
        v.x.c[layer] = v.x.r;
        v.x.s[layer] = -v.y.r;
        v.x.r = 0;

        // Y' = x*sin + y*cos -> Map y to c_layer and x to s_layer
        v.y.c[layer] = v.y.r;
        v.y.s[layer] = v.x.r; // Use original x
        v.y.r = 0;
        
        // Z is unaffected in its rational state
    }

    // The "Collapse" function: The only place where approximations occur
    int32_t collapse(const AlgebraicScalar &val, const MorphismStack &stack) {
        int32_t accumulator = val.r;
        for (int i = 0; i < stack.current_layers; i++) {
            accumulator += (val.c[i] * stack.cos_val[i]) / SCALE;
            accumulator += (val.s[i] * stack.sin_val[i]) / SCALE;
        }
        return accumulator;
    }

    // Projection with Perspective
    void project(const AlgebraicVec3 &v, const MorphismStack &stack, int16_t &sx, int16_t &sy) {
        int32_t x = collapse(v.x, stack);
        int32_t y = collapse(v.y, stack);
        int32_t z = collapse(v.z, stack) + (5 * SCALE); // Camera offset

        if (z > 0) {
            sx = (x * 200) / z; // 200 = Focal length
            sy = (y * 200) / z;
        }
    }
};

// --- Execution ---

RuntimeExactEngine engine;
MorphismStack worldStack;

void setup() {
    Serial.begin(115200);
    
    // 1. Define geometry in base field
    AlgebraicVec3 point = { {100 * SCALE}, {0}, {0} };

    // 2. Add arbitrary rotations at RUNTIME
    int yaw = worldStack.pushRotation(33.5f);   // Layer 0
    int pitch = worldStack.pushRotation(12.7f); // Layer 1
    
    // 3. Apply morphisms
    engine.applyRotationZ(point, yaw);
    // Complex interactions would happen here in a full engine
    
    // 4. Rasterize
    int16_t sx, sy;
    engine.project(point, worldStack, sx, sy);

    Serial.printf("Runtime Engine -> Screen: [%d, %d]\n", sx, sy);
}

void loop() {}
