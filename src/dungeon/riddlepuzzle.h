#ifndef PATHFINDERMINIEXTREME_025_RIDDLEPUZZLE_H
#define PATHFINDERMINIEXTREME_025_RIDDLEPUZZLE_H

#include <stdint.h>

#include "dungeon/npcs.h"

struct Entity;
struct DungeonRoom;

constexpr uint8_t RIDDLEMAN_LOCK_DC_SURCHARGE = 4;
constexpr uint8_t MAX_RIDDLE_ATTEMPTS = 4;

enum RiddlemanDoorBypassResult : uint8_t
{
    RIDDLEMAN_BYPASS_INVALID,
    RIDDLEMAN_BYPASS_FAILED,
    RIDDLEMAN_BYPASS_SUCCEEDED
};

enum RiddleRetryPaymentResult : uint8_t
{
    RIDDLE_RETRY_PAYMENT_INVALID,
    RIDDLE_RETRY_PAYMENT_INSUFFICIENT_GOLD,
    RIDDLE_RETRY_PAYMENT_GRANTED
};

enum RiddleCatCatchResult : uint8_t
{
    RIDDLE_CAT_NOT_ACTIVE,
    RIDDLE_CAT_ESCAPED,
    RIDDLE_CAT_CORNERED,
    RIDDLE_CAT_CAUGHT
};

bool configureRiddlemanPuzzleRoom(
    DungeonRoom& room,
    Direction lockedExitDirection,
    RiddleID riddleID,
    const uint8_t shuffleRolls[3]);

bool isRiddlemanPuzzleRoom(const DungeonRoom& room);
bool isRiddlemanExitLocked(const DungeonRoom& room, Direction direction);
bool handleCurrentBertramRiddleResult(bool correct);
bool collectCurrentRiddleKey(Entity& keyEntity);
bool tryUnlockCurrentRiddleExit(Direction direction);

// The ordinary lock baseline plus a level-scaled puzzle surcharge. The
// caller supplies the current dungeon challenge level (currently the player
// level, matching other dungeon challenge generation).
int getRiddlemanLockDisableDC(uint8_t challengeLevel);
bool getCurrentRiddlemanExitDirectionAt(int x, int y, Direction& direction);
bool noteCurrentRiddlemanBypassAttempt();
RiddlemanDoorBypassResult attemptCurrentRiddlemanDoorBypass(
    Direction direction, int disableDeviceTotal);

// Returns true only for Bertram and records the first/repeat dialogue bit in
// the current room's persistent puzzle state.
bool refuseCurrentBertramHostileAction(Entity& target);

uint16_t getRiddleRetryCost(uint8_t attemptsMade, uint8_t level);
uint8_t selectCatForcedEscapeThreshold(uint8_t randomValue);
uint8_t getCatCatchChance(
    uint8_t catchAttemptsMade, uint8_t forcedEscapeThreshold);
bool isRiddleRetryRequired(const DungeonRoom& room);
RiddleRetryPaymentResult payForCurrentRiddleRetry(Entity& player);
bool startCurrentRiddleCatChase(uint8_t thresholdRandomValue);
bool isBertramRiddleCat(const Entity& entity);
RiddleCatCatchResult attemptCatchCurrentRiddleCat(
    Entity& player, Entity& cat, uint8_t percentileRoll);

#endif
