#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>
#include <cmath>

#define SCALE 1024
#define MAX_LAYERS 16
#define MAX_TERMS_PER_VEC 8
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
    angle %= 360;
    if (angle < 0) angle += 360;
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
};

struct AlgebraicVec3 {
    SymbolicTerm terms[MAX_TERMS_PER_VEC];
    int num_terms = 0;
    AlgebraicVec3() : num_terms(0) {}
    static AlgebraicVec3 fromRational(int32_t x, int32_t y, int32_t z) {
        AlgebraicVec3 res;
        res.num_terms = 1;
        res.terms[0].v[0] = x; res.terms[0].v[1] = y; res.terms[0].v[2] = z;
        res.terms[0].depth = 0;
        return res;
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
    }
    void add(AlgebraicVec3 &out, const AlgebraicVec3 &a, const AlgebraicVec3 &b) {
        out.num_terms = 0;
        for (int i = 0; i < a.num_terms && out.num_terms < MAX_TERMS_PER_VEC; i++)
            out.terms[out.num_terms++] = a.terms[i];
        for (int i = 0; i < b.num_terms && out.num_terms < MAX_TERMS_PER_VEC; i++)
            out.terms[out.num_terms++] = b.terms[i];
    }
    void collapse(const AlgebraicVec3 &v, const MorphismStack &stack, int32_t &ox, int32_t &oy, int32_t &oz) {
        ox = oy = oz = 0;
        for (int i = 0; i < v.num_terms; i++) {
            int64_t cx = v.terms[i].v[0], cy = v.terms[i].v[1], cz = v.terms[i].v[2];
            for (int d = 0; d < v.terms[i].depth; d++) {
                int l = v.terms[i].layers[d];
                int ax = v.terms[i].axes[d];
                int64_t cV = stack.cos_val[l];
                int64_t sV = stack.sin_val[l];
                int64_t nx, ny, nz;
                if (ax == 2) { nx = (cx * cV - cy * sV) / SCALE; ny = (cx * sV + cy * cV) / SCALE; nz = cz; }
                else if (ax == 0) { nx = cx; ny = (cy * cV - cz * sV) / SCALE; nz = (cy * sV + cz * cV) / SCALE; }
                else { nx = (cx * cV + cz * sV) / SCALE; ny = cy; nz = (-cx * sV + cz * cV) / SCALE; }
                cx = nx; cy = ny; cz = nz;
            }
            ox += (int32_t)cx; oy += (int32_t)cy; oz += (int32_t)cz;
        }
    }
    bool project(const AlgebraicVec3 &v, const MorphismStack &stack, int16_t &sx, int16_t &sy, int w, int h) {
        int32_t x, y, z;
        collapse(v, stack, x, y, z);
        z += (450 * SCALE);
        if (z < 20 * SCALE) return false;
        sx = w / 2 + (int16_t)(((int64_t)x * 350) / z);
        sy = h / 2 - (int16_t)(((int64_t)y * 350) / z);
        return true;
    }
};

RuntimeExactEngine engine;
MorphismStack worldStack;
TFT_eSPI tft;

void render(int frame) {
    tft.fillScreen(TFT_BLACK);
    worldStack.reset();
    int lEarth = worldStack.pushRotation(frame % 360);
    int lMoon = worldStack.pushRotation((frame * 4) % 360);
    int lSat = worldStack.pushRotation((frame * 12) % 360);
    int lIncline = worldStack.pushRotation(23);

    AlgebraicVec3 sun = AlgebraicVec3::fromRational(0, 0, 0);
    AlgebraicVec3 earth = AlgebraicVec3::fromRational(120 * SCALE, 0, 0);
    engine.applyRotation(earth, lEarth, 2);
    engine.applyRotation(earth, lIncline, 0);

    AlgebraicVec3 moonRel = AlgebraicVec3::fromRational(40 * SCALE, 0, 0);
    engine.applyRotation(moonRel, lMoon, 2);
    engine.applyRotation(moonRel, lEarth, 2);
    engine.applyRotation(moonRel, lIncline, 0);
    AlgebraicVec3 moon; engine.add(moon, earth, moonRel);

    AlgebraicVec3 satRel = AlgebraicVec3::fromRational(0, 15 * SCALE, 0);
    engine.applyRotation(satRel, lSat, 0);
    engine.applyRotation(satRel, lMoon, 2);
    engine.applyRotation(satRel, lEarth, 2);
    engine.applyRotation(satRel, lIncline, 0);
    AlgebraicVec3 sat; engine.add(sat, moon, satRel);

    auto draw = [&](AlgebraicVec3 &p, uint16_t color, int size) {
        int16_t sx, sy;
        if (engine.project(p, worldStack, sx, sy, tft.width(), tft.height())) {
            tft.fillCircle(sx, sy, size, color);
        }
    };
    draw(sun, TFT_YELLOW, 10);
    draw(earth, TFT_BLUE, 5);
    draw(moon, TFT_WHITE, 3);
    draw(sat, TFT_RED, 1);
}

int main() {
    tft.init();
    tft.setRotation(1);
    tft.enable_framebuffer(true);
    for (int frame = 0; frame < 20; frame++) {
        render(frame * 5);
        char buf[64];
        sprintf(buf, "frame_%03d.ppm", frame);
        tft.save_ppm(buf);
    }
    printf("Solar System Demo Generated.\n");
    return 0;
}
