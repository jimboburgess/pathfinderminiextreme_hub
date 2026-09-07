#ifndef PATHFINDERMINIEXTREME_025_NUMBERTILEPUZZLE_H
#define PATHFINDERMINIEXTREME_025_NUMBERTILEPUZZLE_H

#include <stdint.h>

#include "data/game.h"

struct DungeonRoom;
struct Entity;

enum NumberTileRule : uint8_t
{
    NUMBER_RULE_ODD,
    NUMBER_RULE_EVEN,
    NUMBER_RULE_ONLY_THREE,
    NUMBER_RULE_ONLY_SIX,
    NUMBER_RULE_GREATER_THAN_THREE,
    NUMBER_RULE_LESS_THAN_FOUR,
    NUMBER_RULE_MULTIPLE_OF_TWO,
    NUMBER_RULE_MULTIPLE_OF_THREE,
    NUMBER_RULE_PRIME,
    NUMBER_RULE_COMPOSITE,
    NUMBER_RULE_MULTIPLE_OF_FOUR,
    NUMBER_RULE_GREATER_THAN_FIVE,
    NUMBER_RULE_LESS_THAN_FIVE,
    NUMBER_RULE_BETWEEN_THREE_AND_SEVEN,
    NUMBER_RULE_DIVISOR_OF_TWELVE,
    NUMBER_RULE_DIVISOR_OF_EIGHTEEN,
    NUMBER_RULE_SQUARE_IS_EVEN,
    NUMBER_RULE_SQUARE_IS_ODD,
    NUMBER_RULE_EXACT_SEVEN,
    NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10,
    NUMBER_RULE_TRIPLE_PLUS_TWO_MOD_10,
    NUMBER_RULE_SQUARE_PLUS_ONE_MOD_10,
    NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10,
    NUMBER_RULE_ALTERNATE_PLUS_THREE_DOUBLE_MOD_10,
    NUMBER_RULE_DOUBLE_MINUS_ONE_MOD_10,
    NUMBER_RULE_COUNT
};

enum NumberPuzzleDifficulty : uint8_t
{
    NUMBER_DIFFICULTY_EASY,
    NUMBER_DIFFICULTY_MEDIUM,
    NUMBER_DIFFICULTY_HARD
};

enum NumberTileVisualState : uint8_t
{
    NUMBER_TILE_IDLE,
    NUMBER_TILE_SAFE_ACTIVE,
    NUMBER_TILE_FAILED
};

enum NumberRuleEvaluationMode : uint8_t
{
    NUMBER_RULE_STATELESS,
    NUMBER_RULE_STATEFUL
};

enum NumberPuzzleProgress : uint8_t
{
    NUMBER_PUZZLE_NONE,
    NUMBER_PUZZLE_UNSOLVED,
    NUMBER_PUZZLE_COMPLETE
};

struct NumberTileRuleDefinition
{
    NumberTileRule id;
    const char* clues[2];
    uint8_t clueCount;
    NumberPuzzleDifficulty difficulty;
    uint8_t minimumLevel;
    uint8_t maximumLevel;
    NumberRuleEvaluationMode evaluationMode;
};

// Reserved context parameters make stateful Stage 2 evaluators possible
// without changing movement call sites. Stage 1 rules use only currentDigit.
struct NumberPuzzleEvaluationContext
{
    uint8_t currentDigit = 0;
    uint8_t previousDigit = 0;
    uint8_t stepIndex = 0;
};

constexpr uint8_t NUMBER_FIELD_STORAGE_WIDTH = 8;
constexpr uint8_t NUMBER_FIELD_STORAGE_HEIGHT = 8;
constexpr uint8_t NUMBER_FIELD_TILE_CAPACITY =
    NUMBER_FIELD_STORAGE_WIDTH * NUMBER_FIELD_STORAGE_HEIGHT;
constexpr uint8_t NUMBER_STAGE_ONE_MIN_DIGIT = 1;
constexpr uint8_t NUMBER_STAGE_ONE_MAX_DIGIT = 6;
constexpr uint8_t NUMBER_STATEFUL_MAX_STEPS = 8;

struct NumberTilePuzzleState
{
    uint8_t digits[NUMBER_FIELD_TILE_CAPACITY] = {};
    NumberTileRule rule = NUMBER_RULE_ODD;
    uint8_t clueVariant = 0;
    uint8_t minimumDigit = NUMBER_STAGE_ONE_MIN_DIGIT;
    uint8_t maximumDigit = NUMBER_STAGE_ONE_MAX_DIGIT;
    NumberPuzzleProgress progress = NUMBER_PUZZLE_NONE;
    uint8_t lockedExitDirection = DIR_NORTH;
    uint8_t fieldX = 0;
    uint8_t fieldY = 0;
    uint8_t fieldWidth = 0;
    uint8_t fieldHeight = 0;
    int8_t clueX = -1;
    int8_t clueY = -1;
    int8_t failedX = -1;
    int8_t failedY = -1;
    uint8_t seedA = 0;
    uint8_t seedB = 0;
    uint8_t requiredSequence[NUMBER_STATEFUL_MAX_STEPS] = {};
    uint8_t requiredLength = 0;
};

static_assert(sizeof(NumberTilePuzzleState) == 89,
              "Number puzzle blueprint must remain compact");

const NumberTileRuleDefinition* getNumberTileRuleDefinition(NumberTileRule rule);
const char* getNumberTileClue(const NumberTilePuzzleState& state);
void getNumberPuzzleDigitRange(uint8_t level, uint8_t& minimum, uint8_t& maximum);
bool isNumberTileRuleEligible(
    NumberTileRule rule, uint8_t level, uint8_t minimumDigit, uint8_t maximumDigit);
NumberTileRule selectNumberTileRuleForLevel(
    uint8_t level, uint8_t minimumDigit, uint8_t maximumDigit,
    uint8_t modeRoll, uint8_t ruleRoll);
uint8_t getStatefulNumberRuleChancePercent(uint8_t level);
uint8_t normalizeNumberPuzzleDigit(int value);
uint8_t getExpectedNextNumberPuzzleDigit(
    NumberTileRule rule, const NumberPuzzleEvaluationContext& context);
bool isNumberTileSafe(NumberTileRule rule, uint8_t digit);
bool evaluateNumberTileRule(
    NumberTileRule rule, const NumberPuzzleEvaluationContext& context);
uint8_t getNumberPuzzleDigitAt(const DungeonRoom& room, int x, int y);
bool validateNumberPuzzleSafePath(const DungeonRoom& room);

bool configureNumberTilePuzzleRoom(
    DungeonRoom& room,
    Direction lockedExitDirection,
    uint8_t level,
    const uint8_t* rolls,
    uint16_t rollCount);
bool isNumberTilePuzzleRoom(const DungeonRoom& room);
bool interactWithCurrentNumberPuzzleClue(int x, int y);
bool tryEnterCurrentNumberPuzzleTile(Entity& player, int x, int y);
void handleCurrentNumberPuzzleMovement(
    Entity& player, int previousX, int previousY);
bool tryUnlockCurrentNumberPuzzleExit(Direction direction);
NumberTileVisualState getNumberTileVisualState(
    const DungeonRoom& room, int x, int y, const Entity* player);

#endif
