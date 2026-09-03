#include "graphics/elementalvisual.h"
#include <Arduino.h>
#include "graphics/display.h"
#include "data/game.h"

namespace { ElementalVisualEffect effect; uint32_t lastRedraw = 0; }

uint8_t getElementalVisualIntensity(uint32_t elapsed)
{
    if (elapsed >= ELEMENTAL_VISUAL_DURATION_MS) return 0;
    if (elapsed < ELEMENTAL_VISUAL_FADE_IN_MS)
        return static_cast<uint8_t>((elapsed * 255UL) / ELEMENTAL_VISUAL_FADE_IN_MS);
    return static_cast<uint8_t>(255UL - ((elapsed - ELEMENTAL_VISUAL_FADE_IN_MS) * 255UL /
        (ELEMENTAL_VISUAL_DURATION_MS - ELEMENTAL_VISUAL_FADE_IN_MS)));
}
uint16_t blendRGB565(uint16_t bg, uint16_t fg, uint8_t a)
{
    const uint32_t ia = 255 - a;
    const uint16_t r = (((bg >> 11) & 31) * ia + ((fg >> 11) & 31) * a) / 255;
    const uint16_t g = (((bg >> 5) & 63) * ia + ((fg >> 5) & 63) * a) / 255;
    const uint16_t b = ((bg & 31) * ia + (fg & 31) * a) / 255;
    return (r << 11) | (g << 5) | b;
}
uint16_t getElementalVisualColor(DamageType type, uint8_t intensity)
{
    uint16_t base = 0xF800;
    if (type == DAMAGE_COLD) base = 0x5DDF;
    else if (type == DAMAGE_ELECTRIC) base = 0xFFE0;
    else if (type == DAMAGE_ACID) base = 0xAFE5;
    else if (type == DAMAGE_FIRE && intensity > 220) base = 0xFBE0;
    return base;
}
void startElementalVisualEffect(DamageType type, const ElementalVisualTile* tiles, uint8_t count)
{
    effect = ElementalVisualEffect{}; effect.active = count > 0; effect.type = type;
    effect.startTime = millis(); effect.tileCount = count > 9 ? 9 : count;
    for (uint8_t i=0;i<effect.tileCount;i++) effect.tiles[i] = tiles[i];
}
void updateElementalVisualEffect()
{
    if (!effect.active) return;
    const uint32_t now = millis();
    if (getElementalVisualIntensity(now - effect.startTime) == 0) effect.active = false;
    if (now - lastRedraw < 45) return;
    lastRedraw = now;
    for (uint8_t i=0;i<effect.tileCount;i++) markTileDirty(effect.tiles[i].x,effect.tiles[i].y);
}
bool drawElementalVisualOverlay(int x,int y)
{
    if (!effect.active) return false;
    for(uint8_t i=0;i<effect.tileCount;i++) if(effect.tiles[i].x==x && effect.tiles[i].y==y) {
        const uint8_t a=getElementalVisualIntensity(millis()-effect.startTime);
        const uint16_t c=getElementalVisualColor(effect.type,a);
        // Sparse stipple keeps the original floor/trap graphics readable.
        for(uint8_t py=2;py<15;py+=3) for(uint8_t px=2;px<15;px+=3)
            if (((px+py+millis()/90)%5) < (a*5/256)) tft.drawPixel(x*16+px,y*16+py,c);
        return true;
    }
    return false;
}
