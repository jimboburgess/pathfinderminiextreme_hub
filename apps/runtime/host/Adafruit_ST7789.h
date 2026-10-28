#pragma once
#include <Adafruit_GFX.h>
#include <utility>
constexpr uint16_t ST77XX_BLACK=0x0000, ST77XX_WHITE=0xffff, ST77XX_RED=0xf800,
    ST77XX_GREEN=0x07e0, ST77XX_BLUE=0x001f, ST77XX_CYAN=0x07ff,
    ST77XX_MAGENTA=0xf81f, ST77XX_YELLOW=0xffe0, ST77XX_ORANGE=0xfc00;
class Adafruit_ST7789 : public GFXcanvas16 {
public:
    Adafruit_ST7789(int, int, int) : GFXcanvas16(240,240) {}
    // Move the complete native drawing context, retaining single ownership of
    // each canvas allocation. No firmware display object or RAM is affected.
    void exchangeSurface(Adafruit_ST7789& other) {
        using std::swap;
        swap(buffer,other.buffer);
        swap(_width,other._width);swap(_height,other._height);
        swap(cursor_x,other.cursor_x);swap(cursor_y,other.cursor_y);
        swap(textcolor,other.textcolor);swap(textbgcolor,other.textbgcolor);
        swap(textsize_x,other.textsize_x);swap(textsize_y,other.textsize_y);
        swap(rotation,other.rotation);swap(wrap,other.wrap);
        swap(_cp437,other._cp437);swap(gfxFont,other.gfxFont);
    }
    void init(int, int) {}
    void setSPISpeed(unsigned long) {}
};
