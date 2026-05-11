#include "SymbolicVM.h"
#include "Demos.h"

/*
 * GALOIS FIELD VIRTUAL MACHINE - DEMO SUITE
 * Use the #define below to switch between capabilities.
 */

#define DEMO_SOLAR_SYSTEM 1
#define DEMO_FRACTAL_TREE 2
#define DEMO_SHADED_ICO    3
#define DEMO_MORPH         4
#define DEMO_MENGER        5

#define ACTIVE_DEMO DEMO_MENGER

SymbolicVM engine;
MorphismStack worldStack;
TFT_eSPI tft;

void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    static int frame = 0;
    tft.fillScreen(TFT_BLACK);

    #if ACTIVE_DEMO == DEMO_SOLAR_SYSTEM
        runSolarSystem(frame);
    #elif ACTIVE_DEMO == DEMO_FRACTAL_TREE
        runFractalTree(frame);
    #elif ACTIVE_DEMO == DEMO_SHADED_ICO
        runShadedIco(frame);
    #elif ACTIVE_DEMO == DEMO_MORPH
        runMorph(frame);
    #elif ACTIVE_DEMO == DEMO_MENGER
        runMenger(frame);
    #endif

    frame++;
    delay(20);
}
