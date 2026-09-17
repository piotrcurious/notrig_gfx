/*
  Fixed-point 2D/3D graphics demo for ESP32 Arduino
  --------------------------------------------------
  - No floats in the math core
  - No sin/cos in the runtime path
  - 2D affine transform demo
  - 3D wireframe cube using fixed-point quaternions
  - Display backend: TFT_eSPI

  If you use a different display library, replace the drawLine/fillScreen calls.
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdlib.h>
#include <string.h>
#include <new>

TFT_eSPI tft;

using fx = int32_t;
using s64 = int64_t;
using u64 = uint64_t;

constexpr int FX_SHIFT = 16;
constexpr fx FX_ONE = (fx)1 << FX_SHIFT;

static constexpr inline fx fxFromInt(int32_t v) { return (fx)((s64)v * (1LL << FX_SHIFT)); }
static constexpr inline fx fxFromRaw(int32_t raw) { return raw; }
static constexpr inline fx fxAdd(fx a, fx b) { return a + b; }
static constexpr inline fx fxSub(fx a, fx b) { return a - b; }

static inline fx fxMul(fx a, fx b) {
  return (fx)(((s64)a * (s64)b + (1LL << (FX_SHIFT - 1))) >> FX_SHIFT);
}

static inline fx fxDiv(fx a, fx b) {
  if (b == 0) return 0; // Basic safety
  s64 numerator = (s64)a * (1LL << FX_SHIFT);
  if ((numerator ^ b) >= 0) {
    return (fx)((numerator + (b / 2)) / b);
  } else {
    return (fx)((numerator - (b / 2)) / b);
  }
}

static inline fx fxAbs(fx v) { return v < 0 ? -v : v; }

static u64 isqrt64(u64 x) {
  // Integer square root: floor(sqrt(x))
  if (x == 0) return 0;
  u64 op = x;
  u64 res = 0;

  // Use hardware bit-counting (GCC count-leading-zeros) for instant alignment
  int shift = 62 - (__builtin_clzll(op) & ~1);
  u64 one = (u64)1 << shift;

  while (one != 0) {
    if (op >= res + one) {
      op -= res + one;
      res = (res >> 1) + one;
    } else {
      res >>= 1;
    }
    one >>= 2;
  }
  return res;
}

struct Vec2 {
  fx x, y;
};

struct Vec3 {
  fx x, y, z;
};

struct Quat {
  fx w, x, y, z;
};

struct Mat2D {
  fx a, b, c, d; // 2x2 linear part
};

static inline Vec2 v2(fx x, fx y) { return {x, y}; }
static inline Vec3 v3(fx x, fx y, fx z) { return {x, y, z}; }

static inline Vec2 add2(Vec2 p, Vec2 q) { return {fxAdd(p.x, q.x), fxAdd(p.y, q.y)}; }
static inline Vec2 sub2(Vec2 p, Vec2 q) { return {fxSub(p.x, q.x), fxSub(p.y, q.y)}; }
static inline Vec2 scale2(Vec2 p, fx s) { return {fxMul(p.x, s), fxMul(p.y, s)}; }

static inline Vec3 add3(Vec3 p, Vec3 q) { return {fxAdd(p.x, q.x), fxAdd(p.y, q.y), fxAdd(p.z, q.z)}; }
static inline Vec3 sub3(Vec3 p, Vec3 q) { return {fxSub(p.x, q.x), fxSub(p.y, q.y), fxSub(p.z, q.z)}; }
static inline Vec3 scale3(Vec3 p, fx s) { return {fxMul(p.x, s), fxMul(p.y, s), fxMul(p.z, s)}; }

static inline fx dot3(Vec3 a, Vec3 b) {
  return fxAdd(fxAdd(fxMul(a.x, b.x), fxMul(a.y, b.y)), fxMul(a.z, b.z));
}

static inline Vec3 cross3(Vec3 a, Vec3 b) {
  return {
    fxSub(fxMul(a.y, b.z), fxMul(a.z, b.y)),
    fxSub(fxMul(a.z, b.x), fxMul(a.x, b.z)),
    fxSub(fxMul(a.x, b.y), fxMul(a.y, b.x))
  };
}

static inline fx len3(Vec3 v) {
  u64 s = (u64)((s64)v.x * v.x) + (u64)((s64)v.y * v.y) + (u64)((s64)v.z * v.z);
  return (fx)isqrt64(s);
}

static inline Vec3 normalize3(Vec3 v) {
  u64 s = (u64)((s64)v.x * v.x) + (u64)((s64)v.y * v.y) + (u64)((s64)v.z * v.z);
  if (s == 0) return {0, 0, 0};
  u64 root = isqrt64(s);
  if (root == 0) return {0, 0, 0};

  Vec3 r;
  s64 nx = (s64)v.x * (1LL << FX_SHIFT);
  s64 ny = (s64)v.y * (1LL << FX_SHIFT);
  s64 nz = (s64)v.z * (1LL << FX_SHIFT);

  r.x = (fx)((nx >= 0 ? (nx + (s64)root/2) : (nx - (s64)root/2)) / (s64)root);
  r.y = (fx)((ny >= 0 ? (ny + (s64)root/2) : (ny - (s64)root/2)) / (s64)root);
  r.z = (fx)((nz >= 0 ? (nz + (s64)root/2) : (nz - (s64)root/2)) / (s64)root);
  return r;
}

static inline Quat quatIdentity() {
  return {FX_ONE, 0, 0, 0};
}

static inline Quat quatMul(Quat a, Quat b) {
  return {
    fxSub(fxSub(fxSub(fxMul(a.w, b.w), fxMul(a.x, b.x)), fxMul(a.y, b.y)), fxMul(a.z, b.z)),
    fxAdd(fxAdd(fxAdd(fxMul(a.w, b.x), fxMul(a.x, b.w)), fxMul(a.y, b.z)), (fx)-fxMul(a.z, b.y)),
    fxAdd(fxAdd(fxAdd(fxMul(a.w, b.y), (fx)-fxMul(a.x, b.z)), fxMul(a.y, b.w)), fxMul(a.z, b.x)),
    fxAdd(fxAdd(fxAdd(fxMul(a.w, b.z), fxMul(a.x, b.y)), (fx)-fxMul(a.y, b.x)), fxMul(a.z, b.w))
  };
}

static inline Quat quatNormalize(Quat q) {
  u64 n2 =
    (u64)((s64)q.w * q.w) +
    (u64)((s64)q.x * q.x) +
    (u64)((s64)q.y * q.y) +
    (u64)((s64)q.z * q.z);

  u64 root = isqrt64(n2);
  if (root == 0) return quatIdentity();

  Quat r;
  s64 nw = (s64)q.w * (1LL << FX_SHIFT);
  s64 nx = (s64)q.x * (1LL << FX_SHIFT);
  s64 ny = (s64)q.y * (1LL << FX_SHIFT);
  s64 nz = (s64)q.z * (1LL << FX_SHIFT);

  r.w = (fx)((nw >= 0 ? (nw + (s64)root/2) : (nw - (s64)root/2)) / (s64)root);
  r.x = (fx)((nx >= 0 ? (nx + (s64)root/2) : (nx - (s64)root/2)) / (s64)root);
  r.y = (fx)((ny >= 0 ? (ny + (s64)root/2) : (ny - (s64)root/2)) / (s64)root);
  r.z = (fx)((nz >= 0 ? (nz + (s64)root/2) : (nz - (s64)root/2)) / (s64)root);
  return r;
}

static inline Vec3 quatRotate(Quat q, Vec3 v) {
  // Assumes q is close to unit length; normalize periodically.
  Vec3 u = {q.x, q.y, q.z};
  Vec3 uv = cross3(u, v);
  Vec3 uuv = cross3(u, uv);

  // v' = v + 2*w*(u × v) + 2*(u × (u × v))
  // The scale factor 2*(u x (u x v)) can be written as scale3(uuv, (fx)(FX_ONE << 1))
  // because uuv is (u x (u x v)).
  return add3(
    v,
    add3(
      scale3(uv, (fx)(q.w * 2)),
      scale3(uuv, (fx)(FX_ONE * 2))
    )
  );
}

static inline Mat2D mat2Identity() {
  return {FX_ONE, 0, 0, FX_ONE};
}

static inline Mat2D mat2Mul(Mat2D a, Mat2D b) {
  // a * b
  return {
    fxAdd(fxMul(a.a, b.a), fxMul(a.b, b.c)),
    fxAdd(fxMul(a.a, b.b), fxMul(a.b, b.d)),
    fxAdd(fxMul(a.c, b.a), fxMul(a.d, b.c)),
    fxAdd(fxMul(a.c, b.b), fxMul(a.d, b.d))
  };
}

static inline Vec2 apply2(Mat2D m, Vec2 p) {
  return {
    fxAdd(fxMul(m.a, p.x), fxMul(m.b, p.y)),
    fxAdd(fxMul(m.c, p.x), fxMul(m.d, p.y))
  };
}

static inline Mat2D orthonormalize2(Mat2D m) {
  // Fix first column to unit length, second column = perpendicular.
  u64 s =
    (u64)((s64)m.a * m.a) +
    (u64)((s64)m.c * m.c);
  u64 l = isqrt64(s);
  if (l == 0) return mat2Identity();

  s64 inv = ((s64)FX_ONE * (s64)FX_ONE) / (s64)l;
  fx a = (fx)(((s64)m.a * inv) >> FX_SHIFT);
  fx c = (fx)(((s64)m.c * inv) >> FX_SHIFT);
  return {a, (fx)-c, c, a};
}

static inline Mat2D rot2FromT(fx t) {
  // R = 1/(1+t^2) * [[1-t^2, -2t], [2t, 1-t^2]]
  fx t2 = fxMul(t, t);
  fx den = fxAdd(FX_ONE, t2);
  fx a = fxDiv(fxSub(FX_ONE, t2), den);
  fx b = fxDiv((fx)(-((s64)t * 2)), den);
  fx c = fxDiv((fx)(((s64)t) * 2), den);
  fx d = a;
  return {a, b, c, d};
}

static inline Quat quatFromAxisT(Vec3 axisUnit, fx t) {
  // axisUnit should be normalized.
  // Note: t = tan(theta/4) yields a quaternion rotating by angle theta around axisUnit,
  // since the Weierstrass half-angle substitution here calculates w = cos(2*phi) and s = sin(2*phi) where t = tan(phi).
  fx t2 = fxMul(t, t);
  fx den = fxAdd(FX_ONE, t2);
  fx w = fxDiv(fxSub(FX_ONE, t2), den);
  fx s = fxDiv((fx)(((s64)t) * 2), den);
  return {
    w,
    fxMul(s, axisUnit.x),
    fxMul(s, axisUnit.y),
    fxMul(s, axisUnit.z)
  };
}

// ------------------ Tiled Rasterizer Engine ------------------

struct Tile {
    uint16_t x0, y0, w, h;
    uint16_t *buf;
    bool dirty_curr;
    bool dirty_prev;

    Tile() : x0(0), y0(0), w(0), h(0), buf(nullptr), dirty_curr(false), dirty_prev(false) {}

    void init(uint16_t _x0, uint16_t _y0, uint16_t _w, uint16_t _h) {
        x0 = _x0; y0 = _y0; w = _w; h = _h;
        size_t n = (size_t)w * (size_t)h;
        if (buf) free(buf);
        buf = (uint16_t*) malloc(n * sizeof(uint16_t));
        dirty_curr = true;
        dirty_prev = true;
        if (buf) memset(buf, 0, n * sizeof(uint16_t));
    }

    void prepareFrame() {
        dirty_prev = dirty_curr;
        dirty_curr = false;
        if (buf) memset(buf, 0, (size_t)w * (size_t)h * sizeof(uint16_t));
    }
};

class TileManager {
public:
    uint16_t screen_w, screen_h, tile_size;
    uint16_t cols, rows;
    Tile *tiles;

    TileManager() : screen_w(0), screen_h(0), tile_size(0), cols(0), rows(0), tiles(nullptr) {}

    void init(uint16_t sw, uint16_t sh, uint16_t tsize) {
        screen_w = sw; screen_h = sh; tile_size = tsize;
        cols = (screen_w + tile_size - 1) / tile_size;
        rows = (screen_h + tile_size - 1) / tile_size;
        tiles = (Tile*) malloc(sizeof(Tile) * cols * rows);
        for (uint16_t r = 0; r < rows; ++r) {
            for (uint16_t c = 0; c < cols; ++c) {
                uint16_t x0 = c * tile_size;
                uint16_t y0 = r * tile_size;
                uint16_t tw = (x0 + tile_size <= screen_w) ? tile_size : (screen_w - x0);
                uint16_t th = (y0 + tile_size <= screen_h) ? tile_size : (screen_h - y0);
                new (&tiles[r * cols + c]) Tile();
                tiles[r * cols + c].init(x0, y0, tw, th);
            }
        }
    }

    Tile* tileAtIdx(uint16_t tx, uint16_t ty) {
        if (tx >= cols || ty >= rows) return nullptr;
        return &tiles[ty * cols + tx];
    }

    void writePixelGlobal(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || y < 0 || x >= (int)screen_w || y >= (int)screen_h) return;
        uint16_t tx = (uint16_t)(x / tile_size);
        uint16_t ty = (uint16_t)(y / tile_size);
        uint16_t lx = (uint16_t)(x - tx * tile_size);
        uint16_t ly = (uint16_t)(y - ty * tile_size);

        Tile* t = tileAtIdx(tx, ty);
        if (!t || !t->buf) return;
        t->buf[(size_t)ly * (size_t)t->w + (size_t)lx] = color;
        t->dirty_curr = true;
    }

    void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
        if ((x0 < 0 && x1 < 0) || (x0 >= (int)screen_w && x1 >= (int)screen_w) ||
            (y0 < 0 && y1 < 0) || (y0 >= (int)screen_h && y1 >= (int)screen_h)) return;

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            writePixelGlobal(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void startFrame() {
        uint32_t count = (uint32_t)cols * rows;
        for (uint32_t i = 0; i < count; ++i) tiles[i].prepareFrame();
    }

    void flush(TFT_eSPI &dev) {
        uint32_t total = (uint32_t)cols * rows;
        for (uint32_t i = 0; i < total; ++i) {
            Tile &t = tiles[i];
            if (t.dirty_curr || t.dirty_prev) {
                dev.pushImage(t.x0, t.y0, t.w, t.h, t.buf);
            }
        }
    }
};

static TileManager g_tile_manager;
constexpr uint16_t TILE_SIZE = 32;

// ------------------ Demo geometry ------------------

struct Tri {
  uint8_t a, b, c;
};

struct Edge {
  uint8_t a, b;
};

static const Vec3 cubeVerts[] = {
  { -FX_ONE, -FX_ONE, -FX_ONE },
  {  FX_ONE, -FX_ONE, -FX_ONE },
  {  FX_ONE,  FX_ONE, -FX_ONE },
  { -FX_ONE,  FX_ONE, -FX_ONE },
  { -FX_ONE, -FX_ONE,  FX_ONE },
  {  FX_ONE, -FX_ONE,  FX_ONE },
  {  FX_ONE,  FX_ONE,  FX_ONE },
  { -FX_ONE,  FX_ONE,  FX_ONE }
};

static const Edge cubeEdges[] = {
  {0,1}, {1,2}, {2,3}, {3,0},
  {4,5}, {5,6}, {6,7}, {7,4},
  {0,4}, {1,5}, {2,6}, {3,7}
};

static const Vec2 squareVerts[] = {
  { -fxFromInt(18), -fxFromInt(18) },
  {  fxFromInt(18), -fxFromInt(18) },
  {  fxFromInt(18),  fxFromInt(18) },
  { -fxFromInt(18),  fxFromInt(18) }
};

static const Edge squareEdges[] = {
  {0,1}, {1,2}, {2,3}, {3,0}
};

// ------------------ Rendering Engine ------------------

struct Camera {
  fx focal;
  fx nearZ;
};

struct Viewport {
  int32_t cx, cy;
};

static inline bool project3D(const Camera &cam, const Viewport &vp, Vec3 p, int16_t &sx, int16_t &sy) {
  if (p.z < cam.nearZ) return false;
  // Near plane clipping ensures p.z >= nearZ.
  // Assuming nearZ >= 1, we avoid division by zero.
  fx k = fxDiv(cam.focal, p.z);
  fx x = fxMul(p.x, k);
  fx y = fxMul(p.y, k);

  sx = (int16_t)(vp.cx + (x >> FX_SHIFT));
  sy = (int16_t)(vp.cy + (y >> FX_SHIFT));
  return true;
}

static inline void drawWireEdge2D(Vec2 a, Vec2 b, int ox, int oy, uint16_t color) {
  g_tile_manager.drawLine(
    ox + (a.x >> FX_SHIFT), oy + (a.y >> FX_SHIFT),
    ox + (b.x >> FX_SHIFT), oy + (b.y >> FX_SHIFT),
    color
  );
}

static inline void drawWireEdge3D(const Camera &cam, const Viewport &vp, Vec3 a, Vec3 b, uint16_t color) {
  // Simple near-plane clipping (Z = nearZ)
  if (a.z < cam.nearZ && b.z < cam.nearZ) return;

  if (a.z < cam.nearZ) {
    // a is clipped, find intersection with near plane
    fx t = fxDiv(fxSub(cam.nearZ, a.z), fxSub(b.z, a.z));
    a.x = fxAdd(a.x, fxMul(t, fxSub(b.x, a.x)));
    a.y = fxAdd(a.y, fxMul(t, fxSub(b.y, a.y)));
    a.z = cam.nearZ;
  } else if (b.z < cam.nearZ) {
    // b is clipped, find intersection with near plane
    fx t = fxDiv(fxSub(cam.nearZ, b.z), fxSub(a.z, b.z));
    b.x = fxAdd(b.x, fxMul(t, fxSub(a.x, b.x)));
    b.y = fxAdd(b.y, fxMul(t, fxSub(a.y, b.y)));
    b.z = cam.nearZ;
  }

  int16_t x0, y0, x1, y1;
  if (!project3D(cam, vp, a, x0, y0)) return;
  if (!project3D(cam, vp, b, x1, y1)) return;
  g_tile_manager.drawLine(x0, y0, x1, y1, color);
}

// ------------------ State ------------------

Mat2D gSquareRot;
Quat gCubeRot;

constexpr fx CUBE_SCALE = (fx)(FX_ONE + (FX_ONE / 2)); // 1.5
constexpr fx CUBE_Z = (fxFromInt(8));
Camera gCam = { fxFromInt(120), fxFromInt(1) };
Viewport gVp;

static inline Vec3 cubeTransform(Vec3 p) {
  p = scale3(p, CUBE_SCALE);
  p = quatRotate(gCubeRot, p);
  p.z = fxAdd(p.z, CUBE_Z);
  return p;
}

static inline void drawAxes3D(const Camera &cam, const Viewport &vp, uint16_t colorX, uint16_t colorY, uint16_t colorZ) {
  Vec3 o = {0, 0, CUBE_Z};
  drawWireEdge3D(cam, vp, o, add3(o, quatRotate(gCubeRot, v3(fxFromInt(2), 0, 0))), colorX);
  drawWireEdge3D(cam, vp, o, add3(o, quatRotate(gCubeRot, v3(0, fxFromInt(2), 0))), colorY);
  drawWireEdge3D(cam, vp, o, add3(o, quatRotate(gCubeRot, v3(0, 0, fxFromInt(2)))), colorZ);
}

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  g_tile_manager.init(tft.width(), tft.height(), TILE_SIZE);

  gVp.cx = tft.width() / 2;
  gVp.cy = tft.height() / 2;

  gSquareRot = mat2Identity();
  gCubeRot = quatIdentity();

  // Small rational increments: t = tan(dtheta/2) in fixed-point.
  // These are intentionally tiny to keep the motion smooth and stable.
  // No trig calls are used anywhere in the update loop.
}

static inline void draw2DSquareDemo() {
  // Update the 2D rotation matrix by a tiny exact rational step.
  const fx stepT = (FX_ONE / 96); // small tangent-half-angle step
  Mat2D step = rot2FromT(stepT);
  gSquareRot = mat2Mul(step, gSquareRot);
  gSquareRot = orthonormalize2(gSquareRot);

  int ox = 62;
  int oy = 62;
  uint16_t color = TFT_CYAN;

  Vec2 p[4];
  for (int i = 0; i < 4; ++i) {
    p[i] = apply2(gSquareRot, squareVerts[i]);
  }

  for (int i = 0; i < 4; ++i) {
    int j = (i + 1) & 3;
    drawWireEdge2D(p[i], p[j], ox, oy, color);
  }

  // Crosshair
  g_tile_manager.drawLine(ox - 28, oy, ox + 28, oy, TFT_DARKGREY);
  g_tile_manager.drawLine(ox, oy - 28, ox, oy + 28, TFT_DARKGREY);
}

static inline void updateCubeRotation() {
  // Compose two small principal-axis rotations.
  const fx stepTx = (FX_ONE / 140);
  const fx stepTy = (FX_ONE / 110);

  Quat dqX = quatFromAxisT(v3(FX_ONE, 0, 0), stepTx);
  Quat dqY = quatFromAxisT(v3(0, FX_ONE, 0), stepTy);

  gCubeRot = quatMul(dqY, quatMul(dqX, gCubeRot));
  gCubeRot = quatNormalize(gCubeRot);
}

static inline void drawCubeDemo() {
  updateCubeRotation();

  Vec3 tv[8];
  for (int i = 0; i < 8; ++i) {
    tv[i] = cubeTransform(cubeVerts[i]);
  }

  for (unsigned i = 0; i < sizeof(cubeEdges) / sizeof(cubeEdges[0]); ++i) {
    const Edge &e = cubeEdges[i];
    drawWireEdge3D(gCam, gVp, tv[e.a], tv[e.b], TFT_WHITE);
  }

  drawAxes3D(gCam, gVp, TFT_RED, TFT_GREEN, TFT_BLUE);
}

void loop() {
  uint32_t tStart = millis();

  g_tile_manager.startFrame();

  draw2DSquareDemo();
  drawCubeDemo();

  g_tile_manager.flush(tft);

  uint32_t tElapsed = millis() - tStart;
  if (tElapsed < 16) {
    delay(16 - tElapsed);
  }
}
