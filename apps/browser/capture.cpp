#include "Adafruit_GFX.h"
#include "graphics/sprites.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Same RGB565 constants as Adafruit_ST77xx.h. No hardware driver is linked.
constexpr uint16_t ST77XX_BLACK = 0x0000, ST77XX_WHITE = 0xFFFF;
enum class SoundEffect { TITLE_THEME };
void playSound(SoundEffect) {} // Capture has no audio; documented limitation.
const char* getGameMessage() { return ""; } // Original cold-start message.
unsigned long clockMs = 0;
unsigned long millis() { return clockMs; }
Adafruit_GFX* destination = nullptr;
#define tft (*destination)
#include "title_calls.inc"
#undef tft

// Reference path: original GFX generic primitives, only drawPixel overridden.
class Reference : public Adafruit_GFX {
public:
    std::array<uint16_t, 240 * 240> pixels{};
    Reference() : Adafruit_GFX(240, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x >= 0 && y >= 0 && x < 240 && y < 240) pixels[y * 240 + x] = color;
    }
};

void raw(const char* path, const uint16_t* pixels) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open capture output");
    for (size_t i = 0; i < 240 * 240; ++i) {
        out.put(static_cast<char>(pixels[i] & 255));
        out.put(static_cast<char>(pixels[i] >> 8));
    }
    if (!out) throw std::runtime_error("Cannot write capture output");
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("Usage: pfme_display_capture reference.rgb565 title.rgb565 animated.rgb565");
        Reference reference;
        destination = &reference;
        drawStartScreen();
        raw(argv[1], reference.pixels.data());

        GFXcanvas16 canvas(240, 240);
        if (!canvas.getBuffer()) throw std::runtime_error("Framebuffer allocation failed");
        destination = &canvas;
        drawStartScreen();
        size_t differences = 0;
        for (size_t i = 0; i < reference.pixels.size(); ++i)
            differences += reference.pixels[i] != canvas.getBuffer()[i];
        if (differences) throw std::runtime_error("Title reference differs from framebuffer");
        raw(argv[2], canvas.getBuffer());

        // Exercise the original animation sequence with its actual 800 ms clock.
        clockMs = 800;
        drawStartAnimation();
        clockMs = 1600;
        drawStartAnimation();
        raw(argv[3], canvas.getBuffer());
        destination = &reference;
        // Erasing the previous frame is required, not just overpainting it.
        reference.fillRect(64, 95, 64, 64, ST77XX_BLACK);
        drawSpriteTransparent64(64, 95, FIGHTERWALK2);
        for (size_t i = 0; i < reference.pixels.size(); ++i)
            if (reference.pixels[i] != canvas.getBuffer()[i])
                throw std::runtime_error("Animation reference differs from framebuffer");
        std::cout << "240x240 RGB565: title and animation match original GFX reference; 0 differing pixels\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
