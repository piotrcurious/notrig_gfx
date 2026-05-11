#ifndef TFT_ESPI_H
#define TFT_ESPI_H

#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

struct DrawLineCall {
    int32_t x0, y0, x1, y1;
    uint16_t color;
};

class TFT_eSPI {
public:
    TFT_eSPI() : _width(240), _height(320) {}
    void init() {}
    void setRotation(uint8_t r) {
        if (r == 1 || r == 3) {
            _width = 320;
            _height = 240;
        } else {
            _width = 240;
            _height = 320;
        }
    }
    void fillScreen(uint16_t color) {
        fill_calls++;
        // If we want to simulate a framebuffer:
        if (use_framebuffer) {
            for (int i = 0; i < _width * _height; ++i) framebuffer[i] = color;
        }
    }
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
        lines.push_back({x0, y0, x1, y1, color});
        if (use_framebuffer) {
            // Simple Bresenham or similar for visual verification
            draw_line_fb(x0, y0, x1, y1, color);
        }
    }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
        if (!use_framebuffer) return;
        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                if (i*i + j*j <= r*r) {
                    int px = x + i, py = y + j;
                    if (px >= 0 && px < _width && py >= 0 && py < _height) {
                        framebuffer[py * _width + px] = color;
                    }
                }
            }
        }
    }
    void setTextColor(uint16_t fg, uint16_t bg) {}
    void setCursor(int16_t x, int16_t y) {}
    void print(const char* s) {}
    void print(std::string s) {}

    int32_t width() { return _width; }
    int32_t height() { return _height; }

    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    // Mock helpers
    void clear_mock_data() {
        lines.clear();
        fill_calls = 0;
    }

    void enable_framebuffer(bool enable) {
        use_framebuffer = enable;
        if (enable) {
            framebuffer.assign(_width * _height, 0);
        }
    }

    void save_ppm(const std::string& filename) {
        std::ofstream ofs(filename, std::ios::binary);
        ofs << "P6\n" << _width << " " << _height << "\n255\n";
        for (int i = 0; i < _width * _height; ++i) {
            uint16_t c = framebuffer[i];
            // RGB565 to RGB888
            uint8_t r = ((c >> 11) & 0x1F) << 3;
            uint8_t g = ((c >> 5) & 0x3F) << 2;
            uint8_t b = (c & 0x1F) << 3;
            ofs.put(r);
            ofs.put(g);
            ofs.put(b);
        }
    }

    std::vector<DrawLineCall> lines;
    int fill_calls = 0;
    int32_t _width, _height;
    bool use_framebuffer = false;
    std::vector<uint16_t> framebuffer;

private:
    void draw_line_fb(int x0, int y0, int x1, int y1, uint16_t color) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;
        while (true) {
            if (x0 >= 0 && x0 < _width && y0 >= 0 && y0 < _height) {
                framebuffer[y0 * _width + x0] = color;
            }
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
};

#endif
