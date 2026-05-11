#include "6/SymbolicVM.h"
#include "6/Demos.h"
#include <vector>
#include <cmath>

SymbolicVM engine;
MorphismStack worldStack;
TFT_eSPI tft;

int main() {
    tft.init(); tft.setRotation(1); tft.enable_framebuffer(true);

    printf("Verifying Refactored Demo Suite...\n");

    printf("Testing Solar System...\n");
    runSolarSystem(0);
    tft.save_ppm("demo_solar.ppm");

    printf("Testing Fractal Tree...\n");
    runFractalTree(0);
    tft.save_ppm("demo_tree.ppm");

    printf("Testing Shaded Ico...\n");
    runShadedIco(0);
    tft.save_ppm("demo_ico.ppm");

    printf("Testing Morph...\n");
    runMorph(0);
    tft.save_ppm("demo_morph.ppm");

    printf("All demos rendered successfully in refactored architecture.\n");
    return 0;
}
