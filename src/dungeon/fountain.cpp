#include "dungeon/fountain.h"

#include "characters/characters.h"
#include "dungeon/dungeon.h"
#include "dungeon/roomgen.h"

namespace
{
bool isEntryClearanceTile(const DungeonRoom& room, int x, int y)
{
    for (uint8_t entry = ENTRY_START; entry <= ENTRY_WEST; entry++)
    {
        uint8_t entryX = 0;
        uint8_t entryY = 0;
        if (!getRoomEntryPosition(
                room, static_cast<RoomEntry>(entry), entryX, entryY))
        {
            continue;
        }

        if (x >= static_cast<int>(entryX) - 1 &&
            x <= static_cast<int>(entryX) + 1 &&
            y >= static_cast<int>(entryY) - 1 &&
            y <= static_cast<int>(entryY) + 1)
        {
            return true;
        }
    }

    return false;
}

bool canPlaceHealingFountainAt(const DungeonRoom& room, int x, int y)
{
    if (x < 1 || y < 1 ||
        x + HEALING_FOUNTAIN_WIDTH >= ROOM_SIZE ||
        y + HEALING_FOUNTAIN_HEIGHT >= ROOM_SIZE)
    {
        return false;
    }

    for (uint8_t offsetY = 0; offsetY < HEALING_FOUNTAIN_HEIGHT; offsetY++)
    {
        for (uint8_t offsetX = 0; offsetX < HEALING_FOUNTAIN_WIDTH; offsetX++)
        {
            const int tileX = x + offsetX;
            const int tileY = y + offsetY;
            if (room.map.tiles[tileY][tileX] != TILE_FLOOR ||
                isEntryClearanceTile(room, tileX, tileY))
            {
                return false;
            }
        }
    }

    return true;
}
}

bool placeHealingFountain(DungeonRoom& room)
{
    room.fountain = HealingFountain{};

    // Search from the rear of the room first, retaining the centered entrance
    // and its eastward exit as open circulation space when possible.
    for (int y = ROOM_SIZE - HEALING_FOUNTAIN_HEIGHT - 1; y >= 1; y--)
    {
        for (int x = ROOM_SIZE - HEALING_FOUNTAIN_WIDTH - 1; x >= 1; x--)
        {
            if (!canPlaceHealingFountainAt(room, x, y))
                continue;

            room.fountain.x = static_cast<int8_t>(x);
            room.fountain.y = static_cast<int8_t>(y);
            room.fountain.active = true;
            room.fountain.used = false;
            for (uint8_t offsetY = 0; offsetY < HEALING_FOUNTAIN_HEIGHT; offsetY++)
            {
                for (uint8_t offsetX = 0; offsetX < HEALING_FOUNTAIN_WIDTH; offsetX++)
                {
                    room.map.tiles[y + offsetY][x + offsetX] = TILE_FOUNTAIN;
                }
            }
            return true;
        }
    }

    return false;
}

bool isHealingFountainTile(const DungeonRoom& room, int x, int y)
{
    return getHealingFountainAt(room, x, y) != nullptr;
}

HealingFountain* getHealingFountainAt(DungeonRoom& room, int x, int y)
{
    const HealingFountain& fountain = room.fountain;
    if (!fountain.active || x < fountain.x || y < fountain.y ||
        x >= fountain.x + HEALING_FOUNTAIN_WIDTH ||
        y >= fountain.y + HEALING_FOUNTAIN_HEIGHT)
    {
        return nullptr;
    }

    return &room.fountain;
}

const HealingFountain* getHealingFountainAt(
    const DungeonRoom& room, int x, int y)
{
    const HealingFountain& fountain = room.fountain;
    if (!fountain.active || x < fountain.x || y < fountain.y ||
        x >= fountain.x + HEALING_FOUNTAIN_WIDTH ||
        y >= fountain.y + HEALING_FOUNTAIN_HEIGHT)
    {
        return nullptr;
    }

    return &fountain;
}

bool drinkFromHealingFountain(HealingFountain& fountain, Character& character)
{
    if (!fountain.active || fountain.used)
        return false;

    healCharacter(character, character.health.maxHP);
    if (character.magic.maxMP > 0)
        restoreMana(character, character.magic.maxMP);

    fountain.used = true;
    return true;
}
