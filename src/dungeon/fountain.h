#ifndef PATHFINDERMINIEXTREME_025_FOUNTAIN_H
#define PATHFINDERMINIEXTREME_025_FOUNTAIN_H

#include <stdint.h>

struct DungeonRoom;
struct Character;

constexpr uint8_t HEALING_FOUNTAIN_WIDTH = 2;
constexpr uint8_t HEALING_FOUNTAIN_HEIGHT = 3;

// A room-owned multi-tile interactable. Its coordinates identify the
// upper-left tile; all footprint queries resolve to this one object.
struct HealingFountain
{
    int8_t x = -1;
    int8_t y = -1;
    bool active = false;
    bool used = false;
};

bool placeHealingFountain(DungeonRoom& room);
bool isHealingFountainTile(const DungeonRoom& room, int x, int y);
HealingFountain* getHealingFountainAt(DungeonRoom& room, int x, int y);
const HealingFountain* getHealingFountainAt(
    const DungeonRoom& room, int x, int y);

// Returns false for an absent or spent fountain. Healing itself uses the
// shared character recovery APIs, so their normal maximum clamping applies.
bool drinkFromHealingFountain(HealingFountain& fountain, Character& character);

#endif
