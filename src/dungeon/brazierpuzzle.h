#ifndef PATHFINDERMINIEXTREME_025_BRAZIERPUZZLE_H
#define PATHFINDERMINIEXTREME_025_BRAZIERPUZZLE_H

#include <stdint.h>

#include "data/game.h"

struct DungeonRoom;

enum BrazierPuzzleProgress : uint8_t
{
    BRAZIER_PUZZLE_NONE,
    BRAZIER_PUZZLE_UNSOLVED,
    BRAZIER_PUZZLE_COMPLETE
};

constexpr uint8_t MIN_BRAZIER_PUZZLE_COUNT = 4;
constexpr uint8_t MAX_BRAZIER_PUZZLE_COUNT = 5;
constexpr uint8_t BRAZIER_PUZZLE_UNREACHABLE = 0xff;

// Bit zero is the leftmost brazier at rowX. Increasing bit indices move right.
// Furniture owns the physical positions; litMask is the sole authority for fire.
struct BrazierPuzzleState
{
    uint8_t brazierCount = 0;
    uint8_t litMask = 0;
    BrazierPuzzleProgress progress = BRAZIER_PUZZLE_NONE;
    uint8_t lockedExitDirection = DIR_NORTH;
    int8_t rowX = -1;
    int8_t rowY = -1;
    int8_t rewardX = -1;
    int8_t rewardY = -1;
};

static_assert(sizeof(BrazierPuzzleState) == 8,
              "Brazier puzzle blueprint must remain compact");

uint8_t getBrazierActiveMask(uint8_t brazierCount);
uint8_t toggleBrazierMask(
    uint8_t currentMask, uint8_t brazierCount, uint8_t selectedIndex);
bool areAllBrazierPuzzleBraziersLit(uint8_t litMask, uint8_t brazierCount);
bool didBrazierInteractionIgnite(
    uint8_t oldMask, uint8_t newMask, uint8_t brazierCount);
uint8_t getBrazierMinimumSolutionMoves(uint8_t litMask, uint8_t brazierCount);
uint8_t generateBrazierScramble(
    uint8_t brazierCount, const uint8_t* rolls, uint8_t rollCount);

bool configureBrazierPuzzleRoom(
    DungeonRoom& room,
    Direction lockedExitDirection,
    const uint8_t* rolls,
    uint8_t rollCount);
bool isBrazierPuzzleRoom(const DungeonRoom& room);
int8_t getBrazierPuzzleIndexAt(const DungeonRoom& room, int x, int y);
bool isBrazierPuzzleLitAt(const DungeonRoom& room, int x, int y);
const uint16_t* getBrazierSpriteForRoomTile(
    const DungeonRoom& room, int x, int y);
bool interactWithCurrentBrazierPuzzleAt(int x, int y);
bool tryUnlockCurrentBrazierPuzzleExit(Direction direction);
bool isCurrentBrazierPuzzleRewardAt(int x, int y);
bool isCurrentBrazierPuzzleRewardSealed();

#endif
