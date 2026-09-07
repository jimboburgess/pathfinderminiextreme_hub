#include "dungeon/numbertilepuzzle.h"

#include <Arduino.h>
#include <stdio.h>

#include "data/entities.h"
#include "dungeon/dungeon.h"
#include "dungeon/roomgen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"

namespace
{
const NumberTileRuleDefinition RULES[NUMBER_RULE_COUNT] = {
    {NUMBER_RULE_ODD, {"Only the odd may bear your weight.", "Trust the numbers without pairs."}, 2, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_EVEN, {"Walk where every number has a pair.", "Only even stones will hold."}, 2, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_ONLY_THREE, {"Three alone knows the way.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_ONLY_SIX, {"Six alone will carry you.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_GREATER_THAN_THREE, {"Only numbers greater than three are safe.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_LESS_THAN_FOUR, {"Stay below four.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_MULTIPLE_OF_TWO, {"Two marks every safe stone.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_MULTIPLE_OF_THREE, {"Three marks every safe stone.", nullptr}, 1, NUMBER_DIFFICULTY_EASY, 1, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_PRIME, {"Greater than one, divisible only by one and itself.", "Walk upon the indivisible."}, 2, NUMBER_DIFFICULTY_MEDIUM, 5, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_COMPOSITE, {"Choose numbers built from smaller factors.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_MULTIPLE_OF_FOUR, {"Four marks every safe stone.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 5, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_GREATER_THAN_FIVE, {"Only numbers greater than five will hold.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 5, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_LESS_THAN_FIVE, {"Keep every step below five.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 5, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_BETWEEN_THREE_AND_SEVEN, {"Stay between three and seven, inclusive.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 5, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_DIVISOR_OF_TWELVE, {"Use numbers that divide twelve without remainder.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_DIVISOR_OF_EIGHTEEN, {"Use numbers that divide eighteen without remainder.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_SQUARE_IS_EVEN, {"Only numbers whose square is even are safe.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_SQUARE_IS_ODD, {"Only numbers whose square is odd are safe.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_EXACT_SEVEN, {"Seven alone will carry you.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 10, 20, NUMBER_RULE_STATELESS},
    {NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10, {"Double the number beneath you, add three, and keep only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 10, 20, NUMBER_RULE_STATEFUL},
    {NUMBER_RULE_TRIPLE_PLUS_TWO_MOD_10, {"Triple your number, add two, and keep only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 14, 20, NUMBER_RULE_STATEFUL},
    {NUMBER_RULE_SQUARE_PLUS_ONE_MOD_10, {"Square your number, add one, and keep only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 14, 20, NUMBER_RULE_STATEFUL},
    {NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10, {"Add your last two numbers. Keep only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 18, 20, NUMBER_RULE_STATEFUL},
    {NUMBER_RULE_ALTERNATE_PLUS_THREE_DOUBLE_MOD_10, {"Add three, then double. Repeat, keeping only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_HARD, 14, 20, NUMBER_RULE_STATEFUL},
    {NUMBER_RULE_DOUBLE_MINUS_ONE_MOD_10, {"Double the number, subtract one, and keep only the final digit.", nullptr}, 1, NUMBER_DIFFICULTY_MEDIUM, 10, 20, NUMBER_RULE_STATEFUL}
};

uint8_t digitIndex(const NumberTilePuzzleState& state, int x, int y)
{
    return static_cast<uint8_t>(
        (y - state.fieldY) * NUMBER_FIELD_STORAGE_WIDTH + (x - state.fieldX));
}

bool isInField(const NumberTilePuzzleState& state, int x, int y)
{
    return x >= state.fieldX && x < state.fieldX + state.fieldWidth &&
           y >= state.fieldY && y < state.fieldY + state.fieldHeight;
}

bool isHorizontalCrossing(Direction exitDirection)
{
    return exitDirection == DIR_EAST || exitDirection == DIR_WEST;
}

bool isStartEdge(const NumberTilePuzzleState& state, int x, int y)
{
    if (state.lockedExitDirection == DIR_EAST) return x == state.fieldX;
    if (state.lockedExitDirection == DIR_WEST)
        return x == state.fieldX + state.fieldWidth - 1;
    if (state.lockedExitDirection == DIR_SOUTH) return y == state.fieldY;
    return y == state.fieldY + state.fieldHeight - 1;
}

bool isFarEdge(const NumberTilePuzzleState& state, int x, int y)
{
    if (state.lockedExitDirection == DIR_EAST)
        return x == state.fieldX + state.fieldWidth - 1;
    if (state.lockedExitDirection == DIR_WEST) return x == state.fieldX;
    if (state.lockedExitDirection == DIR_SOUTH)
        return y == state.fieldY + state.fieldHeight - 1;
    return y == state.fieldY;
}

uint8_t chooseDigit(NumberTileRule rule, bool safe, uint8_t minimumDigit,
                    uint8_t maximumDigit, uint8_t roll)
{
    uint8_t choices[10] = {};
    uint8_t count = 0;
    for (uint8_t digit = minimumDigit; digit <= maximumDigit; ++digit)
        if (isNumberTileSafe(rule, digit) == safe) choices[count++] = digit;
    return count == 0 ? minimumDigit : choices[roll % count];
}

bool hasOppositePair(const DungeonRoom& room, Direction exitDirection)
{
    Direction entrance = DIR_SOUTH;
    if (exitDirection == DIR_NORTH) entrance = DIR_SOUTH;
    else if (exitDirection == DIR_SOUTH) entrance = DIR_NORTH;
    else if (exitDirection == DIR_EAST) entrance = DIR_WEST;
    else if (exitDirection == DIR_WEST) entrance = DIR_EAST;
    return room.connectionCount == 2 &&
        getRoomConnection(room, exitDirection) != nullptr &&
        getRoomConnection(room, entrance) != nullptr;
}

void makeCrossingBarriers(DungeonRoom& room, const NumberTilePuzzleState& state)
{
    if (isHorizontalCrossing(static_cast<Direction>(state.lockedExitDirection)))
    {
        const int left = state.fieldX - 1;
        const int right = state.fieldX + state.fieldWidth;
        for (int y = 1; y < ROOM_HEIGHT - 1; ++y)
            if (y < state.fieldY || y >= state.fieldY + state.fieldHeight)
            {
                room.map.tiles[y][left] = TILE_WALL;
                room.map.tiles[y][right] = TILE_WALL;
            }
    }
    else
    {
        const int top = state.fieldY - 1;
        const int bottom = state.fieldY + state.fieldHeight;
        for (int x = 1; x < ROOM_WIDTH - 1; ++x)
            if (x < state.fieldX || x >= state.fieldX + state.fieldWidth)
            {
                room.map.tiles[top][x] = TILE_WALL;
                room.map.tiles[bottom][x] = TILE_WALL;
            }
    }
}

bool isStateful(NumberTileRule rule)
{
    const NumberTileRuleDefinition* definition = getNumberTileRuleDefinition(rule);
    return definition != nullptr && definition->evaluationMode == NUMBER_RULE_STATEFUL;
}

uint8_t statefulLengthForLevel(uint8_t level, uint8_t roll)
{
    if (level <= 13) return 4;
    if (level <= 17) return static_cast<uint8_t>(5 + (roll & 1));
    return static_cast<uint8_t>(6 + (roll % 3));
}

bool buildStatefulSequence(NumberTilePuzzleState& state, uint8_t length)
{
    NumberPuzzleEvaluationContext context;
    context.previousDigit = state.seedA;
    context.currentDigit = state.rule == NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10
        ? state.seedB : state.seedA;
    for (uint8_t step = 0; step < length; ++step)
    {
        context.stepIndex = step;
        const uint8_t next = getExpectedNextNumberPuzzleDigit(state.rule, context);
        state.requiredSequence[step] = next;
        context.previousDigit = context.currentDigit;
        context.currentDigit = next;
    }
    state.requiredLength = length;
    bool allSame = true;
    bool periodTwo = length >= 4;
    for (uint8_t i = 1; i < length; ++i)
        allSame &= state.requiredSequence[i] == state.requiredSequence[0];
    for (uint8_t i = 2; i < length; ++i)
        periodTwo &= state.requiredSequence[i] == state.requiredSequence[i % 2];
    return !allSame && !periodTwo;
}

void resetStatefulTraversal(DungeonRoomRuntime& runtime,
                            const NumberTilePuzzleState& state)
{
    runtime.numberCrossingActive = false;
    runtime.numberCurrentStep = 0;
    runtime.numberPreviousDigit = state.seedA;
    runtime.numberCurrentDigit =
        state.rule == NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10
            ? state.seedB : state.seedA;
}

void movePlayerToStartLanding(Entity& player, const NumberTilePuzzleState& state)
{
    const int oldX = player.x;
    const int oldY = player.y;
    if (state.lockedExitDirection == DIR_EAST)
    { player.x = state.fieldX - 1; player.y = state.fieldY + state.fieldHeight / 2; }
    else if (state.lockedExitDirection == DIR_WEST)
    { player.x = state.fieldX + state.fieldWidth; player.y = state.fieldY + state.fieldHeight / 2; }
    else if (state.lockedExitDirection == DIR_SOUTH)
    { player.x = state.fieldX + state.fieldWidth / 2; player.y = state.fieldY - 1; }
    else
    { player.x = state.fieldX + state.fieldWidth / 2; player.y = state.fieldY + state.fieldHeight; }
    markTileDirty(oldX, oldY);
    markTileDirty(player.x, player.y);
}
}

const NumberTileRuleDefinition* getNumberTileRuleDefinition(NumberTileRule rule)
{
    return rule < NUMBER_RULE_COUNT ? &RULES[rule] : nullptr;
}

const char* getNumberTileClue(const NumberTilePuzzleState& state)
{
    const NumberTileRuleDefinition* definition =
        getNumberTileRuleDefinition(state.rule);
    if (definition == nullptr || definition->clueCount == 0) return "";
    const uint8_t variant = state.clueVariant < definition->clueCount
        ? state.clueVariant : 0;
    return definition->clues[variant];
}

void getNumberPuzzleDigitRange(uint8_t level, uint8_t& minimum, uint8_t& maximum)
{
    if (level <= 4) { minimum = 1; maximum = 6; }
    else if (level <= 9) { minimum = 0; maximum = 7; }
    else { minimum = 0; maximum = 9; }
}

bool isNumberTileRuleEligible(NumberTileRule rule, uint8_t level,
                              uint8_t minimumDigit, uint8_t maximumDigit)
{
    const NumberTileRuleDefinition* definition = getNumberTileRuleDefinition(rule);
    if (definition == nullptr ||
        level < definition->minimumLevel || level > definition->maximumLevel ||
        minimumDigit > maximumDigit || maximumDigit > 9) return false;
    if (definition->evaluationMode == NUMBER_RULE_STATEFUL)
        return minimumDigit == 0 && maximumDigit == 9 && level >= 10;
    uint8_t safeCount = 0;
    uint8_t unsafeCount = 0;
    for (uint8_t digit = minimumDigit; digit <= maximumDigit; ++digit)
        isNumberTileSafe(rule, digit) ? ++safeCount : ++unsafeCount;
    return safeCount > 0 && unsafeCount > 0;
}

uint8_t getStatefulNumberRuleChancePercent(uint8_t level)
{
    if (level < 10) return 0;
    if (level <= 13) return 25;
    if (level <= 17) return 40;
    return 50;
}

NumberTileRule selectNumberTileRuleForLevel(uint8_t level,
                                             uint8_t minimumDigit,
                                             uint8_t maximumDigit,
                                             uint8_t modeRoll,
                                             uint8_t ruleRoll)
{
    NumberTileRule eligible[NUMBER_RULE_COUNT] = {};
    uint8_t count = 0;
    for (uint8_t value = 0; value < NUMBER_RULE_COUNT; ++value)
    {
        const NumberTileRule rule = static_cast<NumberTileRule>(value);
        const NumberTileRuleDefinition* definition = getNumberTileRuleDefinition(rule);
        const bool wantStateful = modeRoll % 100 < getStatefulNumberRuleChancePercent(level);
        if (definition != nullptr &&
            (definition->evaluationMode == NUMBER_RULE_STATEFUL) == wantStateful &&
            isNumberTileRuleEligible(rule, level, minimumDigit, maximumDigit))
            eligible[count++] = rule;
    }
    return count == 0 ? NUMBER_RULE_ODD : eligible[ruleRoll % count];
}

uint8_t normalizeNumberPuzzleDigit(int value)
{
    const int remainder = value % 10;
    return static_cast<uint8_t>(remainder < 0 ? remainder + 10 : remainder);
}

uint8_t getExpectedNextNumberPuzzleDigit(
    NumberTileRule rule, const NumberPuzzleEvaluationContext& context)
{
    switch (rule)
    {
        case NUMBER_RULE_DOUBLE_PLUS_THREE_MOD_10:
            return normalizeNumberPuzzleDigit(2 * context.currentDigit + 3);
        case NUMBER_RULE_TRIPLE_PLUS_TWO_MOD_10:
            return normalizeNumberPuzzleDigit(3 * context.currentDigit + 2);
        case NUMBER_RULE_SQUARE_PLUS_ONE_MOD_10:
            return normalizeNumberPuzzleDigit(context.currentDigit * context.currentDigit + 1);
        case NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10:
            return normalizeNumberPuzzleDigit(context.previousDigit + context.currentDigit);
        case NUMBER_RULE_ALTERNATE_PLUS_THREE_DOUBLE_MOD_10:
            return normalizeNumberPuzzleDigit((context.stepIndex & 1)
                ? 2 * context.currentDigit : context.currentDigit + 3);
        case NUMBER_RULE_DOUBLE_MINUS_ONE_MOD_10:
            return normalizeNumberPuzzleDigit(2 * static_cast<int>(context.currentDigit) - 1);
        default: return 255;
    }
}

bool isNumberTileSafe(NumberTileRule rule, uint8_t digit)
{
    NumberPuzzleEvaluationContext context;
    context.currentDigit = digit;
    return evaluateNumberTileRule(rule, context);
}

bool evaluateNumberTileRule(NumberTileRule rule,
                            const NumberPuzzleEvaluationContext& context)
{
    const uint8_t digit = context.currentDigit;
    switch (rule)
    {
        case NUMBER_RULE_ODD: return digit % 2 == 1;
        case NUMBER_RULE_EVEN:
        case NUMBER_RULE_MULTIPLE_OF_TWO: return digit % 2 == 0;
        case NUMBER_RULE_ONLY_THREE: return digit == 3;
        case NUMBER_RULE_ONLY_SIX: return digit == 6;
        case NUMBER_RULE_GREATER_THAN_THREE: return digit > 3;
        case NUMBER_RULE_LESS_THAN_FOUR: return digit < 4;
        case NUMBER_RULE_MULTIPLE_OF_THREE: return digit % 3 == 0;
        case NUMBER_RULE_PRIME:
            return digit == 2 || digit == 3 || digit == 5 || digit == 7;
        case NUMBER_RULE_COMPOSITE:
            return digit == 4 || digit == 6 || digit == 8 || digit == 9;
        case NUMBER_RULE_MULTIPLE_OF_FOUR: return digit % 4 == 0;
        case NUMBER_RULE_GREATER_THAN_FIVE: return digit > 5;
        case NUMBER_RULE_LESS_THAN_FIVE: return digit < 5;
        case NUMBER_RULE_BETWEEN_THREE_AND_SEVEN:
            return digit >= 3 && digit <= 7;
        case NUMBER_RULE_DIVISOR_OF_TWELVE:
            return digit != 0 && 12 % digit == 0;
        case NUMBER_RULE_DIVISOR_OF_EIGHTEEN:
            return digit != 0 && 18 % digit == 0;
        case NUMBER_RULE_SQUARE_IS_EVEN: return (digit * digit) % 2 == 0;
        case NUMBER_RULE_SQUARE_IS_ODD: return (digit * digit) % 2 == 1;
        case NUMBER_RULE_EXACT_SEVEN: return digit == 7;
        default: return false;
    }
}

uint8_t getNumberPuzzleDigitAt(const DungeonRoom& room, int x, int y)
{
    return isInField(room.numberPuzzle, x, y)
        ? room.numberPuzzle.digits[digitIndex(room.numberPuzzle, x, y)] : 255;
}

bool validateNumberPuzzleSafePath(const DungeonRoom& room)
{
    const NumberTilePuzzleState& state = room.numberPuzzle;
    if (state.fieldWidth == 0 || state.fieldHeight == 0) return false;
    if (isStateful(state.rule))
    {
        if (state.requiredLength == 0 ||
            state.requiredLength > NUMBER_STATEFUL_MAX_STEPS) return false;
        bool reachable[NUMBER_STATEFUL_MAX_STEPS]
                      [NUMBER_FIELD_STORAGE_HEIGHT]
                      [NUMBER_FIELD_STORAGE_WIDTH] = {};
        for (uint8_t y = 0; y < state.fieldHeight; ++y)
            for (uint8_t x = 0; x < state.fieldWidth; ++x)
                if (isStartEdge(state, state.fieldX + x, state.fieldY + y) &&
                    getNumberPuzzleDigitAt(room, state.fieldX + x, state.fieldY + y) ==
                        state.requiredSequence[0])
                    reachable[0][y][x] = true;
        static const int8_t dx[4] = {1, -1, 0, 0};
        static const int8_t dy[4] = {0, 0, 1, -1};
        for (uint8_t step = 1; step < state.requiredLength; ++step)
            for (uint8_t y = 0; y < state.fieldHeight; ++y)
                for (uint8_t x = 0; x < state.fieldWidth; ++x)
                {
                    if (getNumberPuzzleDigitAt(
                            room, state.fieldX + x, state.fieldY + y) !=
                        state.requiredSequence[step]) continue;
                    for (uint8_t d = 0; d < 4; ++d)
                    {
                        const int px = x - dx[d];
                        const int py = y - dy[d];
                        if (px >= 0 && py >= 0 && px < state.fieldWidth &&
                            py < state.fieldHeight && reachable[step - 1][py][px])
                        { reachable[step][y][x] = true; break; }
                    }
                }
        const uint8_t last = state.requiredLength - 1;
        for (uint8_t y = 0; y < state.fieldHeight; ++y)
            for (uint8_t x = 0; x < state.fieldWidth; ++x)
                if (reachable[last][y][x] &&
                    isFarEdge(state, state.fieldX + x, state.fieldY + y))
                    return true;
        return false;
    }
    bool visited[NUMBER_FIELD_STORAGE_HEIGHT][NUMBER_FIELD_STORAGE_WIDTH] = {};
    uint8_t queue[NUMBER_FIELD_TILE_CAPACITY] = {};
    uint8_t head = 0;
    uint8_t tail = 0;
    for (uint8_t localY = 0; localY < state.fieldHeight; ++localY)
        for (uint8_t localX = 0; localX < state.fieldWidth; ++localX)
        {
            const int x = state.fieldX + localX;
            const int y = state.fieldY + localY;
            if (isStartEdge(state, x, y) &&
                isNumberTileSafe(state.rule, getNumberPuzzleDigitAt(room, x, y)))
            {
                visited[localY][localX] = true;
                queue[tail++] = localY * NUMBER_FIELD_STORAGE_WIDTH + localX;
            }
        }
    static const int8_t dx[4] = {1, -1, 0, 0};
    static const int8_t dy[4] = {0, 0, 1, -1};
    while (head < tail)
    {
        const uint8_t index = queue[head++];
        const uint8_t localX = index % NUMBER_FIELD_STORAGE_WIDTH;
        const uint8_t localY = index / NUMBER_FIELD_STORAGE_WIDTH;
        const int x = state.fieldX + localX;
        const int y = state.fieldY + localY;
        if (isFarEdge(state, x, y)) return true;
        for (uint8_t direction = 0; direction < 4; ++direction)
        {
            const int nx = localX + dx[direction];
            const int ny = localY + dy[direction];
            if (nx < 0 || ny < 0 || nx >= state.fieldWidth ||
                ny >= state.fieldHeight || visited[ny][nx]) continue;
            const uint8_t digit = getNumberPuzzleDigitAt(
                room, state.fieldX + nx, state.fieldY + ny);
            if (!isNumberTileSafe(state.rule, digit)) continue;
            visited[ny][nx] = true;
            queue[tail++] = ny * NUMBER_FIELD_STORAGE_WIDTH + nx;
        }
    }
    return false;
}

bool configureNumberTilePuzzleRoom(DungeonRoom& room, Direction exitDirection,
                                   uint8_t level, const uint8_t* rolls,
                                   uint16_t rollCount)
{
    if (room.type != ROOM_PUZZLE || rolls == nullptr || rollCount == 0 ||
        !hasOppositePair(room, exitDirection)) return false;
    DungeonRoom candidate = room;
    candidate.puzzleType = PUZZLE_NUMBER_TILES;
    candidate.npcSpawn = DungeonNPCSpawn{};
    candidate.bellPuzzle = BellPuzzleState{};
    NumberTilePuzzleState& state = candidate.numberPuzzle;
    state = NumberTilePuzzleState{};
    state.progress = NUMBER_PUZZLE_UNSOLVED;
    state.lockedExitDirection = exitDirection;
    getNumberPuzzleDigitRange(level, state.minimumDigit, state.maximumDigit);
    state.rule = selectNumberTileRuleForLevel(
        level, state.minimumDigit, state.maximumDigit,
        rolls[0], rolls[1 % rollCount]);
    const NumberTileRuleDefinition* ruleDefinition =
        getNumberTileRuleDefinition(state.rule);
    state.clueVariant = ruleDefinition != nullptr && ruleDefinition->clueCount > 0
        ? rolls[2 % rollCount] % ruleDefinition->clueCount : 0;
    const bool stateful = isStateful(state.rule);
    if (stateful)
    {
        state.minimumDigit = 0;
        state.maximumDigit = 9;
    }
    if (isHorizontalCrossing(exitDirection))
    {
        state.fieldWidth = stateful ? statefulLengthForLevel(level, rolls[3 % rollCount])
                                   : (level <= 4 ? 4 : (level <= 9 ? 5 : 6));
        state.fieldHeight = level <= 4 ? 4 : 6;
        state.fieldX = (ROOM_WIDTH - state.fieldWidth) / 2;
        state.fieldY = (ROOM_HEIGHT - state.fieldHeight) / 2;
        state.clueX = exitDirection == DIR_EAST ? 2 : ROOM_WIDTH - 3;
        state.clueY = ROOM_HEIGHT / 2;
    }
    else
    {
        state.fieldWidth = level <= 4 ? 4 : 6;
        state.fieldHeight = stateful ? statefulLengthForLevel(level, rolls[3 % rollCount])
                                    : (level <= 4 ? 4 : (level <= 9 ? 5 : 6));
        state.fieldX = (ROOM_WIDTH - state.fieldWidth) / 2;
        state.fieldY = (ROOM_HEIGHT - state.fieldHeight) / 2;
        state.clueX = ROOM_WIDTH / 2;
        state.clueY = exitDirection == DIR_SOUTH ? 2 : ROOM_HEIGHT - 3;
    }
    if (candidate.map.tiles[state.clueY][state.clueX] != TILE_FLOOR) return false;

    bool path[NUMBER_FIELD_STORAGE_HEIGHT][NUMBER_FIELD_STORAGE_WIDTH] = {};
    uint16_t rollIndex = 4;
    int lateral = isHorizontalCrossing(exitDirection)
        ? state.fieldHeight / 2 : state.fieldWidth / 2;
    const uint8_t depth = isHorizontalCrossing(exitDirection)
        ? state.fieldWidth : state.fieldHeight;
    if (stateful)
    {
        const uint8_t length = depth;
        bool useful = false;
        for (uint8_t attempt = 0; attempt < 12 && !useful; ++attempt)
        {
            state.seedA = rolls[rollIndex++ % rollCount] % 10;
            state.seedB = rolls[rollIndex++ % rollCount] % 10;
            useful = buildStatefulSequence(state, length);
        }
        if (!useful)
        {
            state.seedA = state.rule == NUMBER_RULE_DOUBLE_MINUS_ONE_MOD_10 ? 2 : 0;
            state.seedB = 5;
            buildStatefulSequence(state, length);
        }
    }
    for (uint8_t step = 0; step < depth; ++step)
    {
        if (step > 0 && !stateful)
        {
            const int shift = static_cast<int>(rolls[rollIndex++ % rollCount] % 3) - 1;
            const int limit = isHorizontalCrossing(exitDirection)
                ? state.fieldHeight : state.fieldWidth;
            lateral += shift;
            if (lateral < 0) lateral = 0;
            if (lateral >= limit) lateral = limit - 1;
        }
        int localX = isHorizontalCrossing(exitDirection) ? step : lateral;
        int localY = isHorizontalCrossing(exitDirection) ? lateral : step;
        if (exitDirection == DIR_WEST) localX = state.fieldWidth - 1 - localX;
        if (exitDirection == DIR_NORTH) localY = state.fieldHeight - 1 - localY;
        path[localY][localX] = true;
        state.digits[localY * NUMBER_FIELD_STORAGE_WIDTH + localX] =
            stateful ? state.requiredSequence[step] :
                chooseDigit(state.rule, true, state.minimumDigit,
                            state.maximumDigit, rolls[rollIndex++ % rollCount]);
    }

    bool placedSafeDistractor = false;
    bool placedUnsafeDistractor = false;
    for (uint8_t localY = 0; localY < state.fieldHeight; ++localY)
        for (uint8_t localX = 0; localX < state.fieldWidth; ++localX)
        {
            const uint8_t index = localY * NUMBER_FIELD_STORAGE_WIDTH + localX;
            if (!path[localY][localX])
            {
                bool safe = (rolls[rollIndex++ % rollCount] & 1) == 0;
                if (!placedSafeDistractor) safe = true;
                else if (!placedUnsafeDistractor) safe = false;
                if (stateful)
                {
                    const uint8_t expected = state.requiredSequence[
                        rolls[rollIndex++ % rollCount] % state.requiredLength];
                    state.digits[index] = safe ? expected :
                        static_cast<uint8_t>((expected + 1 +
                            rolls[rollIndex++ % rollCount] % 9) % 10);
                }
                else
                    state.digits[index] = chooseDigit(
                        state.rule, safe, state.minimumDigit, state.maximumDigit,
                        rolls[rollIndex++ % rollCount]);
                placedSafeDistractor |= safe;
                placedUnsafeDistractor |= !safe;
            }
            candidate.map.tiles[state.fieldY + localY][state.fieldX + localX] =
                TILE_NUMBER_PUZZLE;
        }
    candidate.map.tiles[state.clueY][state.clueX] = TILE_NUMBER_CLUE_PLAQUE;
    makeCrossingBarriers(candidate, state);
    if (!placedSafeDistractor || !placedUnsafeDistractor ||
        !validateNumberPuzzleSafePath(candidate) ||
        !validateRoomConnectivity(candidate)) return false;
    room = candidate;
    return true;
}

bool isNumberTilePuzzleRoom(const DungeonRoom& room)
{
    return room.type == ROOM_PUZZLE &&
        room.puzzleType == PUZZLE_NUMBER_TILES &&
        room.numberPuzzle.progress != NUMBER_PUZZLE_NONE;
}

bool interactWithCurrentNumberPuzzleClue(int x, int y)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isNumberTilePuzzleRoom(room) || room.numberPuzzle.clueX != x ||
        room.numberPuzzle.clueY != y) return false;
    const char* clue = getNumberTileClue(room.numberPuzzle);
    if (clue[0] == '\0') return false;
    if (isStateful(room.numberPuzzle.rule))
    {
        char message[192];
        if (room.numberPuzzle.rule == NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10)
            snprintf(message, sizeof(message), "Begin with %u, then %u. %s",
                     room.numberPuzzle.seedA, room.numberPuzzle.seedB, clue);
        else
            snprintf(message, sizeof(message), "Begin with %u. %s",
                     room.numberPuzzle.seedA, clue);
        setGameMessage(message);
    }
    else setGameMessage(clue);
    return true;
}

bool tryEnterCurrentNumberPuzzleTile(Entity& player, int x, int y)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return true;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isNumberTilePuzzleRoom(room) || !isInField(room.numberPuzzle, x, y))
        return true;
    NumberTilePuzzleState& state = room.numberPuzzle;
    const uint8_t digit = getNumberPuzzleDigitAt(room, x, y);
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[dungeon.currentRoom];
    const bool stateful = isStateful(state.rule);
    if (!runtime.numberCrossingActive && !isStartEdge(state, x, y))
    {
        setGameMessage("Begin at the near edge.");
        return false;
    }
    const bool valid = stateful
        ? runtime.numberCurrentStep < state.requiredLength &&
          digit == state.requiredSequence[runtime.numberCurrentStep]
        : isNumberTileSafe(state.rule, digit);
    if (!valid)
    {
        state.failedX = x;
        state.failedY = y;
        markTileDirty(x, y);
        if (stateful)
        {
            resetStatefulTraversal(runtime, state);
            movePlayerToStartLanding(player, state);
            setGameMessage("The sequence breaks.");
        }
        else setGameMessage("The tile rejects your step.");
        return false;
    }
    runtime.numberCrossingActive = true;
    if (stateful)
    {
        runtime.numberPreviousDigit = runtime.numberCurrentDigit;
        runtime.numberCurrentDigit = digit;
        ++runtime.numberCurrentStep;
    }
    if (state.failedX >= 0) markTileDirty(state.failedX, state.failedY);
    state.failedX = -1;
    state.failedY = -1;
    return true;
}

void handleCurrentNumberPuzzleMovement(Entity& player, int previousX, int previousY)
{
    if (dungeon.currentRoom >= dungeon.roomCount || player.type != ENTITY_PLAYER)
        return;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isNumberTilePuzzleRoom(room)) return;
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[dungeon.currentRoom];
    bool& crossing = runtime.numberCrossingActive;
    if (!crossing || isInField(room.numberPuzzle, player.x, player.y)) return;
    if (isInField(room.numberPuzzle, previousX, previousY) &&
        isFarEdge(room.numberPuzzle, previousX, previousY))
    {
        const int dx = static_cast<int>(player.x) - previousX;
        const int dy = static_cast<int>(player.y) - previousY;
        const bool exitedForward =
            (room.numberPuzzle.lockedExitDirection == DIR_EAST && dx == 1) ||
            (room.numberPuzzle.lockedExitDirection == DIR_WEST && dx == -1) ||
            (room.numberPuzzle.lockedExitDirection == DIR_SOUTH && dy == 1) ||
            (room.numberPuzzle.lockedExitDirection == DIR_NORTH && dy == -1);
        const bool sequenceComplete = !isStateful(room.numberPuzzle.rule) ||
            runtime.numberCurrentStep == room.numberPuzzle.requiredLength;
        if (exitedForward && sequenceComplete)
        {
            room.numberPuzzle.progress = NUMBER_PUZZLE_COMPLETE;
            room.completed = true;
            crossing = false;
            setGameMessage(isStateful(room.numberPuzzle.rule)
                ? "The pattern is complete."
                : "The numbered path unlocks the exit.");
            return;
        }
    }
    if (isStateful(room.numberPuzzle.rule))
        resetStatefulTraversal(runtime, room.numberPuzzle);
    else crossing = false;
}

bool tryUnlockCurrentNumberPuzzleExit(Direction direction)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isNumberTilePuzzleRoom(room) ||
        room.numberPuzzle.lockedExitDirection != direction) return true;
    if (room.numberPuzzle.progress == NUMBER_PUZZLE_COMPLETE) return true;
    setGameMessage("The exit is locked.");
    return false;
}

NumberTileVisualState getNumberTileVisualState(
    const DungeonRoom& room, int x, int y, const Entity* player)
{
    if (room.numberPuzzle.failedX == x && room.numberPuzzle.failedY == y)
        return NUMBER_TILE_FAILED;
    if (player != nullptr && player->active && player->x == x && player->y == y)
        return NUMBER_TILE_SAFE_ACTIVE;
    return NUMBER_TILE_IDLE;
}
