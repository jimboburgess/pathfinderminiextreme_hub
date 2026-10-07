#include "dungeon/puzzleroom.h"

#include "dungeon/bellpuzzle.h"
#include "dungeon/brazierpuzzle.h"
#include "dungeon/numbertilepuzzle.h"
#include "dungeon/npcs.h"

namespace
{
struct RotatingPuzzleRegistration
{
    DungeonPuzzleType type;
    uint8_t cumulativeUpperBound;
};

constexpr RotatingPuzzleRegistration ROTATING_PUZZLES[] =
{
    {PUZZLE_BELLS, BELL_PUZZLE_WEIGHT},
    {PUZZLE_NUMBER_TILES,
     BELL_PUZZLE_WEIGHT + NUMBER_TILE_PUZZLE_WEIGHT},
    {PUZZLE_BRAZIERS,
     BELL_PUZZLE_WEIGHT + NUMBER_TILE_PUZZLE_WEIGHT +
         BRAZIER_PUZZLE_WEIGHT}
};

static_assert(sizeof(ROTATING_PUZZLES) / sizeof(ROTATING_PUZZLES[0]) ==
              ROTATING_PUZZLE_TYPE_COUNT,
              "Rotating puzzle registry count is stale");
static_assert(BELL_PUZZLE_WEIGHT + NUMBER_TILE_PUZZLE_WEIGHT +
              BRAZIER_PUZZLE_WEIGHT == 100,
              "Rotating puzzle weights must total 100 percent");

uint8_t findPuzzleRegistration(DungeonPuzzleType puzzleType)
{
    for (uint8_t index = 0; index < ROTATING_PUZZLE_TYPE_COUNT; ++index)
        if (ROTATING_PUZZLES[index].type == puzzleType) return index;
    return ROTATING_PUZZLE_TYPE_COUNT;
}
}

RoomType selectMiddleRoomTypeFromRoll(uint8_t roll)
{
    roll %= 100;
    if (roll < 30) return ROOM_COMBAT;
    if (roll < 60) return ROOM_AMBUSH;
    if (roll < 90) return ROOM_PUZZLE;
    return ROOM_EMPTY;
}

DungeonPuzzleType selectRandomPuzzleType(uint8_t roll)
{
    roll %= 100;
    for (const RotatingPuzzleRegistration& puzzle : ROTATING_PUZZLES)
        if (roll < puzzle.cumulativeUpperBound) return puzzle.type;
    return ROTATING_PUZZLES[ROTATING_PUZZLE_TYPE_COUNT - 1].type;
}

bool isRotatingDungeonPuzzleType(DungeonPuzzleType puzzleType)
{
    return findPuzzleRegistration(puzzleType) < ROTATING_PUZZLE_TYPE_COUNT;
}

void resetDungeonPuzzleBlueprints(DungeonRoom& room)
{
    room.puzzleType = PUZZLE_NONE;
    room.completed = false;
    room.npcSpawn = DungeonNPCSpawn{};
    room.bellPuzzle = BellPuzzleState{};
    room.numberPuzzle = NumberTilePuzzleState{};
    room.brazierPuzzle = BrazierPuzzleState{};
}

bool configureSelectedPuzzleRoom(
    DungeonRoom& room, DungeonPuzzleType puzzleType,
    Direction lockedExitDirection, uint8_t level,
    const uint8_t* rolls, uint8_t rollCount)
{
    if (!isRotatingDungeonPuzzleType(puzzleType) || rolls == nullptr ||
        rollCount == 0 || room.type != ROOM_PUZZLE)
        return false;

    DungeonRoom candidate = room;
    resetDungeonPuzzleBlueprints(candidate);
    bool configured = false;
    switch (puzzleType)
    {
        case PUZZLE_BELLS:
            configured = configureBellPuzzleRoom(
                candidate, lockedExitDirection, level, rolls, rollCount);
            break;
        case PUZZLE_NUMBER_TILES:
            configured = configureNumberTilePuzzleRoom(
                candidate, lockedExitDirection, level, rolls, rollCount);
            break;
        case PUZZLE_BRAZIERS:
            configured = configureBrazierPuzzleRoom(
                candidate, lockedExitDirection, rolls, rollCount);
            break;
        default:
            return false;
    }
    if (!configured) return false;
    room = candidate;
    return true;
}

bool configureRotatingPuzzleRoom(
    DungeonRoom& room, DungeonPuzzleType preferredType,
    Direction lockedExitDirection, uint8_t level,
    const uint8_t* rolls, uint8_t rollCount)
{
    uint8_t first = findPuzzleRegistration(preferredType);
    if (first >= ROTATING_PUZZLE_TYPE_COUNT) first = 0;
    for (uint8_t attempt = 0; attempt < ROTATING_PUZZLE_TYPE_COUNT; ++attempt)
    {
        const uint8_t index = static_cast<uint8_t>(
            (first + attempt) % ROTATING_PUZZLE_TYPE_COUNT);
        if (configureSelectedPuzzleRoom(
                room, ROTATING_PUZZLES[index].type, lockedExitDirection,
                level, rolls, rollCount))
            return true;
    }

    resetDungeonPuzzleBlueprints(room);
    room.type = ROOM_EMPTY;
    room.encounterTheme = ENCOUNTER_NONE;
    room.completed = false;
    return false;
}

bool isDungeonPuzzleComplete(const DungeonRoom& room)
{
    if (room.type != ROOM_PUZZLE) return false;
    switch (room.puzzleType)
    {
        case PUZZLE_RIDDLEMAN:
            return room.npcSpawn.puzzleState == RIDDLE_ROOM_COMPLETE;
        case PUZZLE_BELLS:
            return room.bellPuzzle.progress == BELL_PUZZLE_COMPLETE;
        case PUZZLE_NUMBER_TILES:
            return room.numberPuzzle.progress == NUMBER_PUZZLE_COMPLETE;
        case PUZZLE_BRAZIERS:
            return room.brazierPuzzle.progress == BRAZIER_PUZZLE_COMPLETE;
        case PUZZLE_NONE:
        default:
            return false;
    }
}
