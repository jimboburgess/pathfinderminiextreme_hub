#ifndef PATHFINDERMINIEXTREME_025_ELEMENTALVISUAL_H
#define PATHFINDERMINIEXTREME_025_ELEMENTALVISUAL_H

#include <stdint.h>
#include "characters/abilities.h"

constexpr uint16_t ELEMENTAL_VISUAL_FADE_IN_MS = 750;
constexpr uint16_t ELEMENTAL_VISUAL_DURATION_MS = 3000;

struct ElementalVisualTile { int8_t x = -1; int8_t y = -1; };
struct ElementalVisualEffect {
    bool active = false;
    DamageType type = DAMAGE_NONE;
    uint32_t startTime = 0;
    ElementalVisualTile tiles[9]{};
    uint8_t tileCount = 0;
};

uint8_t getElementalVisualIntensity(uint32_t elapsedMs);
uint16_t getElementalVisualColor(DamageType type, uint8_t intensity);
uint16_t blendRGB565(uint16_t background, uint16_t overlay, uint8_t intensity);
void startElementalVisualEffect(DamageType type, const ElementalVisualTile* tiles, uint8_t count);
void updateElementalVisualEffect();
bool drawElementalVisualOverlay(int tileX, int tileY);

#endif
