#ifndef PATHFINDERMINIEXTREME_025_BELLPUZZLE_H
#define PATHFINDERMINIEXTREME_025_BELLPUZZLE_H

#include <stdint.h>

#include "data/game.h"
#include "dungeon/puzzletypes.h"

struct DungeonRoom;
struct Entity;

enum BellToneID : uint8_t
{
    BELL_LOW,
    BELL_MID,
    BELL_HIGH,
    BELL_TONE_COUNT
};

enum BellPuzzleProgress : uint8_t
{
    BELL_PUZZLE_NONE,
    BELL_PUZZLE_UNSOLVED,
    BELL_PUZZLE_KEY_PRESENTED,
    BELL_PUZZLE_KEY_COLLECTED,
    BELL_PUZZLE_COMPLETE
};

enum BellInputResult : uint8_t
{
    BELL_INPUT_IGNORED,
    BELL_INPUT_CORRECT_PREFIX,
    BELL_INPUT_WRONG,
    BELL_INPUT_SOLVED
};

constexpr uint8_t MAX_BELL_SEQUENCE = 7;
constexpr uint16_t BELL_LOW_FREQUENCY = 523;
constexpr uint16_t BELL_MID_FREQUENCY = 659;
constexpr uint16_t BELL_HIGH_FREQUENCY = 784;
constexpr uint16_t BELL_TONE_DURATION_MS = 300;
constexpr uint16_t BELL_PAUSE_DURATION_MS = 160;

struct BellVisualRuntime {
    uint32_t startedAt=0;
    BellToneID tone=BELL_TONE_COUNT,lastTone=BELL_TONE_COUNT;
    bool sequence=false,active=false;
};
static_assert(sizeof(BellVisualRuntime)<=8,"Bell visual timing stays compact");

struct BellPuzzleState
{
    BellToneID sequence[MAX_BELL_SEQUENCE] = {};
    uint8_t sequenceLength = 0;
    BellPuzzleProgress progress = BELL_PUZZLE_NONE;
    uint8_t lockedExitDirection = DIR_NORTH;
    int8_t runeX = -1;
    int8_t runeY = -1;
    int8_t keyX = -1;
    int8_t keyY = -1;
};

static_assert(sizeof(BellPuzzleState) == 14,
              "Bell puzzle blueprint must remain compact");

uint8_t getBellSequenceLengthForLevel(uint8_t level);
uint16_t getBellToneFrequency(BellToneID tone);
bool isValidBellSequence(const BellToneID* sequence, uint8_t length);
void buildBellSequenceFromRolls(
    BellPuzzleState& state,
    uint8_t level,
    const uint8_t* rolls,
    uint8_t rollCount);
BellInputResult submitBellTone(
    BellPuzzleState& state, uint8_t& enteredCount, BellToneID tone);

bool configureBellPuzzleRoom(
    DungeonRoom& room,
    Direction lockedExitDirection,
    uint8_t level,
    const uint8_t* sequenceRolls,
    uint8_t sequenceRollCount);
bool isBellPuzzleRoom(const DungeonRoom& room);
bool playBellPuzzleSequence(const BellPuzzleState& state);
bool strikeCurrentBellAt(int x, int y);
// Shared room clock, independent of the audio mute/output state.
BellToneID getCurrentBellVisualTone(uint32_t now);
bool isCurrentBellVisuallyRingingAt(int x, int y, uint32_t now);
void updateCurrentBellVisuals();
bool isCurrentBellListeningRuneAt(int x, int y);
bool handleCurrentBellListeningRuneEntry(Entity& mover, int x, int y);
bool handleCurrentBellListeningRuneMovement(
    Entity& mover, int previousX, int previousY, int x, int y);
bool collectCurrentBellKey(Entity& keyEntity);
bool tryUnlockCurrentBellExit(Direction direction);

#endif
