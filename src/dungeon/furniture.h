#ifndef PATHFINDERMINIEXTREME_025_FURNITURE_H
#define PATHFINDERMINIEXTREME_025_FURNITURE_H

#include <stdint.h>

#include "characters/abilities.h"
#include "graphics/tiles.h"

struct Dungeon;
struct DungeonRoom;
struct Entity;

enum DungeonFurnitureType : uint8_t
{
    FURNITURE_NONE,
    FURNITURE_BARREL,
    FURNITURE_CRATE,
    FURNITURE_STATUE,
    FURNITURE_BRAZIER
};

struct DungeonFurnitureDefinition
{
    DungeonFurnitureType type;
    TileType tile;
    const char* name;
    int16_t maxHP;
    uint8_t hardness;
    uint8_t strengthDC;
    bool movable;
    bool blocksLOS;
    bool becomesRubble;
};

struct DungeonFurnitureInstance
{
    DungeonFurnitureType type = FURNITURE_NONE;
    int8_t x = -1;
    int8_t y = -1;
    int16_t hp = 0;
};

constexpr uint8_t MAX_FURNITURE_PER_ROOM = 16;
constexpr uint8_t BARREL_STRENGTH_DC = 10;
constexpr uint8_t CRATE_STRENGTH_DC = 12;
constexpr uint8_t BRAZIER_REFLEX_DC = 10;

struct DungeonFurnitureDamageResult
{
    uint16_t incomingDamage = 0;
    uint16_t hardnessPrevented = 0;
    uint16_t appliedDamage = 0;
    bool destroyed = false;
    bool becameRubble = false;
};

enum FurniturePushResult : uint8_t
{
    FURNITURE_PUSH_NOT_APPLICABLE,
    FURNITURE_PUSH_BLOCKED,
    FURNITURE_PUSH_FAILED_STRENGTH,
    FURNITURE_PUSH_SUCCEEDED
};

const DungeonFurnitureDefinition* getDungeonFurnitureDefinition(
    DungeonFurnitureType type);
DungeonFurnitureType getDungeonFurnitureTypeForTile(TileType tile);
bool isDungeonFurnitureTile(TileType tile);
bool isDungeonFurnitureBlockingLOS(TileType tile);

DungeonFurnitureInstance* getDungeonFurnitureAt(
    DungeonRoom& room, int x, int y);
const DungeonFurnitureInstance* getDungeonFurnitureAt(
    const DungeonRoom& room, int x, int y);
bool addDungeonFurniture(
    DungeonRoom& room, DungeonFurnitureType type, int x, int y);
DungeonFurnitureDamageResult damageDungeonFurniture(
    DungeonRoom& room, int x, int y, int rawDamage, DamageType damageType);

// Active-room convenience path for weapons/spells: it uses the same generic
// durability logic and makes the changed dungeon tile redraw immediately.
DungeonFurnitureDamageResult damageCurrentDungeonFurniture(
    int x, int y, int rawDamage, DamageType damageType);

// Moves only barrel/crate furniture. The caller supplies the actual Strength
// check total so gameplay can use the normal d20/stat system and tests remain
// deterministic.
FurniturePushResult tryPushDungeonFurniture(
    Dungeon& dungeon,
    Entity& pusher,
    int x,
    int y,
    int directionX,
    int directionY,
    int strengthCheckTotal);

#endif
