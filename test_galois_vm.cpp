#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>
#include <cmath>

#define SCALE 1024
#define MAX_LAYERS 16
#define MAX_TERMS_PER_VEC 16
#define MAX_DEPTH 6

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
    int32_t val1[MAX_LAYERS]; int32_t val2[MAX_LAYERS]; int current_layers = 0;
    int pushRotation(int32_t angleDegrees) {
        if (current_layers >= MAX_LAYERS) return -1;
        val1[current_layers] = fx_cos(angleDegrees); val2[current_layers] = fx_sin(angleDegrees);
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
    void morph(AlgebraicVec3 &out, const AlgebraicVec3 &a, const AlgebraicVec3 &b, int32_t alpha) {
        out.num_terms = 0;
        int32_t inv_alpha = 1024 - alpha;
        for (int i = 0; i < a.num_terms && out.num_terms < MAX_TERMS_PER_VEC; i++) {
            SymbolicTerm t = a.terms[i];
            t.v[0] = (int32_t)(((int64_t)t.v[0] * inv_alpha) >> 10);
            t.v[1] = (int32_t)(((int64_t)t.v[1] * inv_alpha) >> 10);
            t.v[2] = (int32_t)(((int64_t)t.v[2] * inv_alpha) >> 10);
            out.terms[out.num_terms++] = t;
        }
        for (int i = 0; i < b.num_terms && out.num_terms < MAX_TERMS_PER_VEC; i++) {
            SymbolicTerm t = b.terms[i];
            t.v[0] = (int32_t)(((int64_t)t.v[0] * alpha) >> 10);
            t.v[1] = (int32_t)(((int64_t)t.v[1] * alpha) >> 10);
            t.v[2] = (int32_t)(((int64_t)t.v[2] * alpha) >> 10);
            out.terms[out.num_terms++] = t;
        }
        out.simplify();
    }
    void collapse(const AlgebraicVec3 &v, const MorphismStack &stack, int32_t &ox, int32_t &oy, int32_t &oz) {
        ox = oy = oz = 0;
        for (int i = 0; i < v.num_terms; i++) {
            int64_t cx = v.terms[i].v[0], cy = v.terms[i].v[1], cz = v.terms[i].v[2];
            for (int d = 0; d < v.terms[i].depth; d++) {
                int l = v.terms[i].layers[d]; int ax = v.terms[i].axes[d];
                int64_t cV = stack.val1[l]; int64_t sV = stack.val2[l];
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
        z += (450 * SCALE); if (z < 20 * SCALE) return false;
        sx = w / 2 + (int16_t)(((int64_t)x * 350) / z); sy = h / 2 - (int16_t)(((int64_t)y * 350) / z);
        return true;
    }
};

RuntimeExactEngine engine;
MorphismStack worldStack;
TFT_eSPI tft;

int main() {
    tft.init(); tft.setRotation(1); tft.enable_framebuffer(true);
    for (int frame = 0; frame < 10; frame++) {
        tft.fillScreen(TFT_BLACK);
        worldStack.reset();
        int rX = worldStack.pushRotation(frame * 5);
        int alpha = (frame * 1024) / 10;
        AlgebraicVec3 p1 = AlgebraicVec3::fromRational(50*SCALE, 0, 0);
        AlgebraicVec3 p2 = AlgebraicVec3::fromRational(0, 50*SCALE, 0);
        AlgebraicVec3 morphed;
        engine.morph(morphed, p1, p2, alpha);
        engine.applyRotation(morphed, rX, 0);
        int32_t x, y, z;
        engine.collapse(morphed, worldStack, x, y, z);
        printf("Frame %d: alpha=%d, Collapse=[%d, %d, %d]\n", frame, alpha, x, y, z);
        char buf[64]; sprintf(buf, "frame_%03d.ppm", frame);
        tft.save_ppm(buf);
    }
    return 0;
}
