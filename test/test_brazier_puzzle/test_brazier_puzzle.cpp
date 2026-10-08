#include <Arduino.h>
#include <unity.h>

#include "audio/audio.h"
#include "dungeon/brazierpuzzle.h"
#include "dungeon/dungeon.h"

Dungeon dungeon{};

static_assert(sizeof(BrazierPuzzleState) == 8, "Brazier state grew");
static_assert(sizeof(DungeonRoom) == 768, "Unexpected persistent room size");
static_assert(sizeof(DungeonRoomRuntime) == 16136,
              "Brazier puzzle must not grow room runtime state");
static_assert(sizeof(DungeonFurnitureInstance) == 6,
              "Brazier puzzle must not grow furniture instances");
static_assert(sizeof(Entity) == 1008,
              "Brazier puzzle must not grow Entity");

namespace
{
const char* messageText = "";
SoundEffect lastSound = SoundEffect::NONE;
uint8_t soundCount = 0;
uint8_t dirtyCount = 0;
int dirtyX[3] = {};
int dirtyY[3] = {};
}

const RoomConnection* getRoomConnection(
    const DungeonRoom& room, Direction direction)
{
    for (uint8_t i = 0; i < room.connectionCount; ++i)
        if (room.connections[i].direction == direction)
            return &room.connections[i];
    return nullptr;
}

bool validateRoomConnectivity(const DungeonRoom&) { return true; }
TrapInstance* getTrapAt(DungeonRoom&, int, int) { return nullptr; }
const TrapInstance* getTrapAt(const DungeonRoom&, int, int) { return nullptr; }
bool isHealingFountainTile(const DungeonRoom&, int, int) { return false; }

void markTileDirty(int x, int y)
{
    if (dirtyCount < 3)
    {
        dirtyX[dirtyCount] = x;
        dirtyY[dirtyCount] = y;
    }
    ++dirtyCount;
}

void markEntityFootprintDirty(const Entity&) {}
void setGameMessage(const char* text) { messageText = text; }
void playSound(SoundEffect sound)
{
    lastSound = sound;
    ++soundCount;
}

Entity* getEntityAt(Entity entities[], uint8_t count, uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < count; ++i)
        if (entities[i].active && entities[i].x == x && entities[i].y == y)
            return &entities[i];
    return nullptr;
}

#include "../../src/graphics/dungeonfurniture.cpp"
#include "../../src/dungeon/furniture.cpp"
#include "../../src/dungeon/brazierpuzzle.cpp"

namespace
{
void prepareRoom(DungeonRoom& room)
{
    room = DungeonRoom{};
    room.type = ROOM_PUZZLE;
    for (uint8_t y = 0; y < ROOM_HEIGHT; ++y)
        for (uint8_t x = 0; x < ROOM_WIDTH; ++x)
            room.map.tiles[y][x] = (x == 0 || y == 0 ||
                x == ROOM_WIDTH - 1 || y == ROOM_HEIGHT - 1)
                ? TILE_WALL : TILE_FLOOR;
    room.map.tiles[6][0] = TILE_DOOR;
    room.map.tiles[6][ROOM_WIDTH - 1] = TILE_DOOR;
    room.connections[0] = {DIR_WEST, 0, 6};
    room.connections[1] = {DIR_EAST, ROOM_WIDTH - 1, 6};
    room.connectionCount = 2;
}

void configureRoom(DungeonRoom& room, uint8_t countRoll)
{
    prepareRoom(room);
    uint8_t rolls[64] = {};
    for (uint8_t i = 0; i < sizeof(rolls); ++i)
        rolls[i] = static_cast<uint8_t>(countRoll + i * 37);
    rolls[0] = countRoll;
    TEST_ASSERT_TRUE(configureBrazierPuzzleRoom(
        room, DIR_EAST, rolls, sizeof(rolls)));
}

uint8_t countBrazierFurniture(const DungeonRoom& room)
{
    uint8_t count = 0;
    for (const DungeonFurnitureInstance& furniture : room.furniture)
        if (furniture.type == FURNITURE_BRAZIER) ++count;
    return count;
}

void attachRewardChest(DungeonRoom& room)
{
    dungeon.entities = dungeon.activeDungeonEntities;
    dungeon.entityCount = 1;
    Entity& chest = dungeon.entities[0];
    chest = Entity{};
    chest.active = true;
    chest.type = ENTITY_CHEST;
    chest.x = room.brazierPuzzle.rewardX;
    chest.y = room.brazierPuzzle.rewardY;
    chest.locked = true;
}
}

void setUp()
{
    dungeon = Dungeon{};
    dungeon.roomCount = 1;
    dungeon.currentRoom = 0;
    dungeon.entities = dungeon.activeDungeonEntities;
    messageText = "";
    lastSound = SoundEffect::NONE;
    soundCount = 0;
    dirtyCount = 0;
}

void tearDown() {}

void test_toggle_rules_cover_four_and_five_brazier_edges_and_centers()
{
    TEST_ASSERT_EQUAL_UINT8(0b0011, toggleBrazierMask(0, 4, 0));
    TEST_ASSERT_EQUAL_UINT8(0b1100, toggleBrazierMask(0, 4, 3));
    TEST_ASSERT_EQUAL_UINT8(0b0111, toggleBrazierMask(0, 4, 1));
    TEST_ASSERT_EQUAL_UINT8(0b1110, toggleBrazierMask(0, 4, 2));
    TEST_ASSERT_EQUAL_UINT8(0b00011, toggleBrazierMask(0, 5, 0));
    TEST_ASSERT_EQUAL_UINT8(0b01110, toggleBrazierMask(0, 5, 2));
    TEST_ASSERT_EQUAL_UINT8(0b11000, toggleBrazierMask(0, 5, 4));
}

void test_solved_check_masks_unused_upper_bits()
{
    TEST_ASSERT_TRUE(areAllBrazierPuzzleBraziersLit(0b1111, 4));
    TEST_ASSERT_TRUE(areAllBrazierPuzzleBraziersLit(0b11101111, 4));
    TEST_ASSERT_FALSE(areAllBrazierPuzzleBraziersLit(0b0111, 4));
    TEST_ASSERT_TRUE(areAllBrazierPuzzleBraziersLit(0b11111, 5));
    TEST_ASSERT_FALSE(areAllBrazierPuzzleBraziersLit(0b11110, 5));
}

void test_scrambles_are_unsolved_reachable_difficult_and_vary()
{
    const uint8_t rollsA[] = {3, 17, 91, 4, 55, 18, 201, 72};
    const uint8_t rollsB[] = {8, 2, 33, 111, 7, 44, 19, 5};
    const uint8_t fourA = generateBrazierScramble(4, rollsA, sizeof(rollsA));
    const uint8_t fourB = generateBrazierScramble(4, rollsB, sizeof(rollsB));
    TEST_ASSERT_NOT_EQUAL(0b1111, fourA);
    TEST_ASSERT_NOT_EQUAL(0b1111, fourB);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, getBrazierMinimumSolutionMoves(fourA, 4));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, getBrazierMinimumSolutionMoves(fourB, 4));
    TEST_ASSERT_NOT_EQUAL(fourA, fourB);

    const uint8_t five = generateBrazierScramble(5, rollsA, sizeof(rollsA));
    TEST_ASSERT_NOT_EQUAL(0b11111, five);
    TEST_ASSERT_EQUAL_UINT8(3, getBrazierMinimumSolutionMoves(five, 5));
    TEST_ASSERT_NOT_EQUAL(BRAZIER_PUZZLE_UNREACHABLE,
                          getBrazierMinimumSolutionMoves(five, 5));

    const uint8_t repetitiveRolls[8] = {};
    const uint8_t fallback = generateBrazierScramble(
        5, repetitiveRolls, sizeof(repetitiveRolls));
    TEST_ASSERT_EQUAL_UINT8(3, getBrazierMinimumSolutionMoves(fallback, 5));
}

void test_room_generation_places_exact_horizontal_accessible_row_and_reward()
{
    DungeonRoom room;
    configureRoom(room, 0);
    TEST_ASSERT_EQUAL_UINT8(4, room.brazierPuzzle.brazierCount);
    TEST_ASSERT_EQUAL_UINT8(4, countBrazierFurniture(room));
    TEST_ASSERT_EQUAL(PUZZLE_BRAZIERS, room.puzzleType);
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);
    TEST_ASSERT_EQUAL(TILE_CHEST_SPAWN,
        room.map.tiles[room.brazierPuzzle.rewardY][room.brazierPuzzle.rewardX]);
    for (uint8_t index = 0; index < room.brazierPuzzle.brazierCount; ++index)
    {
        const int x = room.brazierPuzzle.rowX + index;
        TEST_ASSERT_EQUAL_INT8(index, getBrazierPuzzleIndexAt(
            room, x, room.brazierPuzzle.rowY));
        TEST_ASSERT_EQUAL(TILE_FLOOR,
            room.map.tiles[room.brazierPuzzle.rowY - 1][x]);
        TEST_ASSERT_EQUAL(TILE_FLOOR,
            room.map.tiles[room.brazierPuzzle.rowY + 1][x]);
    }

    configureRoom(room, 1);
    TEST_ASSERT_EQUAL_UINT8(5, room.brazierPuzzle.brazierCount);
    TEST_ASSERT_EQUAL_UINT8(5, countBrazierFurniture(room));
    TEST_ASSERT_FALSE(areAllBrazierPuzzleBraziersLit(
        room.brazierPuzzle.litMask, room.brazierPuzzle.brazierCount));
}

void test_render_selection_preserves_normal_brazier_and_switches_puzzle_art()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureRoom(room, 0);
    const int x = room.brazierPuzzle.rowX;
    const int y = room.brazierPuzzle.rowY;
    room.brazierPuzzle.litMask &= static_cast<uint8_t>(~1u);
    TEST_ASSERT_EQUAL_PTR(dungeonBrazierUnlit16x16,
        getBrazierSpriteForRoomTile(room, x, y));
    room.brazierPuzzle.litMask |= 1u;
    TEST_ASSERT_EQUAL_PTR(dungeonBrazier16x16,
        getBrazierSpriteForRoomTile(room, x, y));
    room.puzzleType = PUZZLE_NONE;
    TEST_ASSERT_EQUAL_PTR(dungeonBrazier16x16,
        getBrazierSpriteForRoomTile(room, x, y));
    TEST_ASSERT_EQUAL_UINT32(256 * sizeof(uint16_t),
                             sizeof(dungeonBrazierUnlit16x16));
}

void test_interaction_invalidates_only_changed_tiles_and_queues_one_whoosh()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureRoom(room, 1);
    room.brazierPuzzle.litMask = 0;
    const int x = room.brazierPuzzle.rowX + 2;
    const int y = room.brazierPuzzle.rowY;
    TEST_ASSERT_TRUE(interactWithCurrentBrazierPuzzleAt(x, y));
    TEST_ASSERT_EQUAL_UINT8(0b01110, room.brazierPuzzle.litMask);
    TEST_ASSERT_EQUAL_UINT8(3, dirtyCount);
    TEST_ASSERT_EQUAL_UINT8(1, soundCount);
    TEST_ASSERT_EQUAL(SoundEffect::BRAZIER_IGNITE, lastSound);

    dirtyCount = 0;
    soundCount = 0;
    TEST_ASSERT_TRUE(interactWithCurrentBrazierPuzzleAt(x, y));
    TEST_ASSERT_EQUAL_UINT8(0, room.brazierPuzzle.litMask);
    TEST_ASSERT_EQUAL_UINT8(3, dirtyCount);
    TEST_ASSERT_EQUAL_UINT8(0, soundCount);
}

void test_solving_unlocks_exit_and_reward_once_and_persists_state()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureRoom(room, 1);
    attachRewardChest(room);
    const uint8_t selected = 2;
    room.brazierPuzzle.litMask = toggleBrazierMask(
        getBrazierActiveMask(room.brazierPuzzle.brazierCount),
        room.brazierPuzzle.brazierCount, selected);
    TEST_ASSERT_TRUE(isCurrentBrazierPuzzleRewardSealed());
    TEST_ASSERT_FALSE(tryUnlockCurrentBrazierPuzzleExit(DIR_EAST));
    TEST_ASSERT_TRUE(interactWithCurrentBrazierPuzzleAt(
        room.brazierPuzzle.rowX + selected, room.brazierPuzzle.rowY));
    TEST_ASSERT_EQUAL(BRAZIER_PUZZLE_COMPLETE, room.brazierPuzzle.progress);
    TEST_ASSERT_TRUE(room.completed);
    TEST_ASSERT_FALSE(dungeon.entities[0].locked);
    TEST_ASSERT_FALSE(isCurrentBrazierPuzzleRewardSealed());
    TEST_ASSERT_TRUE(tryUnlockCurrentBrazierPuzzleExit(DIR_EAST));

    const uint8_t solvedMask = room.brazierPuzzle.litMask;
    const uint8_t soundsAfterSolve = soundCount;
    TEST_ASSERT_TRUE(interactWithCurrentBrazierPuzzleAt(
        room.brazierPuzzle.rowX + selected, room.brazierPuzzle.rowY));
    TEST_ASSERT_EQUAL_UINT8(solvedMask, room.brazierPuzzle.litMask);
    TEST_ASSERT_EQUAL_UINT8(soundsAfterSolve, soundCount);

    DungeonRoom persisted = room;
    TEST_ASSERT_EQUAL(BRAZIER_PUZZLE_COMPLETE,
                      persisted.brazierPuzzle.progress);
    TEST_ASSERT_EQUAL_UINT8(solvedMask, persisted.brazierPuzzle.litMask);
    TEST_ASSERT_EQUAL_INT8(room.brazierPuzzle.rewardX,
                           persisted.brazierPuzzle.rewardX);
}

void test_normal_brazier_never_invokes_puzzle_toggle()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    TEST_ASSERT_TRUE(addDungeonFurniture(room, FURNITURE_BRAZIER, 7, 7));
    TEST_ASSERT_FALSE(interactWithCurrentBrazierPuzzleAt(7, 7));
    TEST_ASSERT_EQUAL_UINT8(0, soundCount);
    TEST_ASSERT_EQUAL_UINT8(0, dirtyCount);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_toggle_rules_cover_four_and_five_brazier_edges_and_centers);
    RUN_TEST(test_solved_check_masks_unused_upper_bits);
    RUN_TEST(test_scrambles_are_unsolved_reachable_difficult_and_vary);
    RUN_TEST(test_room_generation_places_exact_horizontal_accessible_row_and_reward);
    RUN_TEST(test_render_selection_preserves_normal_brazier_and_switches_puzzle_art);
    RUN_TEST(test_interaction_invalidates_only_changed_tiles_and_queues_one_whoosh);
    RUN_TEST(test_solving_unlocks_exit_and_reward_once_and_persists_state);
    RUN_TEST(test_normal_brazier_never_invokes_puzzle_toggle);
    UNITY_END();
}

void loop() {}
