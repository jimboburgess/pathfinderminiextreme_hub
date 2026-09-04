#include <Arduino.h>
#include <unity.h>

#include "../../src/dungeon/dungeongraph.cpp"

namespace
{
bool roomConnectsTo(const DungeonRoom& room, uint8_t target)
{
    return room.north == target || room.east == target ||
        room.south == target || room.west == target;
}
}

void setUp() {}
void tearDown() {}

void test_generated_topologies_hold_all_graph_invariants_across_300_seeds()
{
    bool sawLoop = false;
    bool sawDeadEnd = false;
    bool sawDifferentRoomCounts = false;
    uint8_t previousRoomCount = 0;

    for (uint16_t seed = 1; seed <= 300; seed++)
    {
        randomSeed(seed);
        Dungeon dungeon{};
        const uint8_t roomCount = static_cast<uint8_t>(
            MIN_DUNGEON_ROOMS + seed %
                (MAX_DUNGEON_ROOMS - MIN_DUNGEON_ROOMS + 1));
        TEST_ASSERT_TRUE(generateDungeonTopology(dungeon, roomCount));
        TEST_ASSERT_TRUE(validateDungeonTopology(dungeon));
        TEST_ASSERT_EQUAL_UINT8(roomCount, dungeon.roomCount);
        TEST_ASSERT_EQUAL_UINT8(0, getRoomDistanceFromEntrance(dungeon, 0));
        TEST_ASSERT_TRUE(dungeonGraphHasBranch(dungeon));
        TEST_ASSERT_TRUE(getRoomDistanceFromEntrance(
            dungeon, dungeon.bossRoom) >= MIN_BOSS_GRAPH_DISTANCE);
        TEST_ASSERT_EQUAL_UINT8(
            1, getRoomDegree(dungeon.rooms[dungeon.treasureRoom]));
        TEST_ASSERT_TRUE(roomConnectsTo(
            dungeon.rooms[dungeon.treasureRoom], dungeon.bossRoom));

        uint8_t entranceCount = 0;
        uint8_t bossCount = 0;
        uint8_t treasureCount = 0;
        for (uint8_t i = 0; i < dungeon.roomCount; i++)
        {
            entranceCount += dungeon.rooms[i].type == ROOM_ENTRANCE;
            bossCount += dungeon.rooms[i].type == ROOM_BOSS;
            treasureCount += dungeon.rooms[i].type == ROOM_TREASURE;
            TEST_ASSERT_NOT_EQUAL(NO_ROOM,
                getRoomDistanceFromEntrance(dungeon, i));
            if (i != 0 && i != dungeon.treasureRoom &&
                isDeadEndRoom(dungeon.rooms[i]))
                sawDeadEnd = true;
            for (uint8_t other = i + 1; other < dungeon.roomCount; other++)
                TEST_ASSERT_FALSE(
                    dungeon.rooms[i].dungeonX == dungeon.rooms[other].dungeonX &&
                    dungeon.rooms[i].dungeonY == dungeon.rooms[other].dungeonY);
        }
        TEST_ASSERT_EQUAL_UINT8(1, entranceCount);
        TEST_ASSERT_EQUAL_UINT8(1, bossCount);
        TEST_ASSERT_EQUAL_UINT8(1, treasureCount);
        sawLoop = sawLoop || dungeonGraphHasLoop(dungeon);
        sawDifferentRoomCounts = sawDifferentRoomCounts ||
            (previousRoomCount != 0 && previousRoomCount != roomCount);
        previousRoomCount = roomCount;
    }

    TEST_ASSERT_TRUE(sawLoop);
    TEST_ASSERT_TRUE(sawDeadEnd);
    TEST_ASSERT_TRUE(sawDifferentRoomCounts);
}

void test_invalid_room_counts_are_rejected()
{
    Dungeon dungeon{};
    TEST_ASSERT_FALSE(generateDungeonTopology(
        dungeon, MIN_DUNGEON_ROOMS - 1));
    TEST_ASSERT_FALSE(generateDungeonTopology(
        dungeon, MAX_DUNGEON_ROOMS + 1));
}

void test_graph_data_survives_normal_runtime_mutation()
{
    randomSeed(77);
    Dungeon dungeon{};
    TEST_ASSERT_TRUE(generateDungeonTopology(dungeon, 10));
    const int8_t roomX = dungeon.rooms[3].dungeonX;
    const int8_t roomY = dungeon.rooms[3].dungeonY;
    const uint8_t east = dungeon.rooms[3].east;
    const uint8_t boss = dungeon.bossRoom;

    dungeon.rooms[3].completed = true;
    dungeon.roomRuntime[3].initialized = true;
    dungeon.roomRuntime[3].entityCount = 1;

    TEST_ASSERT_EQUAL_INT8(roomX, dungeon.rooms[3].dungeonX);
    TEST_ASSERT_EQUAL_INT8(roomY, dungeon.rooms[3].dungeonY);
    TEST_ASSERT_EQUAL_UINT8(east, dungeon.rooms[3].east);
    TEST_ASSERT_EQUAL_UINT8(boss, dungeon.bossRoom);
    TEST_ASSERT_TRUE(validateDungeonTopology(dungeon));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_generated_topologies_hold_all_graph_invariants_across_300_seeds);
    RUN_TEST(test_invalid_room_counts_are_rejected);
    RUN_TEST(test_graph_data_survives_normal_runtime_mutation);
    UNITY_END();
}

void loop() {}
