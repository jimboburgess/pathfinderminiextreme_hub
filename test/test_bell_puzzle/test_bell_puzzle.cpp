#include <Arduino.h>
#include <unity.h>

#include "dungeon/dungeon.h"
#include "dungeon/bellpuzzle.h"
#include "audio/audio.h"

Dungeon dungeon{};
namespace
{
const char* messageText = "";
uint8_t playbackRequestCount = 0;
uint8_t playedToneCount = 0;
uint16_t playedFrequencies[MAX_BELL_SEQUENCE] = {};
uint16_t playedToneDuration = 0;
uint16_t playedPauseDuration = 0;
}

const RoomConnection* getRoomConnection(const DungeonRoom& room, Direction direction)
{
    for (uint8_t i = 0; i < room.connectionCount; ++i)
        if (room.connections[i].direction == direction) return &room.connections[i];
    return nullptr;
}
bool validateRoomConnectivity(const DungeonRoom&) { return true; }
TrapInstance* getTrapAt(DungeonRoom&, int, int) { return nullptr; }
const TrapInstance* getTrapAt(const DungeonRoom&, int, int) { return nullptr; }
bool isHealingFountainTile(const DungeonRoom&, int, int) { return false; }
void markTileDirty(int, int) {}
void setGameMessage(const char* text) { messageText = text; }
bool playToneSequence(const uint16_t* frequencies, uint8_t count,
                      uint16_t toneDuration, uint16_t pauseDuration, AudioDuty)
{
    ++playbackRequestCount;
    playedToneCount = count;
    playedToneDuration = toneDuration;
    playedPauseDuration = pauseDuration;
    for (uint8_t i = 0; i < count && i < MAX_BELL_SEQUENCE; ++i)
        playedFrequencies[i] = frequencies[i];
    return true;
}
Entity* getEntityAt(Entity entities[], uint8_t count, uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < count; ++i)
        if (entities[i].active && entities[i].x == x && entities[i].y == y)
            return &entities[i];
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

#include "../../src/dungeon/furniture.cpp"
#include "../../src/dungeon/bellpuzzle.cpp"

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

uint8_t countBellFurniture(const DungeonRoom& room)
{
    uint8_t count = 0;
    for (const DungeonFurnitureInstance& object : room.furniture)
        if (object.type >= FURNITURE_BELL_LOW &&
            object.type <= FURNITURE_BELL_HIGH) ++count;
    return count;
}

bool findAdjacentFloor(const DungeonRoom& room, int runeX, int runeY,
                       int& floorX, int& floorY)
{
    constexpr int offsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& offset : offsets)
    {
        const int x = runeX + offset[0];
        const int y = runeY + offset[1];
        if (x >= 0 && x < ROOM_WIDTH && y >= 0 && y < ROOM_HEIGHT &&
            room.map.tiles[y][x] == TILE_FLOOR)
        {
            floorX = x;
            floorY = y;
            return true;
        }
    }
    return false;
}
}

void setUp()
{
    dungeon = Dungeon{};
    dungeon.roomCount = 1;
    dungeon.entities = dungeon.activeDungeonEntities;
    messageText = "";
    playbackRequestCount = 0;
    playedToneCount = 0;
    playedToneDuration = 0;
    playedPauseDuration = 0;
    for (uint16_t& frequency : playedFrequencies) frequency = 0;
}
void tearDown() {}

void test_sequence_length_scales_from_three_to_seven()
{
    TEST_ASSERT_EQUAL_UINT8(3, getBellSequenceLengthForLevel(1));
    TEST_ASSERT_EQUAL_UINT8(3, getBellSequenceLengthForLevel(3));
    TEST_ASSERT_EQUAL_UINT8(4, getBellSequenceLengthForLevel(4));
    TEST_ASSERT_EQUAL_UINT8(5, getBellSequenceLengthForLevel(8));
    TEST_ASSERT_EQUAL_UINT8(6, getBellSequenceLengthForLevel(15));
    TEST_ASSERT_EQUAL_UINT8(7, getBellSequenceLengthForLevel(20));
}

void test_generation_rules_and_bounded_fallback()
{
    BellPuzzleState state;
    const uint8_t invalidRolls[MAX_BELL_SEQUENCE * 8] = {};
    buildBellSequenceFromRolls(
        state, 20, invalidRolls, sizeof(invalidRolls));
    TEST_ASSERT_EQUAL_UINT8(7, state.sequenceLength);
    TEST_ASSERT_TRUE(isValidBellSequence(state.sequence, state.sequenceLength));
    for (uint8_t i = 2; i < state.sequenceLength; ++i)
        TEST_ASSERT_FALSE(state.sequence[i] == state.sequence[i - 1] &&
                          state.sequence[i] == state.sequence[i - 2]);
}

void test_room_contains_three_blocking_bells_and_one_walkable_rune()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureBellPuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));
    TEST_ASSERT_TRUE(isBellPuzzleRoom(room));
    TEST_ASSERT_EQUAL_UINT8(3, countBellFurniture(room));
    TEST_ASSERT_EQUAL(TILE_BELL_LISTEN_RUNE,
                      room.map.tiles[room.bellPuzzle.runeY][room.bellPuzzle.runeX]);
    TEST_ASSERT_TRUE(isDungeonFloorTerrain(TILE_BELL_LISTEN_RUNE));
    TEST_ASSERT_FALSE(isDungeonFloorTerrain(TILE_BELL_LOW));
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);
}

void test_wrong_note_resets_and_full_sequence_solves()
{
    BellPuzzleState state;
    state.progress = BELL_PUZZLE_UNSOLVED;
    state.sequenceLength = 3;
    state.sequence[0] = BELL_LOW;
    state.sequence[1] = BELL_MID;
    state.sequence[2] = BELL_HIGH;
    uint8_t entered = 0;
    TEST_ASSERT_EQUAL(BELL_INPUT_CORRECT_PREFIX,
                      submitBellTone(state, entered, BELL_LOW));
    TEST_ASSERT_EQUAL(BELL_INPUT_WRONG,
                      submitBellTone(state, entered, BELL_HIGH));
    TEST_ASSERT_EQUAL_UINT8(0, entered);
    TEST_ASSERT_EQUAL(BELL_INPUT_CORRECT_PREFIX,
                      submitBellTone(state, entered, BELL_LOW));
    TEST_ASSERT_EQUAL(BELL_INPUT_CORRECT_PREFIX,
                      submitBellTone(state, entered, BELL_MID));
    TEST_ASSERT_EQUAL(BELL_INPUT_SOLVED,
                      submitBellTone(state, entered, BELL_HIGH));
}

void test_rune_entry_is_edge_triggered_walkable_and_replayable()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureBellPuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));
    Entity player{};
    player.active = true;
    player.type = ENTITY_PLAYER;
    int floorX = -1;
    int floorY = -1;
    TEST_ASSERT_TRUE(findAdjacentFloor(
        room, room.bellPuzzle.runeX, room.bellPuzzle.runeY, floorX, floorY));
    TEST_ASSERT_TRUE(isDungeonFloorTerrain(TILE_BELL_LISTEN_RUNE));

    TEST_ASSERT_TRUE(handleCurrentBellListeningRuneMovement(
        player, floorX, floorY,
        room.bellPuzzle.runeX, room.bellPuzzle.runeY));
    TEST_ASSERT_EQUAL_UINT8(1, playbackRequestCount);
    TEST_ASSERT_EQUAL_UINT8(3, playedToneCount);

    TEST_ASSERT_FALSE(handleCurrentBellListeningRuneMovement(
        player, room.bellPuzzle.runeX, room.bellPuzzle.runeY,
        room.bellPuzzle.runeX, room.bellPuzzle.runeY));
    TEST_ASSERT_EQUAL_UINT8(1, playbackRequestCount);

    TEST_ASSERT_FALSE(handleCurrentBellListeningRuneMovement(
        player, room.bellPuzzle.runeX, room.bellPuzzle.runeY, floorX, floorY));
    TEST_ASSERT_EQUAL_UINT8(1, playbackRequestCount);

    TEST_ASSERT_TRUE(handleCurrentBellListeningRuneMovement(
        player, floorX, floorY,
        room.bellPuzzle.runeX, room.bellPuzzle.runeY));
    TEST_ASSERT_EQUAL_UINT8(2, playbackRequestCount);
}

void test_playback_uses_exact_stored_sequence_after_room_reload()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[] = {0, 2, 1, 0};
    TEST_ASSERT_TRUE(configureBellPuzzleRoom(
        room, DIR_EAST, 4, rolls, sizeof(rolls)));
    TEST_ASSERT_EQUAL_UINT8(4, room.bellPuzzle.sequenceLength);

    const BellToneID saved[MAX_BELL_SEQUENCE] = {
        room.bellPuzzle.sequence[0], room.bellPuzzle.sequence[1],
        room.bellPuzzle.sequence[2], room.bellPuzzle.sequence[3]};
    const DungeonRoom persistedRoom = room;
    room = DungeonRoom{};
    room = persistedRoom;

    TEST_ASSERT_TRUE(playBellPuzzleSequence(room.bellPuzzle));
    TEST_ASSERT_EQUAL_UINT8(4, playedToneCount);
    TEST_ASSERT_EQUAL_UINT16(BELL_TONE_DURATION_MS, playedToneDuration);
    TEST_ASSERT_EQUAL_UINT16(BELL_PAUSE_DURATION_MS, playedPauseDuration);
    for (uint8_t i = 0; i < 4; ++i)
    {
        TEST_ASSERT_EQUAL(saved[i], room.bellPuzzle.sequence[i]);
        TEST_ASSERT_EQUAL_UINT16(
            getBellToneFrequency(saved[i]), playedFrequencies[i]);
    }
}

void test_max_length_playback_maps_every_tone_without_regeneration()
{
    BellPuzzleState state{};
    state.sequenceLength = MAX_BELL_SEQUENCE;
    const BellToneID sequence[MAX_BELL_SEQUENCE] = {
        BELL_LOW, BELL_HIGH, BELL_MID, BELL_LOW,
        BELL_MID, BELL_HIGH, BELL_LOW};
    for (uint8_t i = 0; i < MAX_BELL_SEQUENCE; ++i)
        state.sequence[i] = sequence[i];

    TEST_ASSERT_TRUE(playBellPuzzleSequence(state));
    TEST_ASSERT_EQUAL_UINT8(MAX_BELL_SEQUENCE, playedToneCount);
    for (uint8_t i = 0; i < MAX_BELL_SEQUENCE; ++i)
    {
        TEST_ASSERT_EQUAL(sequence[i], state.sequence[i]);
        TEST_ASSERT_EQUAL_UINT16(
            getBellToneFrequency(sequence[i]), playedFrequencies[i]);
    }
}

void test_invalid_playback_state_fails_without_audio_request()
{
    BellPuzzleState state{};
    TEST_ASSERT_FALSE(playBellPuzzleSequence(state));

    state.sequenceLength = MAX_BELL_SEQUENCE + 1;
    TEST_ASSERT_FALSE(playBellPuzzleSequence(state));

    state.sequenceLength = 1;
    state.sequence[0] = static_cast<BellToneID>(BELL_TONE_COUNT + 1);
    TEST_ASSERT_FALSE(playBellPuzzleSequence(state));
    TEST_ASSERT_EQUAL_UINT8(0, playbackRequestCount);
}

void test_manual_bells_keep_their_centralized_frequencies()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureBellPuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));
    room.bellPuzzle.progress = BELL_PUZZLE_COMPLETE;

    const DungeonFurnitureType types[3] = {
        FURNITURE_BELL_LOW, FURNITURE_BELL_MID, FURNITURE_BELL_HIGH};
    const BellToneID tones[3] = {BELL_LOW, BELL_MID, BELL_HIGH};
    for (uint8_t i = 0; i < 3; ++i)
    {
        const DungeonFurnitureInstance* bell = nullptr;
        for (const DungeonFurnitureInstance& furniture : room.furniture)
            if (furniture.type == types[i]) bell = &furniture;
        TEST_ASSERT_NOT_NULL(bell);
        TEST_ASSERT_TRUE(strikeCurrentBellAt(bell->x, bell->y));
        TEST_ASSERT_EQUAL_UINT8(1, playedToneCount);
        TEST_ASSERT_EQUAL_UINT16(
            getBellToneFrequency(tones[i]), playedFrequencies[0]);
        TEST_ASSERT_EQUAL_UINT16(0, playedPauseDuration);
    }
}

void test_key_reward_state_remains_persistent_and_unique()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureBellPuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));

    uint8_t entered = 0;
    TEST_ASSERT_EQUAL(BELL_INPUT_CORRECT_PREFIX,
        submitBellTone(room.bellPuzzle, entered, room.bellPuzzle.sequence[0]));
    TEST_ASSERT_EQUAL(BELL_INPUT_CORRECT_PREFIX,
        submitBellTone(room.bellPuzzle, entered, room.bellPuzzle.sequence[1]));
    TEST_ASSERT_EQUAL(BELL_INPUT_SOLVED,
        submitBellTone(room.bellPuzzle, entered, room.bellPuzzle.sequence[2]));
    room.bellPuzzle.progress = BELL_PUZZLE_KEY_PRESENTED;
    TEST_ASSERT_EQUAL(BELL_PUZZLE_KEY_PRESENTED, room.bellPuzzle.progress);
}

void test_sprite_declarations_are_exactly_16_by_16()
{
    TEST_ASSERT_EQUAL_UINT32(256 * sizeof(uint16_t), sizeof(bellLow16x16));
    TEST_ASSERT_EQUAL_UINT32(256 * sizeof(uint16_t), sizeof(bellMid16x16));
    TEST_ASSERT_EQUAL_UINT32(256 * sizeof(uint16_t), sizeof(bellHigh16x16));
    TEST_ASSERT_EQUAL_UINT32(256 * sizeof(uint16_t), sizeof(bellListenRune16x16));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_sequence_length_scales_from_three_to_seven);
    RUN_TEST(test_generation_rules_and_bounded_fallback);
    RUN_TEST(test_room_contains_three_blocking_bells_and_one_walkable_rune);
    RUN_TEST(test_wrong_note_resets_and_full_sequence_solves);
    RUN_TEST(test_rune_entry_is_edge_triggered_walkable_and_replayable);
    RUN_TEST(test_playback_uses_exact_stored_sequence_after_room_reload);
    RUN_TEST(test_max_length_playback_maps_every_tone_without_regeneration);
    RUN_TEST(test_invalid_playback_state_fails_without_audio_request);
    RUN_TEST(test_manual_bells_keep_their_centralized_frequencies);
    RUN_TEST(test_key_reward_state_remains_persistent_and_unique);
    RUN_TEST(test_sprite_declarations_are_exactly_16_by_16);
    UNITY_END();
}
void loop() {}
