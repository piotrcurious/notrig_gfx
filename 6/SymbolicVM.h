#ifndef SYMBOLIC_VM_H
#define SYMBOLIC_VM_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define SCALE 1024
#define MAX_LAYERS 16
#define MAX_TERMS_PER_VEC 16
#define MAX_DEPTH 6

// --- Fixed-Point Math ---
static const int16_t SIN_TABLE[91] = {
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

static inline int32_t fx_sin(int32_t angle_q4) {
    // Input is angle in degrees * 16 (Q4 fixed point)
    int32_t angle = angle_q4 >> 4;
    int32_t fract = angle_q4 & 0xF;

    auto get_sin = [](int32_t a) -> int32_t {
        a %= 360; if (a < 0) a += 360;
        if (a <= 90) return SIN_TABLE[a];
        if (a <= 180) return SIN_TABLE[180 - a];
        if (a <= 270) return -SIN_TABLE[a - 180];
        return -SIN_TABLE[360 - a];
    };

    int32_t s1 = get_sin(angle);
    int32_t s2 = get_sin(angle + 1);
    return s1 + ((s2 - s1) * fract >> 4);
}
static inline int32_t fx_cos(int32_t angle_q4) { return fx_sin(angle_q4 + (90 << 4)); }

// --- VM Data Structures ---
struct SymbolicTerm {
    int32_t v[3];
    int8_t layers[MAX_DEPTH];
    int8_t axes[MAX_DEPTH]; // 0=X, 1=Y, 2=Z, 3=SCALE
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
    bool is_normal = false;

    AlgebraicVec3() : num_terms(0), is_normal(false) {}
    static AlgebraicVec3 fromRational(int32_t x, int32_t y, int32_t z, bool normal = false) {
        AlgebraicVec3 res; res.num_terms = 1; res.is_normal = normal;
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
        val1[current_layers] = fx_cos(angleDegrees << 4); val2[current_layers] = fx_sin(angleDegrees << 4);
        return current_layers++;
    }
    int pushRotationQ4(int32_t angleQ4) {
        if (current_layers >= MAX_LAYERS) return -1;
        val1[current_layers] = fx_cos(angleQ4); val2[current_layers] = fx_sin(angleQ4);
        return current_layers++;
    }
    int pushScale(int32_t scaleFactor) {
        if (current_layers >= MAX_LAYERS) return -1;
        val1[current_layers] = scaleFactor; val2[current_layers] = 0;
        return current_layers++;
    }
    void reset() { current_layers = 0; }
};

class SymbolicVM {
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
    void applyScale(AlgebraicVec3 &v, int layer) {
        if (v.is_normal || layer < 0 || layer >= MAX_LAYERS) return;
        for (int i = 0; i < v.num_terms; i++) {
            if (v.terms[i].depth < MAX_DEPTH) {
                v.terms[i].layers[v.terms[i].depth] = (int8_t)layer;
                v.terms[i].axes[v.terms[i].depth] = 3;
                v.terms[i].depth++;
            }
        }
        v.simplify();
    }
    void add(AlgebraicVec3 &out, const AlgebraicVec3 &a, const AlgebraicVec3 &b) {
        AlgebraicVec3 res;
        for (int i = 0; i < a.num_terms && res.num_terms < MAX_TERMS_PER_VEC; i++) res.terms[res.num_terms++] = a.terms[i];
        for (int i = 0; i < b.num_terms && res.num_terms < MAX_TERMS_PER_VEC; i++) res.terms[res.num_terms++] = b.terms[i];
        res.simplify();
        out = res;
    }
    void morph(AlgebraicVec3 &out, const AlgebraicVec3 &a, const AlgebraicVec3 &b, int32_t alpha) {
        AlgebraicVec3 res;
        int32_t inv_alpha = 1024 - alpha;
        for (int i = 0; i < a.num_terms && res.num_terms < MAX_TERMS_PER_VEC; i++) {
            SymbolicTerm t = a.terms[i];
            t.v[0] = (int32_t)(((int64_t)t.v[0] * inv_alpha) >> 10);
            t.v[1] = (int32_t)(((int64_t)t.v[1] * inv_alpha) >> 10);
            t.v[2] = (int32_t)(((int64_t)t.v[2] * inv_alpha) >> 10);
            res.terms[res.num_terms++] = t;
        }
        for (int i = 0; i < b.num_terms && res.num_terms < MAX_TERMS_PER_VEC; i++) {
            SymbolicTerm t = b.terms[i];
            t.v[0] = (int32_t)(((int64_t)t.v[0] * alpha) >> 10);
            t.v[1] = (int32_t)(((int64_t)t.v[1] * alpha) >> 10);
            t.v[2] = (int32_t)(((int64_t)t.v[2] * alpha) >> 10);
            res.terms[res.num_terms++] = t;
        }
        res.simplify();
        out = res;
    }
    void collapse(const AlgebraicVec3 &v, const MorphismStack &stack, int32_t &ox, int32_t &oy, int32_t &oz) {
        ox = oy = oz = 0;
        for (int i = 0; i < v.num_terms; i++) {
            int64_t cx = v.terms[i].v[0], cy = v.terms[i].v[1], cz = v.terms[i].v[2];
            for (int d = 0; d < v.terms[i].depth; d++) {
                int l = v.terms[i].layers[d]; int ax = v.terms[i].axes[d];
                int64_t v1 = stack.val1[l]; int64_t v2 = stack.val2[l];
                int64_t nx, ny, nz;
                if (ax == 3) {
                    int64_t px = cx * v1, py = cy * v1, pz = cz * v1;
                    nx = (px + (px >= 0 ? 512 : -512)) >> 10;
                    ny = (py + (py >= 0 ? 512 : -512)) >> 10;
                    nz = (pz + (pz >= 0 ? 512 : -512)) >> 10;
                } else if (ax == 2) {
                    int64_t rx = cx * v1 - cy * v2, ry = cx * v2 + cy * v1;
                    nx = (rx + (rx >= 0 ? 512 : -512)) >> 10;
                    ny = (ry + (ry >= 0 ? 512 : -512)) >> 10;
                    nz = cz;
                } else if (ax == 0) {
                    int64_t ry = cy * v1 - cz * v2, rz = cy * v2 + cz * v1;
                    nx = cx;
                    ny = (ry + (ry >= 0 ? 512 : -512)) >> 10;
                    nz = (rz + (rz >= 0 ? 512 : -512)) >> 10;
                } else {
                    int64_t rx = cx * v1 + cz * v2, rz = -cx * v2 + cz * v1;
                    nx = (rx + (rx >= 0 ? 512 : -512)) >> 10;
                    ny = cy;
                    nz = (rz + (rz >= 0 ? 512 : -512)) >> 10;
                }
                cx = nx; cy = ny; cz = nz;
            }
            ox += (int32_t)cx; oy += (int32_t)cy; oz += (int32_t)cz;
        }
    }
    int32_t getLighting(const AlgebraicVec3 &normal, const MorphismStack &stack, int32_t lx, int32_t ly, int32_t lz) {
        int32_t nx, ny, nz; collapse(normal, stack, nx, ny, nz);
        int64_t dot = (int64_t)nx * lx + (int64_t)ny * ly + (int64_t)nz * lz;
        int32_t intensity = (int32_t)(dot >> 10);
        return intensity > 0 ? intensity : 0;
    }
    bool project(int32_t x, int32_t y, int32_t z, int16_t &sx, int16_t &sy, int w, int h) {
        z += (450 * SCALE); if (z < 20 * SCALE) return false;
        sx = w / 2 + (int16_t)(((int64_t)x * 350) / z); sy = h / 2 - (int16_t)(((int64_t)y * 350) / z);
        return true;
    }

    bool project(const AlgebraicVec3 &v, const MorphismStack &stack, int16_t &sx, int16_t &sy, int w, int h) {
        int32_t x, y, z;
        collapse(v, stack, x, y, z);
        return project(x, y, z, sx, sy, w, h);
    }

    void drawClippedLine(TFT_eSPI &tft, const AlgebraicVec3 &v1, const AlgebraicVec3 &v2, const MorphismStack &stack, uint16_t color) {
        int32_t x1, y1, z1, x2, y2, z2;
        collapse(v1, stack, x1, y1, z1);
        collapse(v2, stack, x2, y2, z2);

        int32_t nz1 = z1 + 450*SCALE, nz2 = z2 + 450*SCALE;
        int32_t nearZ = 20 * SCALE;
        if (nz1 < nearZ && nz2 < nearZ) return;

        if (nz1 < nearZ) {
            int64_t t = ((int64_t)(nearZ - nz1) << 10) / (nz2 - nz1);
            x1 = x1 + (int32_t)(((int64_t)(x2 - x1) * t) >> 10);
            y1 = y1 + (int32_t)(((int64_t)(y2 - y1) * t) >> 10);
            z1 = nearZ - 450*SCALE;
        } else if (nz2 < nearZ) {
            int64_t t = ((int64_t)(nearZ - nz2) << 10) / (nz1 - nz2);
            x2 = x2 + (int32_t)(((int64_t)(x1 - x2) * t) >> 10);
            y2 = y2 + (int32_t)(((int64_t)(y1 - y2) * t) >> 10);
            z2 = nearZ - 450*SCALE;
        }

        int16_t sx1, sy1, sx2, sy2;
        if (project(x1, y1, z1, sx1, sy1, tft.width(), tft.height()) &&
            project(x2, y2, z2, sx2, sy2, tft.width(), tft.height())) {
            tft.drawLine(sx1, sy1, sx2, sy2, color);
        }
    }
};

#endif
