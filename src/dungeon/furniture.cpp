#include "dungeon/furniture.h"

#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/traps.h"
#include "graphics/display.h"

namespace
{
const DungeonFurnitureDefinition BARREL_DEFINITION = {
    FURNITURE_BARREL, TILE_BARREL, "barrel", 6, 1, BARREL_STRENGTH_DC,
    true, false, false
};
const DungeonFurnitureDefinition CRATE_DEFINITION = {
    FURNITURE_CRATE, TILE_CRATE, "crate", 10, 2, CRATE_STRENGTH_DC,
    true, false, false
};
const DungeonFurnitureDefinition STATUE_DEFINITION = {
    FURNITURE_STATUE, TILE_STATUE, "statue", 30, 5, 0,
    false, true, true
};
const DungeonFurnitureDefinition BRAZIER_DEFINITION = {
    FURNITURE_BRAZIER, TILE_BRAZIER, "brazier", 0, 0, 0,
    false, false, false
};

bool isInsideRoom(int x, int y)
{
    return x >= 0 && x < ROOM_SIZE && y >= 0 && y < ROOM_SIZE;
}

uint16_t clampPositiveDamage(int damage)
{
    if (damage <= 0)
        return 0;
    return damage > 65535 ? 65535 : static_cast<uint16_t>(damage);
}

bool isPushDestinationValid(
    const Dungeon& dungeon,
    const DungeonRoom& room,
    const Entity& pusher,
    int x,
    int y)
{
    if (!isInsideRoom(x, y) || room.map.tiles[y][x] != TILE_FLOOR ||
        getTrapAt(room, x, y) != nullptr ||
        isHealingFountainTile(room, x, y))
    {
        return false;
    }

    Entity* occupant = getEntityAt(
        dungeon.entities, dungeon.entityCount,
        static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    return occupant == nullptr || occupant == &pusher;
}
}

const DungeonFurnitureDefinition* getDungeonFurnitureDefinition(
    DungeonFurnitureType type)
{
    switch (type)
    {
        case FURNITURE_BARREL: return &BARREL_DEFINITION;
        case FURNITURE_CRATE: return &CRATE_DEFINITION;
        case FURNITURE_STATUE: return &STATUE_DEFINITION;
        case FURNITURE_BRAZIER: return &BRAZIER_DEFINITION;
        case FURNITURE_NONE:
        default: return nullptr;
    }
}

DungeonFurnitureType getDungeonFurnitureTypeForTile(TileType tile)
{
    switch (tile)
    {
        case TILE_BARREL: return FURNITURE_BARREL;
        case TILE_CRATE: return FURNITURE_CRATE;
        case TILE_STATUE: return FURNITURE_STATUE;
        case TILE_BRAZIER: return FURNITURE_BRAZIER;
        default: return FURNITURE_NONE;
    }
}

bool isDungeonFurnitureTile(TileType tile)
{
    return getDungeonFurnitureTypeForTile(tile) != FURNITURE_NONE;
}

bool isDungeonFurnitureBlockingLOS(TileType tile)
{
    const DungeonFurnitureDefinition* definition =
        getDungeonFurnitureDefinition(getDungeonFurnitureTypeForTile(tile));
    return definition != nullptr && definition->blocksLOS;
}

DungeonFurnitureInstance* getDungeonFurnitureAt(
    DungeonRoom& room, int x, int y)
{
    for (DungeonFurnitureInstance& furniture : room.furniture)
    {
        if (furniture.type != FURNITURE_NONE && furniture.x == x &&
            furniture.y == y)
        {
            return &furniture;
        }
    }
    return nullptr;
}

const DungeonFurnitureInstance* getDungeonFurnitureAt(
    const DungeonRoom& room, int x, int y)
{
    for (const DungeonFurnitureInstance& furniture : room.furniture)
    {
        if (furniture.type != FURNITURE_NONE && furniture.x == x &&
            furniture.y == y)
        {
            return &furniture;
        }
    }
    return nullptr;
}

bool addDungeonFurniture(
    DungeonRoom& room, DungeonFurnitureType type, int x, int y)
{
    const DungeonFurnitureDefinition* definition =
        getDungeonFurnitureDefinition(type);
    if (definition == nullptr || !isInsideRoom(x, y) ||
        room.map.tiles[y][x] != TILE_FLOOR ||
        getDungeonFurnitureAt(room, x, y) != nullptr ||
        getTrapAt(room, x, y) != nullptr || isHealingFountainTile(room, x, y))
    {
        return false;
    }

    for (DungeonFurnitureInstance& furniture : room.furniture)
    {
        if (furniture.type != FURNITURE_NONE)
            continue;

        furniture.type = type;
        furniture.x = static_cast<int8_t>(x);
        furniture.y = static_cast<int8_t>(y);
        furniture.hp = definition->maxHP;
        room.map.tiles[y][x] = definition->tile;
        return true;
    }

    return false;
}

DungeonFurnitureDamageResult damageDungeonFurniture(
    DungeonRoom& room, int x, int y, int rawDamage, DamageType damageType)
{
    DungeonFurnitureDamageResult result;
    DungeonFurnitureInstance* furniture = getDungeonFurnitureAt(room, x, y);
    if (furniture == nullptr || damageType == DAMAGE_NONE)
        return result;

    const DungeonFurnitureDefinition* definition =
        getDungeonFurnitureDefinition(furniture->type);
    if (definition == nullptr || definition->maxHP <= 0 || furniture->hp <= 0)
        return result;

    result.incomingDamage = clampPositiveDamage(rawDamage);
    result.hardnessPrevented = result.incomingDamage < definition->hardness
        ? result.incomingDamage : definition->hardness;
    result.appliedDamage = result.incomingDamage - result.hardnessPrevented;
    if (result.appliedDamage == 0)
        return result;

    furniture->hp -= result.appliedDamage;
    if (furniture->hp > 0)
        return result;

    furniture->hp = 0;
    result.destroyed = true;
    result.becameRubble = definition->becomesRubble;
    room.map.tiles[y][x] = definition->becomesRubble
        ? TILE_RUBBLE : TILE_FLOOR;
    *furniture = DungeonFurnitureInstance{};
    return result;
}

DungeonFurnitureDamageResult damageCurrentDungeonFurniture(
    int x, int y, int rawDamage, DamageType damageType)
{
    DungeonFurnitureDamageResult result;
    if (dungeon.currentRoom >= MAX_ROOMS)
        return result;

    result = damageDungeonFurniture(
        dungeon.rooms[dungeon.currentRoom], x, y, rawDamage, damageType);
    if (result.appliedDamage > 0 || result.destroyed)
        markTileDirty(x, y);
    return result;
}

FurniturePushResult tryPushDungeonFurniture(
    Dungeon& dungeon,
    Entity& pusher,
    int x,
    int y,
    int directionX,
    int directionY,
    int strengthCheckTotal)
{
    if (dungeon.currentRoom >= MAX_ROOMS || directionX == 0 && directionY == 0)
        return FURNITURE_PUSH_NOT_APPLICABLE;

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    DungeonFurnitureInstance* furniture = getDungeonFurnitureAt(room, x, y);
    if (furniture == nullptr)
        return FURNITURE_PUSH_NOT_APPLICABLE;

    const DungeonFurnitureDefinition* definition =
        getDungeonFurnitureDefinition(furniture->type);
    if (definition == nullptr || !definition->movable)
        return FURNITURE_PUSH_BLOCKED;

    const int destinationX = x + directionX;
    const int destinationY = y + directionY;
    if (!isPushDestinationValid(
            dungeon, room, pusher, destinationX, destinationY))
    {
        return FURNITURE_PUSH_BLOCKED;
    }

    if (strengthCheckTotal < definition->strengthDC)
        return FURNITURE_PUSH_FAILED_STRENGTH;

    room.map.tiles[y][x] = TILE_FLOOR;
    room.map.tiles[destinationY][destinationX] = definition->tile;
    furniture->x = static_cast<int8_t>(destinationX);
    furniture->y = static_cast<int8_t>(destinationY);
    return FURNITURE_PUSH_SUCCEEDED;
}
