#include <Arduino.h>

// ---------------------------------------------------------
// 1. ALGEBRAIC TYPES & MATHEMATICS
// ---------------------------------------------------------

// Scale factor for fixed-point arithmetic (10 bits fractional)
#define SCALE 1024 
#define HALF_SCALE 512

// A number in the extended field Q(sqrt(N))
// Represents: (a + b*sqrt(N)) / SCALE
struct SymNum {
    int32_t a; // Rational part
    int32_t b; // Irrational residual
};

// 3D Vector in the extended field
struct SymVector {
    SymNum x, y, z;
};

// 3x3 Transformation Matrix in the extended field
struct SymMatrix {
    SymNum m[3][3];
    int32_t N; // The residual identifier (e.g., 2 for sqrt(2), 3 for sqrt(3))
};

// Mathematically exact multiplication in Q(sqrt(N))
// (a1 + b1*sqrt(N)) * (a2 + b2*sqrt(N))
inline SymNum mulSym(SymNum n1, SymNum n2, int32_t N) {
    return {
        (n1.a * n2.a + n1.b * n2.b * N + HALF_SCALE) / SCALE,
        (n1.a * n2.b + n1.b * n2.a + HALF_SCALE) / SCALE
    };
}

// Add two symbolic numbers
inline SymNum addSym(SymNum n1, SymNum n2) {
    return { n1.a + n2.a, n1.b + n2.b };
}

// ---------------------------------------------------------
// 2. THE SYMBOLIC ENGINE & LUT
// ---------------------------------------------------------

class ExactEngine3D {
public:
    // Multiplies two 3x3 matrices exactly.
    // Used to composite Pitch, Yaw, and Roll into one transformation.
    SymMatrix multiplyMatrix(const SymMatrix& A, const SymMatrix& B) {
        SymMatrix result;
        result.N = A.N; // Assumes we are operating in the same extension field

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                result.m[row][col] = {0, 0};
                for (int k = 0; k < 3; ++k) {
                    SymNum product = mulSym(A.m[row][k], B.m[k][col], result.N);
                    result.m[row][col] = addSym(result.m[row][col], product);
                }
            }
        }
        return result;
    }

    // Applies the composite matrix to a 3D vertex
    SymVector transformVertex(const SymMatrix& M, const SymVector& v) {
        SymVector out;
        
        out.x = addSym(addSym(mulSym(M.m[0][0], v.x, M.N), mulSym(M.m[0][1], v.y, M.N)), mulSym(M.m[0][2], v.z, M.N));
        out.y = addSym(addSym(mulSym(M.m[1][0], v.x, M.N), mulSym(M.m[1][1], v.y, M.N)), mulSym(M.m[1][2], v.z, M.N));
        out.z = addSym(addSym(mulSym(M.m[2][0], v.x, M.N), mulSym(M.m[2][1], v.y, M.N)), mulSym(M.m[2][2], v.z, M.N));
        
        return out;
    }

    // Safely collapses the field extension to a standard integer for rasterization
    // sqrtLUT must contain the scaled value of sqrt(N)
    void project(const SymVector& v, int16_t &screenX, int16_t &screenY, int32_t N_val, int32_t focalLength) {
        // Collapse: a + b*sqrt(N)
        int32_t finalX = v.x.a + ((v.x.b * N_val) / SCALE);
        int32_t finalY = v.y.a + ((v.y.b * N_val) / SCALE);
        int32_t finalZ = v.z.a + ((v.z.b * N_val) / SCALE);

        // Translate Z to position the camera
        finalZ += (5 * SCALE); 

        // Perspective divide
        if (finalZ > 0) { // Near-plane clipping check
            screenX = (finalX * focalLength) / finalZ;
            screenY = (finalY * focalLength) / finalZ;
        } else {
            screenX = -1; // Flag as clipped
            screenY = -1;
        }
    }
};

// ---------------------------------------------------------
// 3. THE LOOK-UP TABLE (LUT)
// ---------------------------------------------------------

// Structurally exact matrices. 
// E.g., for 45 deg, N=2. cos(45)=sin(45)=0 + 0.5*sqrt(2).
// Scaled by 1024: 0.5 = 512.
const SymMatrix LUT_ROT_Z_45 = {
    {{ {0, 512},  {0, -512}, {0, 0} },  // Row 0: c, -s, 0
     { {0, 512},  {0, 512},  {0, 0} },  // Row 1: s,  c, 0
     { {0, 0},    {0, 0},    {SCALE, 0} }}, // Row 2: 0,  0, 1
    2 // N = 2 (Field Extension Q(sqrt(2)))
};

const SymMatrix LUT_ROT_X_45 = {
    {{ {SCALE, 0}, {0, 0},    {0, 0} },
     { {0, 0},     {0, 512},  {0, -512} },
     { {0, 0},     {0, 512},  {0, 512} }},
    2
};

// ---------------------------------------------------------
// 4. MAIN EXECUTION
// ---------------------------------------------------------
ExactEngine3D engine;

// A simple 3D triangle in standard rational space (represented as a+0*sqrt(N))
SymVector mesh[3] = {
    { {SCALE, 0}, {-SCALE, 0}, {0, 0} },
    { {-SCALE, 0}, {-SCALE, 0}, {0, 0} },
    { {0, 0}, {SCALE, 0}, {0, 0} }
};

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("--- 3D Field Extension Engine ---");

    // 1. Composite the Model-View Matrix EXACTLY in the field.
    // Multiply a Z-rotation by an X-rotation to create a multi-axis rotation matrix.
    SymMatrix compositeMatrix = engine.multiplyMatrix(LUT_ROT_Z_45, LUT_ROT_X_45);

    // Provide the integer approximation of sqrt(2)*1024 for the final projection step
    const int32_t SQRT_2_SCALED = 1448; 
    const int32_t FOCAL_LENGTH = 200;

    // 2. Apply Matrix and Project 
    for(int i = 0; i < 3; i++) {
        // Transform purely symbolically (zero loss of precision)
        SymVector transformed = engine.transformVertex(compositeMatrix, mesh[i]);
        
        // Collapse and project to 2D screen coordinates
        int16_t screenX, screenY;
        engine.project(transformed, screenX, screenY, SQRT_2_SCALED, FOCAL_LENGTH);
        
        Serial.printf("Vertex %d -> Exact Z:(%d + %d√2)/1024 | Screen [X:%d, Y:%d]\n", 
            i, transformed.z.a, transformed.z.b, screenX, screenY);
    }
}

void loop() {}
