#include <Arduino.h>
#include <unity.h>

#include "dungeon/dungeon.h"
#include "dungeon/numbertilepuzzle.h"

Dungeon dungeon{};
namespace { const char* messageText = ""; }

const RoomConnection* getRoomConnection(const DungeonRoom& room, Direction direction)
{
    for (uint8_t i = 0; i < room.connectionCount; ++i)
        if (room.connections[i].direction == direction) return &room.connections[i];
    return nullptr;
}
bool validateRoomConnectivity(const DungeonRoom&) { return true; }
void markTileDirty(int, int) {}
void setGameMessage(const char* message) { messageText = message; }

#include "../../src/dungeon/numbertilepuzzle.cpp"

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

void configureTestRoom(DungeonRoom& room)
{
    prepareRoom(room);
    uint8_t rolls[128];
    for (uint8_t i = 0; i < sizeof(rolls); ++i)
        rolls[i] = static_cast<uint8_t>(i * 37 + 11);
    TEST_ASSERT_TRUE(configureNumberTilePuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));
}
}

void setUp()
{
    dungeon = Dungeon{};
    dungeon.roomCount = 1;
    dungeon.currentRoom = 0;
    messageText = "";
}
void tearDown() {}

void test_every_stage_one_rule_for_digits_one_through_six()
{
    const bool expected[NUMBER_RULE_COUNT][10] = {
        {false,true,false,true,false,true,false,true,false,true},
        {true,false,true,false,true,false,true,false,true,false},
        {false,false,false,true,false,false,false,false,false,false},
        {false,false,false,false,false,false,true,false,false,false},
        {false,false,false,false,true,true,true,true,true,true},
        {true,true,true,true,false,false,false,false,false,false},
        {true,false,true,false,true,false,true,false,true,false},
        {true,false,false,true,false,false,true,false,false,true},
        {false,false,true,true,false,true,false,true,false,false},
        {false,false,false,false,true,false,true,false,true,true},
        {true,false,false,false,true,false,false,false,true,false},
        {false,false,false,false,false,false,true,true,true,true},
        {true,true,true,true,true,false,false,false,false,false},
        {false,false,false,true,true,true,true,true,false,false},
        {false,true,true,true,true,false,true,false,false,false},
        {false,true,true,true,false,false,true,false,false,true},
        {true,false,true,false,true,false,true,false,true,false},
        {false,true,false,true,false,true,false,true,false,true},
        {false,false,false,false,false,false,false,true,false,false}};
    for (uint8_t rule = 0; rule < NUMBER_RULE_COUNT; ++rule)
    {
        TEST_ASSERT_NOT_NULL(getNumberTileRuleDefinition(
            static_cast<NumberTileRule>(rule)));
        for (uint8_t digit = 0; digit <= 9; ++digit)
            TEST_ASSERT_EQUAL(expected[rule][digit],
                isNumberTileSafe(static_cast<NumberTileRule>(rule), digit));
    }
}

void test_digit_ranges_and_rule_eligibility_scale_by_level()
{
    uint8_t minimum = 0;
    uint8_t maximum = 0;
    getNumberPuzzleDigitRange(1, minimum, maximum);
    TEST_ASSERT_EQUAL_UINT8(1, minimum);
    TEST_ASSERT_EQUAL_UINT8(6, maximum);
    TEST_ASSERT_FALSE(isNumberTileRuleEligible(
        NUMBER_RULE_PRIME, 1, minimum, maximum));
    TEST_ASSERT_FALSE(isNumberTileRuleEligible(
        NUMBER_RULE_EXACT_SEVEN, 1, minimum, maximum));

    getNumberPuzzleDigitRange(5, minimum, maximum);
    TEST_ASSERT_EQUAL_UINT8(0, minimum);
    TEST_ASSERT_EQUAL_UINT8(7, maximum);
    TEST_ASSERT_TRUE(isNumberTileRuleEligible(
        NUMBER_RULE_PRIME, 5, minimum, maximum));
    TEST_ASSERT_FALSE(isNumberTileRuleEligible(
        NUMBER_RULE_COMPOSITE, 5, minimum, maximum));

    getNumberPuzzleDigitRange(10, minimum, maximum);
    TEST_ASSERT_EQUAL_UINT8(0, minimum);
    TEST_ASSERT_EQUAL_UINT8(9, maximum);
    TEST_ASSERT_TRUE(isNumberTileRuleEligible(
        NUMBER_RULE_COMPOSITE, 10, minimum, maximum));
    TEST_ASSERT_TRUE(isNumberTileRuleEligible(
        NUMBER_RULE_EXACT_SEVEN, 10, minimum, maximum));
}

void test_prime_composite_and_zero_semantics()
{
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_PRIME, 0));
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_PRIME, 1));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_PRIME, 2));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_PRIME, 3));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_PRIME, 5));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_PRIME, 7));
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 0));
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 1));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 4));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 6));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 8));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_COMPOSITE, 9));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_EVEN, 0));
    TEST_ASSERT_TRUE(isNumberTileSafe(NUMBER_RULE_MULTIPLE_OF_THREE, 0));
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_DIVISOR_OF_TWELVE, 0));
    TEST_ASSERT_FALSE(isNumberTileSafe(NUMBER_RULE_DIVISOR_OF_EIGHTEEN, 0));
}

void test_generation_uses_digits_one_to_six_and_has_safe_path()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureTestRoom(room);
    TEST_ASSERT_TRUE(isNumberTilePuzzleRoom(room));
    TEST_ASSERT_EQUAL(PUZZLE_NUMBER_TILES, room.puzzleType);
    TEST_ASSERT_TRUE(validateNumberPuzzleSafePath(room));
    bool safeDistractor = false;
    bool unsafeDistractor = false;
    for (uint8_t y = 0; y < room.numberPuzzle.fieldHeight; ++y)
        for (uint8_t x = 0; x < room.numberPuzzle.fieldWidth; ++x)
        {
            const uint8_t digit = room.numberPuzzle.digits[
                y * NUMBER_FIELD_STORAGE_WIDTH + x];
            TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, digit);
            TEST_ASSERT_LESS_OR_EQUAL_UINT8(6, digit);
            safeDistractor |= isNumberTileSafe(room.numberPuzzle.rule, digit);
            unsafeDistractor |= !isNumberTileSafe(room.numberPuzzle.rule, digit);
        }
    TEST_ASSERT_TRUE(safeDistractor);
    TEST_ASSERT_TRUE(unsafeDistractor);
    TEST_ASSERT_EQUAL(NPC_NONE, room.npcSpawn.id);
}

void test_invalid_layout_shape_is_rejected_without_unbounded_retry()
{
    DungeonRoom room;
    prepareRoom(room);
    room.connectionCount = 1;
    const uint8_t rolls[] = {0, 1, 2};
    TEST_ASSERT_FALSE(configureNumberTilePuzzleRoom(
        room, DIR_EAST, 1, rolls, sizeof(rolls)));
}

void test_safe_and_unsafe_movement_preserve_character_and_layout()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureTestRoom(room);
    Entity player{};
    player.active = true;
    player.type = ENTITY_PLAYER;
    player.character.health.currentHP = 12;
    const NumberTileRule savedRule = room.numberPuzzle.rule;
    uint8_t savedDigits[NUMBER_FIELD_TILE_CAPACITY];
    memcpy(savedDigits, room.numberPuzzle.digits, sizeof(savedDigits));
    int safeX = -1, safeY = -1, unsafeX = -1, unsafeY = -1;
    for (uint8_t y = 0; y < room.numberPuzzle.fieldHeight; ++y)
        for (uint8_t x = 0; x < room.numberPuzzle.fieldWidth; ++x)
        {
            const int worldX = room.numberPuzzle.fieldX + x;
            const int worldY = room.numberPuzzle.fieldY + y;
            const bool safe = isNumberTileSafe(room.numberPuzzle.rule,
                getNumberPuzzleDigitAt(room, worldX, worldY));
            if (safe && worldX == room.numberPuzzle.fieldX && safeX < 0)
                { safeX = worldX; safeY = worldY; }
            if (!safe && unsafeX < 0) { unsafeX = worldX; unsafeY = worldY; }
        }
    TEST_ASSERT_TRUE(tryEnterCurrentNumberPuzzleTile(player, safeX, safeY));
    TEST_ASSERT_FALSE(tryEnterCurrentNumberPuzzleTile(player, unsafeX, unsafeY));
    TEST_ASSERT_EQUAL_INT(12, player.character.health.currentHP);
    TEST_ASSERT_EQUAL(savedRule, room.numberPuzzle.rule);
    TEST_ASSERT_EQUAL_MEMORY(savedDigits, room.numberPuzzle.digits,
                             sizeof(savedDigits));
    TEST_ASSERT_EQUAL(NUMBER_PUZZLE_UNSOLVED, room.numberPuzzle.progress);
}

void test_crossing_completion_directly_unlocks_once()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureTestRoom(room);
    Entity player{};
    player.active = true;
    player.type = ENTITY_PLAYER;
    dungeon.roomRuntime[0].numberCrossingActive = true;
    const int previousX = room.numberPuzzle.fieldX + room.numberPuzzle.fieldWidth - 1;
    const int previousY = room.numberPuzzle.fieldY;
    player.x = previousX + 1;
    player.y = previousY;
    handleCurrentNumberPuzzleMovement(player, previousX, previousY);
    TEST_ASSERT_EQUAL(NUMBER_PUZZLE_COMPLETE, room.numberPuzzle.progress);
    TEST_ASSERT_TRUE(room.completed);
    TEST_ASSERT_TRUE(tryUnlockCurrentNumberPuzzleExit(DIR_EAST));
    TEST_ASSERT_TRUE(tryUnlockCurrentNumberPuzzleExit(DIR_EAST));
}

void test_clue_is_repeatable_and_rule_state_persists()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureTestRoom(room);
    const NumberTileRule savedRule = room.numberPuzzle.rule;
    TEST_ASSERT_TRUE(interactWithCurrentNumberPuzzleClue(
        room.numberPuzzle.clueX, room.numberPuzzle.clueY));
    TEST_ASSERT_EQUAL_STRING(getNumberTileClue(room.numberPuzzle), messageText);
    messageText = "";
    TEST_ASSERT_TRUE(interactWithCurrentNumberPuzzleClue(
        room.numberPuzzle.clueX, room.numberPuzzle.clueY));
    TEST_ASSERT_EQUAL(savedRule, room.numberPuzzle.rule);
}

void test_renderer_contract_supports_digits_zero_through_nine()
{
    DungeonRoom room{};
    room.numberPuzzle.fieldX = 1;
    room.numberPuzzle.fieldY = 1;
    room.numberPuzzle.fieldWidth = 7;
    room.numberPuzzle.fieldHeight = 2;
    for (uint8_t digit = 0; digit <= 9; ++digit)
    {
        const int x = 1 + digit % 7;
        const int y = 1 + digit / 7;
        room.numberPuzzle.digits[(y - 1) * NUMBER_FIELD_STORAGE_WIDTH + x - 1] = digit;
        TEST_ASSERT_EQUAL_UINT8(digit, getNumberPuzzleDigitAt(room, x, y));
    }
}

void test_modulo_normalization_and_stateful_evaluators()
{
    TEST_ASSERT_EQUAL_UINT8(1, normalizeNumberPuzzleDigit(11));
    TEST_ASSERT_EQUAL_UINT8(0, normalizeNumberPuzzleDigit(20));
    TEST_ASSERT_EQUAL_UINT8(9, normalizeNumberPuzzleDigit(-1));
    TEST_ASSERT_EQUAL_UINT8(9, normalizeNumberPuzzleDigit(-11));
    NumberPuzzleEvaluationContext context{};
    context.currentDigit = 4;
    TEST_ASSERT_EQUAL_UINT8(1, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10, context));
    TEST_ASSERT_EQUAL_UINT8(4, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_TRIPLE_PLUS_TWO_MOD_10, context));
    TEST_ASSERT_EQUAL_UINT8(7, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_SQUARE_PLUS_ONE_MOD_10, context));
    context.previousDigit = 7;
    TEST_ASSERT_EQUAL_UINT8(1, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10, context));
    context.currentDigit = 0;
    TEST_ASSERT_EQUAL_UINT8(9, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_DOUBLE_MINUS_ONE_MOD_10, context));
    context.currentDigit = 4;
    context.stepIndex = 0;
    TEST_ASSERT_EQUAL_UINT8(7, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_ALTERNATE_PLUS_THREE_DOUBLE_MOD_10, context));
    context.stepIndex = 1;
    TEST_ASSERT_EQUAL_UINT8(8, getExpectedNextNumberPuzzleDigit(
        NUMBER_RULE_ALTERNATE_PLUS_THREE_DOUBLE_MOD_10, context));
}

void test_stateful_level_weight_and_eligibility()
{
    TEST_ASSERT_EQUAL_UINT8(0, getStatefulNumberRuleChancePercent(9));
    TEST_ASSERT_EQUAL_UINT8(25, getStatefulNumberRuleChancePercent(10));
    TEST_ASSERT_EQUAL_UINT8(40, getStatefulNumberRuleChancePercent(14));
    TEST_ASSERT_EQUAL_UINT8(50, getStatefulNumberRuleChancePercent(18));
    TEST_ASSERT_FALSE(isNumberTileRuleEligible(
        NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10, 9, 0, 9));
    TEST_ASSERT_TRUE(isNumberTileRuleEligible(
        NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10, 10, 0, 9));
    TEST_ASSERT_FALSE(isNumberTileRuleEligible(
        NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10, 17, 0, 9));
    TEST_ASSERT_TRUE(isNumberTileRuleEligible(
        NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10, 18, 0, 9));
    const NumberTileRule selected = selectNumberTileRuleForLevel(20, 0, 9, 0, 0);
    TEST_ASSERT_EQUAL(NUMBER_RULE_STATEFUL,
        getNumberTileRuleDefinition(selected)->evaluationMode);
}

void test_stateful_room_generation_is_persistent_and_solvable()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    uint8_t rolls[160];
    for (uint16_t i = 0; i < sizeof(rolls); ++i)
        rolls[i] = static_cast<uint8_t>(i * 29 + 7);
    rolls[0] = 0;
    rolls[1] = 0;
    TEST_ASSERT_TRUE(configureNumberTilePuzzleRoom(
        room, DIR_EAST, 20, rolls, sizeof(rolls)));
    TEST_ASSERT_EQUAL(NUMBER_RULE_STATEFUL,
        getNumberTileRuleDefinition(room.numberPuzzle.rule)->evaluationMode);
    TEST_ASSERT_TRUE(room.numberPuzzle.requiredLength >= 6);
    TEST_ASSERT_TRUE(room.numberPuzzle.requiredLength <= 8);
    TEST_ASSERT_TRUE(validateNumberPuzzleSafePath(room));
    const NumberTilePuzzleState saved = room.numberPuzzle;
    for (uint8_t i = 0; i < saved.requiredLength; ++i)
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(9, saved.requiredSequence[i]);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &room.numberPuzzle, sizeof(saved));
}

void test_stateful_movement_is_value_based_and_wrong_step_resets()
{
    DungeonRoom& room = dungeon.rooms[0];
    configureTestRoom(room);
    room.numberPuzzle.rule = NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10;
    room.numberPuzzle.seedA = 0;
    room.numberPuzzle.requiredLength = 2;
    room.numberPuzzle.requiredSequence[0] = 3;
    room.numberPuzzle.requiredSequence[1] = 9;
    const int x = room.numberPuzzle.fieldX;
    const int y = room.numberPuzzle.fieldY;
    room.numberPuzzle.digits[0] = 3;
    room.numberPuzzle.digits[1 * NUMBER_FIELD_STORAGE_WIDTH] = 3;
    Entity player{};
    player.active = true;
    player.type = ENTITY_PLAYER;
    player.x = x - 1;
    player.y = y;
    TEST_ASSERT_TRUE(tryEnterCurrentNumberPuzzleTile(player, x, y));
    dungeon.roomRuntime[0].numberCrossingActive = false;
    dungeon.roomRuntime[0].numberCurrentStep = 0;
    TEST_ASSERT_TRUE(tryEnterCurrentNumberPuzzleTile(player, x, y + 1));
    room.numberPuzzle.digits[1 * NUMBER_FIELD_STORAGE_WIDTH + 1] = 8;
    player.x = x;
    player.y = y + 1;
    TEST_ASSERT_FALSE(tryEnterCurrentNumberPuzzleTile(player, x + 1, y + 1));
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.roomRuntime[0].numberCurrentStep);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[0].numberCrossingActive);
    TEST_ASSERT_EQUAL_STRING("The sequence breaks.", messageText);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_every_stage_one_rule_for_digits_one_through_six);
    RUN_TEST(test_digit_ranges_and_rule_eligibility_scale_by_level);
    RUN_TEST(test_prime_composite_and_zero_semantics);
    RUN_TEST(test_generation_uses_digits_one_to_six_and_has_safe_path);
    RUN_TEST(test_invalid_layout_shape_is_rejected_without_unbounded_retry);
    RUN_TEST(test_safe_and_unsafe_movement_preserve_character_and_layout);
    RUN_TEST(test_crossing_completion_directly_unlocks_once);
    RUN_TEST(test_clue_is_repeatable_and_rule_state_persists);
    RUN_TEST(test_renderer_contract_supports_digits_zero_through_nine);
    RUN_TEST(test_modulo_normalization_and_stateful_evaluators);
    RUN_TEST(test_stateful_level_weight_and_eligibility);
    RUN_TEST(test_stateful_room_generation_is_persistent_and_solvable);
    RUN_TEST(test_stateful_movement_is_value_based_and_wrong_step_resets);
    UNITY_END();
}
void loop() {}
