#ifndef DEMOS_H
#define DEMOS_H

#include "SymbolicVM.h"
#include <TFT_eSPI.h>

extern SymbolicVM engine;
extern MorphismStack worldStack;
extern TFT_eSPI tft;

// 1. Solar System Demo
void runSolarSystem(int frame) {
    worldStack.reset();
    int lE = worldStack.pushRotation(frame % 360);
    int lM = worldStack.pushRotation((frame * 4) % 360);
    int lS = worldStack.pushRotation((frame * 12) % 360);

    AlgebraicVec3 sun = AlgebraicVec3::fromRational(0, 0, 0);
    AlgebraicVec3 earth = AlgebraicVec3::fromRational(100 * SCALE, 0, 0);
    engine.applyRotation(earth, lE, 2);

    AlgebraicVec3 moonRel = AlgebraicVec3::fromRational(30 * SCALE, 0, 0);
    engine.applyRotation(moonRel, lM, 2); engine.applyRotation(moonRel, lE, 2);
    AlgebraicVec3 moon; engine.add(moon, earth, moonRel);

    AlgebraicVec3 satRel = AlgebraicVec3::fromRational(0, 12 * SCALE, 0);
    engine.applyRotation(satRel, lS, 0); engine.applyRotation(satRel, lM, 2); engine.applyRotation(satRel, lE, 2);
    AlgebraicVec3 sat; engine.add(sat, moon, satRel);

    auto draw = [&](AlgebraicVec3 &p, uint16_t color, int size) {
        int16_t sx, sy;
        if (engine.project(p, worldStack, sx, sy, tft.width(), tft.height())) tft.fillCircle(sx, sy, size, color);
    };
    draw(sun, TFT_YELLOW, 8); draw(earth, TFT_BLUE, 4); draw(moon, TFT_WHITE, 2); draw(sat, TFT_RED, 1);
}

// 5. Symbolic Fractal Cube Demo (Menger inspired)
void drawMenger(AlgebraicVec3 center, int size, int depth, int rL) {
    if (depth == 0) {
        // Draw a simple symbolic "dot" or small cube frame
        int16_t sx, sy;
        if (engine.project(center, worldStack, sx, sy, tft.width(), tft.height())) {
            tft.fillCircle(sx, sy, 1, TFT_GOLD);
        }
        return;
    }

    int nextSize = size / 3;
    int s = size * SCALE;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                if (abs(x) + abs(y) + abs(z) > 1) { // Menger-like pattern
                    AlgebraicVec3 nextPos = AlgebraicVec3::fromRational(x * nextSize * SCALE, y * nextSize * SCALE, z * nextSize * SCALE);
                    engine.applyRotation(nextPos, rL, 2);
                    engine.add(nextPos, nextPos, center);
                    drawMenger(nextPos, nextSize, depth - 1, rL);
                }
            }
        }
    }
}

void runMenger(int frame) {
    worldStack.reset();
    int rL = worldStack.pushRotation(frame * 2);
    AlgebraicVec3 root = AlgebraicVec3::fromRational(0, 0, 0);
    drawMenger(root, 60, 2, rL);
}

// 2. Recursive Tree Demo
void drawRecursiveTree(AlgebraicVec3 base, int height, int lL, int lR, int depth) {
    if (depth == 0) return;
    AlgebraicVec3 end = AlgebraicVec3::fromRational(0, height * SCALE, 0);
    engine.add(end, end, base);

    engine.drawClippedLine(tft, base, end, worldStack, depth == 1 ? TFT_GREEN : TFT_BROWN);

    AlgebraicVec3 left = AlgebraicVec3::fromRational(0, (height*3/4)*SCALE, 0);
    engine.applyRotation(left, lL, 2); engine.add(left, left, end);
    AlgebraicVec3 right = AlgebraicVec3::fromRational(0, (height*3/4)*SCALE, 0);
    engine.applyRotation(right, lR, 2); engine.add(right, right, end);
    drawRecursiveTree(left, height*3/4, lL, lR, depth-1);
    drawRecursiveTree(right, height*3/4, lL, lR, depth-1);
}

void runFractalTree(int frame) {
    worldStack.reset();
    int lL = worldStack.pushRotationQ4((25 << 4) + (fx_sin(frame << 4) >> 6));
    int lR = worldStack.pushRotationQ4((-25 << 4) + (fx_cos(frame << 4) >> 6));
    AlgebraicVec3 root = AlgebraicVec3::fromRational(0, -80*SCALE, 0);
    drawRecursiveTree(root, 50, lL, lR, 5);
}

// 3. Shaded Icosahedron Demo
void runShadedIco(int frame) {
    worldStack.reset();
    int rX = worldStack.pushRotation(frame % 360);
    int rY = worldStack.pushRotation((frame * 2) % 360);
    static const int32_t icoV[12][3] = {{-10,16,0},{10,16,0},{-10,-16,0},{10,-16,0},{0,-10,16},{0,10,16},{0,-10,-16},{0,10,-16},{16,0,-10},{16,0,10},{-16,0,-10},{-16,0,10}};
    static const int icoF[20][3] = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    for (int i=0; i<20; i++) {
        AlgebraicVec3 v[3], normal; int32_t nx=0, ny=0, nz=0;
        for (int j=0; j<3; j++) {
            v[j] = AlgebraicVec3::fromRational(icoV[icoF[i][j]][0]*4*SCALE, icoV[icoF[i][j]][1]*4*SCALE, icoV[icoF[i][j]][2]*4*SCALE);
            nx += icoV[icoF[i][j]][0]; ny += icoV[icoF[i][j]][1]; nz += icoV[icoF[i][j]][2];
            engine.applyRotation(v[j], rX, 0); engine.applyRotation(v[j], rY, 1);
        }
        normal = AlgebraicVec3::fromRational(nx*SCALE, ny*SCALE, nz*SCALE, true);
        engine.applyRotation(normal, rX, 0); engine.applyRotation(normal, rY, 1);
        int32_t intensity = engine.getLighting(normal, worldStack, 0, 0, -SCALE);
        if (intensity > 0) {
            int16_t sx[3], sy[3]; bool ok=true;
            for(int j=0; j<3; j++) if(!engine.project(v[j], worldStack, sx[j], sy[j], tft.width(), tft.height())) ok=false;
            if(ok) {
                uint8_t c = (intensity > 1023) ? 255 : (intensity >> 2);
                uint16_t color = tft.color565(c/2, c/2, c);
                for(int j=0; j<3; j++) tft.drawLine(sx[j], sy[j], sx[(j+1)%3], sy[(j+1)%3], color);
            }
        }
    }
}

// 4. Metamorphosis Demo
void runMorph(int frame) {
    worldStack.reset();
    int rX = worldStack.pushRotation(frame % 360);
    int rY = worldStack.pushRotation((frame * 2) % 360);
    int alpha = (fx_sin(frame * 2) + 1024) / 2;
    int s1 = 40*SCALE, s2 = 12*SCALE;
    AlgebraicVec3 pA[4] = {AlgebraicVec3::fromRational(-s1,-s1,0), AlgebraicVec3::fromRational(s1,-s1,0), AlgebraicVec3::fromRational(s1,s1,0), AlgebraicVec3::fromRational(-s1,s1,0)};
    AlgebraicVec3 pB[4] = {AlgebraicVec3::fromRational(0,-s2,0), AlgebraicVec3::fromRational(s2,0,0), AlgebraicVec3::fromRational(0,s2,0), AlgebraicVec3::fromRational(-s2,0,0)};
    for (int i=0; i<4; i++) {
        AlgebraicVec3 m1, m2;
        engine.morph(m1, pA[i], pB[i], alpha); engine.applyRotation(m1, rX, 0); engine.applyRotation(m1, rY, 1);
        engine.morph(m2, pA[(i+1)%4], pB[(i+1)%4], alpha); engine.applyRotation(m2, rX, 0); engine.applyRotation(m2, rY, 1);
        int16_t sx1, sy1, sx2, sy2;
        if (engine.project(m1, worldStack, sx1, sy1, tft.width(), tft.height()) && engine.project(m2, worldStack, sx2, sy2, tft.width(), tft.height()))
            tft.drawLine(sx1, sy1, sx2, sy2, TFT_CYAN);
    }
}

#endif
