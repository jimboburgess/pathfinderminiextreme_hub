#include <Arduino.h>
#include <unity.h>

#include "../../src/dungeon/dungeon.h"
#include "../../src/data/entityspawn.h"

Dungeon dungeon{};

void markTileDirty(int, int)
{
}

TrapInstance* getTrapAt(DungeonRoom&, int, int)
{
    return nullptr;
}

const TrapInstance* getTrapAt(const DungeonRoom&, int, int)
{
    return nullptr;
}

bool isHealingFountainTile(const DungeonRoom&, int, int)
{
    return false;
}

Entity* getEntityAt(Entity[], uint8_t, uint8_t, uint8_t)
{
    return nullptr;
}

#include "../../src/dungeon/furniture.cpp"

namespace
{
void fillTestFloor(DungeonRoom& room)
{
    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
    }
}
}

void test_barrel_and_crate_definitions_are_tunable_and_blocking()
{
    const DungeonFurnitureDefinition* barrel =
        getDungeonFurnitureDefinition(FURNITURE_BARREL);
    const DungeonFurnitureDefinition* crate =
        getDungeonFurnitureDefinition(FURNITURE_CRATE);
    TEST_ASSERT_NOT_NULL(barrel);
    TEST_ASSERT_NOT_NULL(crate);
    TEST_ASSERT_EQUAL_INT(6, barrel->maxHP);
    TEST_ASSERT_EQUAL_UINT8(1, barrel->hardness);
    TEST_ASSERT_EQUAL_UINT8(BARREL_STRENGTH_DC, barrel->strengthDC);
    TEST_ASSERT_EQUAL_INT(10, crate->maxHP);
    TEST_ASSERT_EQUAL_UINT8(2, crate->hardness);
    TEST_ASSERT_EQUAL_UINT8(CRATE_STRENGTH_DC, crate->strengthDC);
    TEST_ASSERT_FALSE(isDungeonFloorTerrain(TILE_BARREL));
    TEST_ASSERT_FALSE(isTileBlockingSight(TILE_BARREL));
}

void test_furniture_damage_respects_hardness_and_removes_destroyed_object()
{
    DungeonRoom room{};
    fillTestFloor(room);
    TEST_ASSERT_TRUE(addDungeonFurniture(room, FURNITURE_BARREL, 4, 4));

    DungeonFurnitureDamageResult resisted = damageDungeonFurniture(
        room, 4, 4, 1, DAMAGE_FIRE);
    TEST_ASSERT_EQUAL_UINT16(1, resisted.hardnessPrevented);
    TEST_ASSERT_EQUAL_UINT16(0, resisted.appliedDamage);
    TEST_ASSERT_FALSE(resisted.destroyed);

    DungeonFurnitureDamageResult destroyed = damageDungeonFurniture(
        room, 4, 4, 7, DAMAGE_FIRE);
    TEST_ASSERT_EQUAL_UINT16(6, destroyed.appliedDamage);
    TEST_ASSERT_TRUE(destroyed.destroyed);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[4][4]);
    TEST_ASSERT_NULL(getDungeonFurnitureAt(room, 4, 4));
}

void test_statue_blocks_los_and_becomes_rubble_when_destroyed()
{
    DungeonRoom room{};
    fillTestFloor(room);
    TEST_ASSERT_TRUE(addDungeonFurniture(room, FURNITURE_STATUE, 6, 6));
    TEST_ASSERT_TRUE(isTileBlockingSight(TILE_STATUE));

    DungeonFurnitureDamageResult destroyed = damageDungeonFurniture(
        room, 6, 6, 35, DAMAGE_BLUDGEONING);
    TEST_ASSERT_TRUE(destroyed.destroyed);
    TEST_ASSERT_TRUE(destroyed.becameRubble);
    TEST_ASSERT_EQUAL(TILE_RUBBLE, room.map.tiles[6][6]);
    TEST_ASSERT_TRUE(isDungeonFloorTerrain(room.map.tiles[6][6]));
}

void test_push_uses_strength_total_and_requires_clear_destination()
{
    DungeonRoom room{};
    fillTestFloor(room);
    Dungeon dungeon{};
    Entity pusher{};
    Entity entities[] = {pusher};
    dungeon.currentRoom = 0;
    dungeon.entities = entities;
    dungeon.entityCount = 1;
    dungeon.rooms[0] = room;
    TEST_ASSERT_TRUE(addDungeonFurniture(
        dungeon.rooms[0], FURNITURE_CRATE, 5, 5));

    TEST_ASSERT_EQUAL(
        FURNITURE_PUSH_FAILED_STRENGTH,
        tryPushDungeonFurniture(dungeon, entities[0], 5, 5, 1, 0, 11));
    TEST_ASSERT_EQUAL(
        FURNITURE_PUSH_SUCCEEDED,
        tryPushDungeonFurniture(dungeon, entities[0], 5, 5, 1, 0, 12));
    TEST_ASSERT_EQUAL(TILE_FLOOR, dungeon.rooms[0].map.tiles[5][5]);
    TEST_ASSERT_EQUAL(TILE_CRATE, dungeon.rooms[0].map.tiles[5][6]);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_barrel_and_crate_definitions_are_tunable_and_blocking);
    RUN_TEST(test_furniture_damage_respects_hardness_and_removes_destroyed_object);
    RUN_TEST(test_statue_blocks_los_and_becomes_rubble_when_destroyed);
    RUN_TEST(test_push_uses_strength_total_and_requires_clear_destination);
    UNITY_END();
}

void loop()
{
}
