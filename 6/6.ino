#include <Arduino.h>
#include <TFT_eSPI.h>

/*
 * GALOIS FIELD VIRTUAL MACHINE - 3D SYMBOLIC ENGINE (v4)
 *
 * "Points are Programs, Rotations are Instructions"
 *
 * v4 Improvements:
 * - Recursive Program Generation: Building complex structures by nesting algebraic states.
 * - Instruction Fusion: Enhanced simplify() to maintain VM performance.
 * - Recursive Tree Demo: A fractal tree where each node is a sub-program.
 */

#define SCALE 1024
#define MAX_LAYERS 16
#define MAX_TERMS_PER_VEC 32  // Increased for recursive structures
#define MAX_DEPTH 8           // Deeper recursion for trees

const int16_t SIN_TABLE[91] = {
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160,
    178, 195, 213, 230, 248, 265, 282, 299, 316, 333,
    350, 367, 384, 400, 416, 433, 449, 465, 481, 496,
    512, 527, 543, 558, 573, 587, 602, 616, 630, 644,
    658, 672, 685, 698, 711, 724, 737, 749, 761, 773,
    784, 796, 807, 818, 828, 839, 849, 859, 869, 878,
    887, 896, 905, 913, 922, 930, 937, 945, 952, 959,
    966, 972, 978, 984, 989, 994, 999, 1003, 1007, 1011,
    1014, 1017, 1020, 1022, 1023, 1024, 1024, 1024, 1024, 1024, 1024
};

int32_t fx_sin(int32_t angle) {
    angle %= 360; if (angle < 0) angle += 360;
    if (angle <= 90) return SIN_TABLE[angle];
    if (angle <= 180) return SIN_TABLE[180 - angle];
    if (angle <= 270) return -SIN_TABLE[angle - 180];
    return -SIN_TABLE[360 - angle];
}
int32_t fx_cos(int32_t angle) { return fx_sin(angle + 90); }

struct SymbolicTerm {
    int32_t v[3];
    int8_t layers[MAX_DEPTH];
    int8_t axes[MAX_DEPTH];
    int8_t depth = 0;
    SymbolicTerm() { v[0]=v[1]=v[2]=0; depth=0; }
    bool hasSameHistory(const SymbolicTerm& other) const {
        if (depth != other.depth) return false;
        for (int i = 0; i < depth; i++)
            if (layers[i] != other.layers[i] || axes[i] != other.axes[i]) return false;
        return true;
    }
};

struct AlgebraicVec3 {
    SymbolicTerm terms[MAX_TERMS_PER_VEC];
    int num_terms = 0;
    AlgebraicVec3() : num_terms(0) {}
    static AlgebraicVec3 fromRational(int32_t x, int32_t y, int32_t z) {
        AlgebraicVec3 res; res.num_terms = 1;
        res.terms[0].v[0] = x; res.terms[0].v[1] = y; res.terms[0].v[2] = z;
        return res;
    }
    void simplify() {
        for (int i = 0; i < num_terms; i++) {
            if (terms[i].v[0] == 0 && terms[i].v[1] == 0 && terms[i].v[2] == 0) {
                for (int k = i; k < num_terms - 1; k++) terms[k] = terms[k + 1];
                num_terms--; i--; continue;
            }
            for (int j = i + 1; j < num_terms; j++) {
                if (terms[i].hasSameHistory(terms[j])) {
                    terms[i].v[0] += terms[j].v[0]; terms[i].v[1] += terms[j].v[1]; terms[i].v[2] += terms[j].v[2];
                    for (int k = j; k < num_terms - 1; k++) terms[k] = terms[k + 1];
                    num_terms--; j--;
                }
            }
        }
    }
};

struct MorphismStack {
    int32_t cos_val[MAX_LAYERS];
    int32_t sin_val[MAX_LAYERS];
    int current_layers = 0;
    int pushRotation(int32_t angleDegrees) {
        if (current_layers >= MAX_LAYERS) return -1;
        cos_val[current_layers] = fx_cos(angleDegrees);
        sin_val[current_layers] = fx_sin(angleDegrees);
        return current_layers++;
    }
    void reset() { current_layers = 0; }
};

class RuntimeExactEngine {
public:
    void applyRotation(AlgebraicVec3 &v, int layer, int axis) {
        if (layer < 0 || layer >= MAX_LAYERS) return;
        for (int i = 0; i < v.num_terms; i++) {
            if (v.terms[i].depth < MAX_DEPTH) {
                v.terms[i].layers[v.terms[i].depth] = (int8_t)layer;
                v.terms[i].axes[v.terms[i].depth] = (int8_t)axis;
                v.terms[i].depth++;
            }
        }
        v.simplify();
    }
    void add(AlgebraicVec3 &out, const AlgebraicVec3 &a, const AlgebraicVec3 &b) {
        out.num_terms = 0;
        int n = a.num_terms; if (n > MAX_TERMS_PER_VEC) n = MAX_TERMS_PER_VEC;
        for (int i = 0; i < n; i++) out.terms[out.num_terms++] = a.terms[i];
        int m = b.num_terms; if (out.num_terms + m > MAX_TERMS_PER_VEC) m = MAX_TERMS_PER_VEC - out.num_terms;
        for (int i = 0; i < m; i++) out.terms[out.num_terms++] = b.terms[i];
        out.simplify();
    }
    void collapse(const AlgebraicVec3 &v, const MorphismStack &stack, int32_t &ox, int32_t &oy, int32_t &oz) {
        ox = oy = oz = 0;
        for (int i = 0; i < v.num_terms; i++) {
            int64_t cx = v.terms[i].v[0], cy = v.terms[i].v[1], cz = v.terms[i].v[2];
            for (int d = 0; d < v.terms[i].depth; d++) {
                int l = v.terms[i].layers[d]; int ax = v.terms[i].axes[d];
                int64_t cV = stack.cos_val[l]; int64_t sV = stack.sin_val[l];
                int64_t nx, ny, nz;
                if (ax == 2) { nx = (cx * cV - cy * sV + 512) >> 10; ny = (cx * sV + cy * cV + 512) >> 10; nz = cz; }
                else if (ax == 0) { nx = cx; ny = (cy * cV - cz * sV + 512) >> 10; nz = (cy * sV + cz * cV + 512) >> 10; }
                else { nx = (cx * cV + cz * sV + 512) >> 10; ny = cy; nz = (-cx * sV + cz * cV + 512) >> 10; }
                cx = nx; cy = ny; cz = nz;
            }
            ox += (int32_t)cx; oy += (int32_t)cy; oz += (int32_t)cz;
        }
    }
    bool project(const AlgebraicVec3 &v, const MorphismStack &stack, int16_t &sx, int16_t &sy, int w, int h) {
        int32_t x, y, z; collapse(v, stack, x, y, z);
        z += (500 * SCALE); if (z < 20 * SCALE) return false;
        sx = w / 2 + (int16_t)(((int64_t)x * 400) / z); sy = h / 2 - (int16_t)(((int64_t)y * 400) / z);
        return true;
    }
};

RuntimeExactEngine engine;
MorphismStack worldStack;
TFT_eSPI tft;

// Recursive program generator: Fractal Tree
// Each node is an AlgebraicVec3 program composed of its parent's history
void drawSymbolicTree(AlgebraicVec3 base, int height, int layerL, int layerR, int depth) {
    if (depth == 0) return;

    // Trunk vertex
    AlgebraicVec3 trunkEnd = AlgebraicVec3::fromRational(0, height * SCALE, 0);
    engine.add(trunkEnd, trunkEnd, base); // Move trunk relative to base

    int16_t sx1, sy1, sx2, sy2;
    if (engine.project(base, worldStack, sx1, sy1, tft.width(), tft.height()) &&
        engine.project(trunkEnd, worldStack, sx2, sy2, tft.width(), tft.height())) {
        tft.drawLine(sx1, sy1, sx2, sy2, (depth == 1) ? TFT_GREEN : TFT_BROWN);
    }

    // Left branch program
    AlgebraicVec3 left = AlgebraicVec3::fromRational(0, (height * 3 / 4) * SCALE, 0);
    engine.applyRotation(left, layerL, 2); // Rotate Z
    engine.add(left, left, trunkEnd);      // Attach to trunk end

    // Right branch program
    AlgebraicVec3 right = AlgebraicVec3::fromRational(0, (height * 3 / 4) * SCALE, 0);
    engine.applyRotation(right, layerR, 2); // Rotate Z
    engine.add(right, right, trunkEnd);      // Attach to trunk end

    drawSymbolicTree(left, height * 3 / 4, layerL, layerR, depth - 1);
    drawSymbolicTree(right, height * 3 / 4, layerL, layerR, depth - 1);
}

void setup() {
    tft.init(); tft.setRotation(1);
}

void loop() {
    static int frame = 0;
    tft.fillScreen(TFT_BLACK);
    worldStack.reset();

    // VM State
    int lL = worldStack.pushRotation(25 + fx_sin(frame) / 64);
    int lR = worldStack.pushRotation(-25 + fx_cos(frame) / 64);
    int lWind = worldStack.pushRotation(fx_sin(frame / 2) / 128);

    AlgebraicVec3 root = AlgebraicVec3::fromRational(0, -100 * SCALE, 0);
    engine.applyRotation(root, lWind, 0); // Wind affects the base

    drawSymbolicTree(root, 60, lL, lR, 5);

    frame++;
    delay(30);
}
