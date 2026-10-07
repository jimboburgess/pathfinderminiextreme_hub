#include <Arduino.h>
#include <unity.h>

#include "audio/audio.h"
#include "dungeon/puzzleroom.h"

Dungeon dungeon{};

namespace
{
const char* messageText = "";
}

const RoomConnection* getRoomConnection(
    const DungeonRoom& room, Direction direction)
{
    for (uint8_t index = 0; index < room.connectionCount; ++index)
        if (room.connections[index].direction == direction)
            return &room.connections[index];
    return nullptr;
}

bool validateRoomConnectivity(const DungeonRoom&) { return true; }
TrapInstance* getTrapAt(DungeonRoom&, int, int) { return nullptr; }
const TrapInstance* getTrapAt(const DungeonRoom&, int, int) { return nullptr; }
bool isHealingFountainTile(const DungeonRoom&, int, int) { return false; }
void markTileDirty(int, int) {}
void markEntityFootprintDirty(const Entity&) {}
void setGameMessage(const char* text) { messageText = text; }
void playSound(SoundEffect) {}
bool playToneSequence(const uint16_t*, uint8_t, uint16_t, uint16_t, AudioDuty)
{
    return true;
}

Entity* getEntityAt(Entity entities[], uint8_t count, uint8_t x, uint8_t y)
{
    for (uint8_t index = 0; index < count; ++index)
        if (entities[index].active && entities[index].x == x &&
            entities[index].y == y)
            return &entities[index];
    return nullptr;
}

Entity* spawnEntity(Entity entities[], uint8_t& count, EntityType type,
                    uint8_t x, uint8_t y)
{
    if (count >= MAX_ENTITIES) return nullptr;
    Entity& entity = entities[count++];
    entity = Entity{};
    entity.active = true;
    entity.type = type;
    entity.x = x;
    entity.y = y;
    return &entity;
}

#include "../../src/graphics/dungeonfurniture.cpp"
#include "../../src/dungeon/furniture.cpp"
#include "../../src/dungeon/bellpuzzle.cpp"
#include "../../src/dungeon/numbertilepuzzle.cpp"
#include "../../src/dungeon/brazierpuzzle.cpp"
#include "../../src/dungeon/puzzleroom.cpp"

namespace
{
void prepareSquareRoom(DungeonRoom& room, bool oppositeConnections = true)
{
    room = DungeonRoom{};
    room.type = ROOM_PUZZLE;
    room.shape = SHAPE_SQUARE;
    for (uint8_t y = 0; y < ROOM_HEIGHT; ++y)
        for (uint8_t x = 0; x < ROOM_WIDTH; ++x)
            room.map.tiles[y][x] = (x == 0 || y == 0 ||
                x == ROOM_WIDTH - 1 || y == ROOM_HEIGHT - 1)
                ? TILE_WALL : TILE_FLOOR;
    room.map.tiles[6][ROOM_WIDTH - 1] = TILE_DOOR;
    room.connections[0] = {DIR_EAST, ROOM_WIDTH - 1, 6};
    room.connectionCount = 1;
    if (oppositeConnections)
    {
        room.map.tiles[6][0] = TILE_DOOR;
        room.connections[1] = {DIR_WEST, 0, 6};
        room.connectionCount = 2;
    }
}

void fillRolls(uint8_t rolls[PUZZLE_GENERATION_ROLL_COUNT], uint8_t seed)
{
    for (uint16_t index = 0; index < PUZZLE_GENERATION_ROLL_COUNT; ++index)
        rolls[index] = static_cast<uint8_t>(seed + index * 37u);
}

uint8_t countFurniture(const DungeonRoom& room, DungeonFurnitureType type)
{
    uint8_t count = 0;
    for (const DungeonFurnitureInstance& furniture : room.furniture)
        if (furniture.type == type) ++count;
    return count;
}

void seedMixedPuzzleState(DungeonRoom& room)
{
    room.puzzleType = PUZZLE_RIDDLEMAN;
    room.npcSpawn.id = NPC_BERTRAM_RIDDLEMAN;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_UNSOLVED;
    room.bellPuzzle.progress = BELL_PUZZLE_COMPLETE;
    room.numberPuzzle.progress = NUMBER_PUZZLE_COMPLETE;
    room.brazierPuzzle.progress = BRAZIER_PUZZLE_COMPLETE;
}
}

void setUp()
{
    dungeon = Dungeon{};
    dungeon.roomCount = 1;
    dungeon.entities = dungeon.activeDungeonEntities;
    messageText = "";
}

void tearDown() {}

void test_middle_room_category_boundaries_remain_thirty_thirty_thirty_ten()
{
    TEST_ASSERT_EQUAL(ROOM_COMBAT, selectMiddleRoomTypeFromRoll(0));
    TEST_ASSERT_EQUAL(ROOM_COMBAT, selectMiddleRoomTypeFromRoll(29));
    TEST_ASSERT_EQUAL(ROOM_AMBUSH, selectMiddleRoomTypeFromRoll(30));
    TEST_ASSERT_EQUAL(ROOM_AMBUSH, selectMiddleRoomTypeFromRoll(59));
    TEST_ASSERT_EQUAL(ROOM_PUZZLE, selectMiddleRoomTypeFromRoll(60));
    TEST_ASSERT_EQUAL(ROOM_PUZZLE, selectMiddleRoomTypeFromRoll(89));
    TEST_ASSERT_EQUAL(ROOM_EMPTY, selectMiddleRoomTypeFromRoll(90));
    TEST_ASSERT_EQUAL(ROOM_EMPTY, selectMiddleRoomTypeFromRoll(99));
}

void test_puzzle_selector_boundaries_are_34_33_33_without_a_none_hole()
{
    TEST_ASSERT_EQUAL(PUZZLE_BELLS, selectRandomPuzzleType(0));
    TEST_ASSERT_EQUAL(PUZZLE_BELLS, selectRandomPuzzleType(33));
    TEST_ASSERT_EQUAL(PUZZLE_NUMBER_TILES, selectRandomPuzzleType(34));
    TEST_ASSERT_EQUAL(PUZZLE_NUMBER_TILES, selectRandomPuzzleType(66));
    TEST_ASSERT_EQUAL(PUZZLE_BRAZIERS, selectRandomPuzzleType(67));
    TEST_ASSERT_EQUAL(PUZZLE_BRAZIERS, selectRandomPuzzleType(99));
    for (uint8_t roll = 0; roll < 100; ++roll)
    {
        const DungeonPuzzleType selected = selectRandomPuzzleType(roll);
        TEST_ASSERT_TRUE(isRotatingDungeonPuzzleType(selected));
        TEST_ASSERT_NOT_EQUAL(PUZZLE_NONE, selected);
        TEST_ASSERT_NOT_EQUAL(PUZZLE_RIDDLEMAN, selected);
    }
}

void test_each_registered_type_configures_and_clears_other_blueprints()
{
    uint8_t rolls[PUZZLE_GENERATION_ROLL_COUNT];
    fillRolls(rolls, 11);
    DungeonRoom room;

    prepareSquareRoom(room);
    seedMixedPuzzleState(room);
    TEST_ASSERT_TRUE(configureSelectedPuzzleRoom(
        room, PUZZLE_BELLS, DIR_EAST, 5, rolls, sizeof(rolls)));
    TEST_ASSERT_TRUE(isBellPuzzleRoom(room));
    TEST_ASSERT_TRUE(isValidBellSequence(
        room.bellPuzzle.sequence, room.bellPuzzle.sequenceLength));
    TEST_ASSERT_EQUAL_UINT8(1, countFurniture(room, FURNITURE_BELL_LOW));
    TEST_ASSERT_EQUAL_UINT8(1, countFurniture(room, FURNITURE_BELL_MID));
    TEST_ASSERT_EQUAL_UINT8(1, countFurniture(room, FURNITURE_BELL_HIGH));
    TEST_ASSERT_EQUAL(NUMBER_PUZZLE_NONE, room.numberPuzzle.progress);
    TEST_ASSERT_EQUAL(BRAZIER_PUZZLE_NONE, room.brazierPuzzle.progress);
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);

    prepareSquareRoom(room);
    seedMixedPuzzleState(room);
    TEST_ASSERT_TRUE(configureSelectedPuzzleRoom(
        room, PUZZLE_NUMBER_TILES, DIR_EAST, 5, rolls, sizeof(rolls)));
    TEST_ASSERT_TRUE(isNumberTilePuzzleRoom(room));
    TEST_ASSERT_TRUE(validateNumberPuzzleSafePath(room));
    TEST_ASSERT_EQUAL(BELL_PUZZLE_NONE, room.bellPuzzle.progress);
    TEST_ASSERT_EQUAL(BRAZIER_PUZZLE_NONE, room.brazierPuzzle.progress);
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);

    prepareSquareRoom(room);
    seedMixedPuzzleState(room);
    TEST_ASSERT_TRUE(configureSelectedPuzzleRoom(
        room, PUZZLE_BRAZIERS, DIR_EAST, 5, rolls, sizeof(rolls)));
    TEST_ASSERT_TRUE(isBrazierPuzzleRoom(room));
    TEST_ASSERT_TRUE(room.brazierPuzzle.brazierCount == 4 ||
                     room.brazierPuzzle.brazierCount == 5);
    TEST_ASSERT_FALSE(areAllBrazierPuzzleBraziersLit(
        room.brazierPuzzle.litMask, room.brazierPuzzle.brazierCount));
    TEST_ASSERT_NOT_EQUAL(BRAZIER_PUZZLE_UNREACHABLE,
        getBrazierMinimumSolutionMoves(
            room.brazierPuzzle.litMask, room.brazierPuzzle.brazierCount));
    TEST_ASSERT_EQUAL(BELL_PUZZLE_NONE, room.bellPuzzle.progress);
    TEST_ASSERT_EQUAL(NUMBER_PUZZLE_NONE, room.numberPuzzle.progress);
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);
}

void test_ineligible_preference_tries_each_other_type_once()
{
    DungeonRoom room;
    prepareSquareRoom(room, false);
    uint8_t rolls[PUZZLE_GENERATION_ROLL_COUNT];
    fillRolls(rolls, 29);
    TEST_ASSERT_TRUE(configureRotatingPuzzleRoom(
        room, PUZZLE_NUMBER_TILES, DIR_EAST, 5,
        rolls, sizeof(rolls)));
    TEST_ASSERT_EQUAL(PUZZLE_BRAZIERS, room.puzzleType);
    TEST_ASSERT_TRUE(isBrazierPuzzleRoom(room));
}

void test_no_eligible_type_falls_back_to_plain_traversable_empty_room()
{
    DungeonRoom room;
    prepareSquareRoom(room, false);
    room.connectionCount = 0;
    room.map.tiles[6][ROOM_WIDTH - 1] = TILE_FLOOR;
    uint8_t rolls[PUZZLE_GENERATION_ROLL_COUNT];
    fillRolls(rolls, 53);
    TEST_ASSERT_FALSE(configureRotatingPuzzleRoom(
        room, PUZZLE_BELLS, DIR_EAST, 5, rolls, sizeof(rolls)));
    TEST_ASSERT_EQUAL(ROOM_EMPTY, room.type);
    TEST_ASSERT_EQUAL(PUZZLE_NONE, room.puzzleType);
    TEST_ASSERT_EQUAL(BELL_PUZZLE_NONE, room.bellPuzzle.progress);
    TEST_ASSERT_EQUAL(NUMBER_PUZZLE_NONE, room.numberPuzzle.progress);
    TEST_ASSERT_EQUAL(BRAZIER_PUZZLE_NONE, room.brazierPuzzle.progress);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
}

void test_completion_dispatch_never_completes_an_unsolved_empty_puzzle_room()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;

    room.puzzleType = PUZZLE_BELLS;
    room.bellPuzzle.progress = BELL_PUZZLE_UNSOLVED;
    TEST_ASSERT_FALSE(isDungeonPuzzleComplete(room));
    room.bellPuzzle.progress = BELL_PUZZLE_COMPLETE;
    TEST_ASSERT_TRUE(isDungeonPuzzleComplete(room));

    room.puzzleType = PUZZLE_NUMBER_TILES;
    room.numberPuzzle.progress = NUMBER_PUZZLE_UNSOLVED;
    TEST_ASSERT_FALSE(isDungeonPuzzleComplete(room));
    room.numberPuzzle.progress = NUMBER_PUZZLE_COMPLETE;
    TEST_ASSERT_TRUE(isDungeonPuzzleComplete(room));

    room.puzzleType = PUZZLE_BRAZIERS;
    room.brazierPuzzle.progress = BRAZIER_PUZZLE_UNSOLVED;
    TEST_ASSERT_FALSE(isDungeonPuzzleComplete(room));
    room.brazierPuzzle.progress = BRAZIER_PUZZLE_COMPLETE;
    TEST_ASSERT_TRUE(isDungeonPuzzleComplete(room));

    room.puzzleType = PUZZLE_RIDDLEMAN;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_UNSOLVED;
    TEST_ASSERT_FALSE(isDungeonPuzzleComplete(room));
    room.npcSpawn.puzzleState = RIDDLE_ROOM_COMPLETE;
    TEST_ASSERT_TRUE(isDungeonPuzzleComplete(room));

    room.puzzleType = PUZZLE_NONE;
    TEST_ASSERT_FALSE(isDungeonPuzzleComplete(room));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_middle_room_category_boundaries_remain_thirty_thirty_thirty_ten);
    RUN_TEST(test_puzzle_selector_boundaries_are_34_33_33_without_a_none_hole);
    RUN_TEST(test_each_registered_type_configures_and_clears_other_blueprints);
    RUN_TEST(test_ineligible_preference_tries_each_other_type_once);
    RUN_TEST(test_no_eligible_type_falls_back_to_plain_traversable_empty_room);
    RUN_TEST(test_completion_dispatch_never_completes_an_unsolved_empty_puzzle_room);
    UNITY_END();
}

void loop() {}
