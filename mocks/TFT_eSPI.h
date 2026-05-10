#ifndef TFT_ESPI_H
#define TFT_ESPI_H

#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>

struct DrawLineCall {
    int32_t x0, y0, x1, y1;
    uint16_t color;
};

class TFT_eSPI {
public:
    TFT_eSPI() : _width(240), _height(320) {}
    void init() {}
    void setRotation(uint8_t r) {}
    void fillScreen(uint16_t color) {
        fill_calls++;
    }
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
        lines.push_back({x0, y0, x1, y1, color});
    }
    void setTextColor(uint16_t fg, uint16_t bg) {}
    void setCursor(int16_t x, int16_t y) {}
    void print(const char* s) {}
    void print(std::string s) {}

    int32_t width() { return _width; }
    int32_t height() { return _height; }

    // Mock helpers
    void clear_mock_data() {
        lines.clear();
        fill_calls = 0;
    }

    std::vector<DrawLineCall> lines;
    int fill_calls = 0;
    int32_t _width, _height;
};

#endif
