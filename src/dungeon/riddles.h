#ifndef PATHFINDERMINIEXTREME_025_RIDDLES_H
#define PATHFINDERMINIEXTREME_025_RIDDLES_H

#include <stdint.h>

enum RiddleID : uint8_t
{
    RIDDLE_MOUNTAIN,
    RIDDLE_WIND,
    RIDDLE_DARK,
    RIDDLE_FISH,
    RIDDLE_TIME,
    RIDDLE_TEETH,
    RIDDLE_EGG,
    RIDDLE_LETTER_E,
    RIDDLE_MATCH,
    RIDDLE_RIVER,
    RIDDLE_TREE,
    RIDDLE_ONION,
    RIDDLE_ECHO,
    RIDDLE_SPLINTER,
    RIDDLE_LEAVES,
    RIDDLE_GLOVES,
    RIDDLE_SKULL,
    RIDDLE_FOOTSTEPS,
    RIDDLE_SILENCE,
    RIDDLE_FIRE,
    RIDDLE_COUNT,
    RIDDLE_NONE = 255
};

enum RiddleDifficulty : uint8_t
{
    RIDDLE_EASY,
    RIDDLE_MEDIUM,
    RIDDLE_HARD
};

struct RiddleDefinition
{
    RiddleID id;
    RiddleDifficulty difficulty;
    const char* question;
    const char* answers[4];
    uint8_t correctAnswerIndex;
};

enum RiddleResult : uint8_t
{
    RIDDLE_UNANSWERED,
    RIDDLE_ANSWERED_INCORRECT,
    RIDDLE_ANSWERED_CORRECT
};

// Compact room-owned state. answerOrder maps each displayed position back to
// its source-data answer index and is generated exactly once per encounter.
struct RiddleState
{
    RiddleID id = RIDDLE_NONE;
    uint8_t answerOrder[4] = {0, 1, 2, 3};
    uint8_t selectedAnswer = 255;
    RiddleResult result = RIDDLE_UNANSWERED;
};

const RiddleDefinition* getRiddleDefinition(RiddleID id);
bool isValidRiddleAnswerOrder(const RiddleState& state);
void initializeRiddleState(
    RiddleState& state,
    RiddleID id,
    const uint8_t shuffleRolls[3]);
bool answerRiddle(RiddleState& state, uint8_t displayedAnswerIndex);
const char* getDisplayedRiddleAnswer(
    const RiddleState& state, uint8_t displayedAnswerIndex);

#endif
