#ifndef PATHFINDERMINIEXTREME_025_PUZZLEROOM_H
#define PATHFINDERMINIEXTREME_025_PUZZLEROOM_H

#include <stdint.h>

#include "dungeon/dungeon.h"

constexpr uint8_t BELL_PUZZLE_WEIGHT = 34;
constexpr uint8_t NUMBER_TILE_PUZZLE_WEIGHT = 33;
constexpr uint8_t BRAZIER_PUZZLE_WEIGHT = 33;
constexpr uint8_t ROTATING_PUZZLE_TYPE_COUNT = 3;
constexpr uint8_t PUZZLE_GENERATION_ROLL_COUNT = 128;

RoomType selectMiddleRoomTypeFromRoll(uint8_t roll);
DungeonPuzzleType selectRandomPuzzleType(uint8_t roll);
bool isRotatingDungeonPuzzleType(DungeonPuzzleType puzzleType);

// Clears all mutually exclusive puzzle blueprints while preserving geometry.
void resetDungeonPuzzleBlueprints(DungeonRoom& room);

// Configures exactly one requested reusable puzzle. Failure is transactional.
bool configureSelectedPuzzleRoom(
    DungeonRoom& room,
    DungeonPuzzleType puzzleType,
    Direction lockedExitDirection,
    uint8_t level,
    const uint8_t* rolls,
    uint8_t rollCount);

// Tries the weighted selection first, then each other registered type once.
// If every configurator rejects the geometry, leaves a safe empty room.
bool configureRotatingPuzzleRoom(
    DungeonRoom& room,
    DungeonPuzzleType preferredType,
    Direction lockedExitDirection,
    uint8_t level,
    const uint8_t* rolls,
    uint8_t rollCount);

bool isDungeonPuzzleComplete(const DungeonRoom& room);

#endif
