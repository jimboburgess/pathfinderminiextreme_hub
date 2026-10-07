#include "dungeon/brazierpuzzle.h"

#include <Arduino.h>

#include "audio/audio.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/furniture.h"
#include "dungeon/roomgen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "graphics/tiles.h"

namespace
{
constexpr uint8_t MAX_SCRAMBLE_ATTEMPTS = 8;

bool isReservedPuzzleTile(const DungeonRoom& room, int x, int y)
{
    if (x <= 0 || x >= ROOM_WIDTH - 1 || y <= 0 || y >= ROOM_HEIGHT - 1 ||
        room.map.tiles[y][x] != TILE_FLOOR ||
        getDungeonFurnitureAt(room, x, y) != nullptr ||
        getTrapAt(room, x, y) != nullptr || isHealingFountainTile(room, x, y))
        return true;
    for (uint8_t i = 0; i < room.connectionCount; ++i)
    {
        const RoomConnection& door = room.connections[i];
        if (abs(static_cast<int>(door.x) - x) +
            abs(static_cast<int>(door.y) - y) <= 2)
            return true;
    }
    return false;
}

bool canPlaceBrazierRow(const DungeonRoom& room, int rowX, int rowY,
                        uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        const int x = rowX + i;
        if (isReservedPuzzleTile(room, x, rowY) ||
            isReservedPuzzleTile(room, x, rowY - 1) ||
            isReservedPuzzleTile(room, x, rowY + 1))
            return false;
    }
    return true;
}

bool findRewardTile(const DungeonRoom& room, int rowX, int rowY,
                    uint8_t count, int8_t& outX, int8_t& outY)
{
    const int centerX = rowX + count / 2;
    for (int distance = 0; distance < ROOM_WIDTH + ROOM_HEIGHT; ++distance)
        for (int y = 2; y < ROOM_HEIGHT - 2; ++y)
            for (int x = 2; x < ROOM_WIDTH - 2; ++x)
            {
                if (abs(x - centerX) + abs(y - (rowY + 3)) != distance ||
                    isReservedPuzzleTile(room, x, y) ||
                    (y == rowY && x >= rowX && x < rowX + count))
                    continue;
                outX = static_cast<int8_t>(x);
                outY = static_cast<int8_t>(y);
                return true;
            }
    return false;
}

void unlockCurrentRewardChest(const BrazierPuzzleState& state)
{
    Entity* chest = getEntityAt(
        dungeon.entities, dungeon.entityCount,
        static_cast<uint8_t>(state.rewardX),
        static_cast<uint8_t>(state.rewardY));
    if (chest == nullptr || chest->type != ENTITY_CHEST) return;
    chest->locked = false;
    markEntityFootprintDirty(*chest);
}
}

uint8_t getBrazierActiveMask(uint8_t brazierCount)
{
    if (brazierCount < MIN_BRAZIER_PUZZLE_COUNT ||
        brazierCount > MAX_BRAZIER_PUZZLE_COUNT)
        return 0;
    return static_cast<uint8_t>((1u << brazierCount) - 1u);
}

uint8_t toggleBrazierMask(uint8_t currentMask, uint8_t brazierCount,
                          uint8_t selectedIndex)
{
    const uint8_t activeMask = getBrazierActiveMask(brazierCount);
    if (activeMask == 0 || selectedIndex >= brazierCount)
        return currentMask & activeMask;
    uint8_t toggleMask = static_cast<uint8_t>(1u << selectedIndex);
    if (selectedIndex > 0)
        toggleMask |= static_cast<uint8_t>(1u << (selectedIndex - 1));
    if (selectedIndex + 1 < brazierCount)
        toggleMask |= static_cast<uint8_t>(1u << (selectedIndex + 1));
    return static_cast<uint8_t>((currentMask ^ toggleMask) & activeMask);
}

bool areAllBrazierPuzzleBraziersLit(uint8_t litMask, uint8_t brazierCount)
{
    const uint8_t activeMask = getBrazierActiveMask(brazierCount);
    return activeMask != 0 && (litMask & activeMask) == activeMask;
}

bool didBrazierInteractionIgnite(uint8_t oldMask, uint8_t newMask,
                                 uint8_t brazierCount)
{
    const uint8_t activeMask = getBrazierActiveMask(brazierCount);
    return (static_cast<uint8_t>(~oldMask) & newMask & activeMask) != 0;
}

uint8_t getBrazierMinimumSolutionMoves(uint8_t litMask, uint8_t brazierCount)
{
    const uint8_t activeMask = getBrazierActiveMask(brazierCount);
    if (activeMask == 0) return BRAZIER_PUZZLE_UNREACHABLE;
    const uint8_t target = static_cast<uint8_t>(litMask & activeMask);
    uint8_t distance[32];
    for (uint8_t& value : distance) value = BRAZIER_PUZZLE_UNREACHABLE;
    uint8_t queue[32] = {};
    uint8_t head = 0;
    uint8_t tail = 0;
    distance[activeMask] = 0;
    queue[tail++] = activeMask;
    while (head < tail)
    {
        const uint8_t mask = queue[head++];
        if (mask == target) return distance[mask];
        for (uint8_t index = 0; index < brazierCount; ++index)
        {
            const uint8_t next = toggleBrazierMask(mask, brazierCount, index);
            if (distance[next] != BRAZIER_PUZZLE_UNREACHABLE) continue;
            distance[next] = static_cast<uint8_t>(distance[mask] + 1);
            queue[tail++] = next;
        }
    }
    return BRAZIER_PUZZLE_UNREACHABLE;
}

uint8_t generateBrazierScramble(uint8_t brazierCount, const uint8_t* rolls,
                                uint8_t rollCount)
{
    const uint8_t activeMask = getBrazierActiveMask(brazierCount);
    if (activeMask == 0) return 0;
    const uint8_t minimumDistance = brazierCount == 4 ? 2 : 3;
    const uint8_t minimumDepth = brazierCount == 4 ? 3 : 4;
    const uint8_t depthRange = brazierCount == 4 ? 4 : 5;
    uint8_t rollIndex = 0;
    for (uint8_t attempt = 0; attempt < MAX_SCRAMBLE_ATTEMPTS; ++attempt)
    {
        uint8_t mask = activeMask;
        const uint8_t depthRoll = rolls != nullptr && rollCount > 0
            ? rolls[rollIndex++ % rollCount] : attempt;
        const uint8_t depth = static_cast<uint8_t>(
            minimumDepth + depthRoll % depthRange);
        for (uint8_t step = 0; step < depth; ++step)
        {
            const uint8_t moveRoll = rolls != nullptr && rollCount > 0
                ? rolls[rollIndex++ % rollCount]
                : static_cast<uint8_t>(attempt + step * 3);
            mask = toggleBrazierMask(mask, brazierCount,
                                     moveRoll % brazierCount);
        }
        const uint8_t distance = getBrazierMinimumSolutionMoves(
            mask, brazierCount);
        if (mask != activeMask && distance != BRAZIER_PUZZLE_UNREACHABLE &&
            distance >= minimumDistance)
            return mask;
    }

    // Bounded deterministic fallback: select only a proven-reachable state at
    // the requested minimum distance. This also handles repetitive RNG rolls.
    uint8_t candidates[32] = {};
    uint8_t candidateCount = 0;
    for (uint8_t mask = 0; mask <= activeMask; ++mask)
    {
        const uint8_t distance = getBrazierMinimumSolutionMoves(
            mask, brazierCount);
        if (mask != activeMask && distance != BRAZIER_PUZZLE_UNREACHABLE &&
            distance >= minimumDistance)
            candidates[candidateCount++] = mask;
    }
    if (candidateCount == 0) return toggleBrazierMask(activeMask, brazierCount, 0);
    const uint8_t choice = rolls != nullptr && rollCount > 0
        ? rolls[rollCount - 1] : 0;
    return candidates[choice % candidateCount];
}

bool configureBrazierPuzzleRoom(DungeonRoom& room, Direction lockedDirection,
                                const uint8_t* rolls, uint8_t rollCount)
{
    if (room.type != ROOM_PUZZLE || rolls == nullptr || rollCount == 0 ||
        getRoomConnection(room, lockedDirection) == nullptr)
        return false;

    DungeonRoom candidate = room;
    candidate.puzzleType = PUZZLE_BRAZIERS;
    candidate.npcSpawn = DungeonNPCSpawn{};
    candidate.bellPuzzle = BellPuzzleState{};
    candidate.numberPuzzle = NumberTilePuzzleState{};
    candidate.brazierPuzzle = BrazierPuzzleState{};
    candidate.fountain = HealingFountain{};
    for (DungeonFurnitureInstance& furniture : candidate.furniture)
        furniture = DungeonFurnitureInstance{};
    for (TrapInstance& trap : candidate.traps) trap = TrapInstance{};
    for (SuspicionInstance& suspicion : candidate.suspicions)
        suspicion = SuspicionInstance{};

    BrazierPuzzleState& state = candidate.brazierPuzzle;
    state.brazierCount = static_cast<uint8_t>(
        MIN_BRAZIER_PUZZLE_COUNT + (rolls[0] & 1u));
    state.litMask = generateBrazierScramble(
        state.brazierCount, rolls + 1,
        static_cast<uint8_t>(rollCount - 1));
    state.progress = BRAZIER_PUZZLE_UNSOLVED;
    state.lockedExitDirection = lockedDirection;

    const int rowX = (ROOM_WIDTH - state.brazierCount) / 2;
    bool rowPlaced = false;
    for (uint8_t offset = 0; offset < 7 && !rowPlaced; ++offset)
    {
        const int rowY = 3 + ((rolls[1 % rollCount] + offset) % 7);
        if (!canPlaceBrazierRow(candidate, rowX, rowY, state.brazierCount))
            continue;
        DungeonRoom placement = candidate;
        bool placed = true;
        for (uint8_t index = 0; index < state.brazierCount; ++index)
            placed = placed && addDungeonFurniture(
                placement, FURNITURE_BRAZIER, rowX + index, rowY);
        if (!placed) continue;
        placement.brazierPuzzle.rowX = static_cast<int8_t>(rowX);
        placement.brazierPuzzle.rowY = static_cast<int8_t>(rowY);
        candidate = placement;
        rowPlaced = true;
    }
    if (!rowPlaced || !findRewardTile(
            candidate, candidate.brazierPuzzle.rowX,
            candidate.brazierPuzzle.rowY, state.brazierCount,
            candidate.brazierPuzzle.rewardX,
            candidate.brazierPuzzle.rewardY))
        return false;

    candidate.map.tiles[candidate.brazierPuzzle.rewardY]
                       [candidate.brazierPuzzle.rewardX] = TILE_CHEST_SPAWN;
    if (!validateRoomConnectivity(candidate)) return false;
    room = candidate;
    return true;
}

bool isBrazierPuzzleRoom(const DungeonRoom& room)
{
    return room.type == ROOM_PUZZLE && room.puzzleType == PUZZLE_BRAZIERS &&
        room.brazierPuzzle.progress != BRAZIER_PUZZLE_NONE;
}

int8_t getBrazierPuzzleIndexAt(const DungeonRoom& room, int x, int y)
{
    if (!isBrazierPuzzleRoom(room) || y != room.brazierPuzzle.rowY ||
        x < room.brazierPuzzle.rowX ||
        x >= room.brazierPuzzle.rowX + room.brazierPuzzle.brazierCount)
        return -1;
    const DungeonFurnitureInstance* furniture =
        getDungeonFurnitureAt(room, x, y);
    if (furniture == nullptr || furniture->type != FURNITURE_BRAZIER)
        return -1;
    return static_cast<int8_t>(x - room.brazierPuzzle.rowX);
}

bool isBrazierPuzzleLitAt(const DungeonRoom& room, int x, int y)
{
    const int8_t index = getBrazierPuzzleIndexAt(room, x, y);
    return index >= 0 &&
        (room.brazierPuzzle.litMask & (1u << index)) != 0;
}

const uint16_t* getBrazierSpriteForRoomTile(
    const DungeonRoom& room, int x, int y)
{
    const int8_t index = getBrazierPuzzleIndexAt(room, x, y);
    if (index < 0) return dungeonBrazier16x16;
    return isBrazierPuzzleLitAt(room, x, y)
        ? dungeonBrazier16x16 : dungeonBrazierUnlit16x16;
}

bool interactWithCurrentBrazierPuzzleAt(int x, int y)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    const int8_t index = getBrazierPuzzleIndexAt(room, x, y);
    if (index < 0) return false;
    BrazierPuzzleState& state = room.brazierPuzzle;
    if (state.progress == BRAZIER_PUZZLE_COMPLETE)
    {
        setGameMessage("The braziers burn steadily.");
        return true;
    }

    const uint8_t oldMask = state.litMask;
    state.litMask = toggleBrazierMask(
        oldMask, state.brazierCount, static_cast<uint8_t>(index));
    const uint8_t changedMask = static_cast<uint8_t>(oldMask ^ state.litMask);
    for (uint8_t changedIndex = 0;
         changedIndex < state.brazierCount; ++changedIndex)
        if ((changedMask & (1u << changedIndex)) != 0)
            markTileDirty(state.rowX + changedIndex, state.rowY);

    if (didBrazierInteractionIgnite(oldMask, state.litMask,
                                    state.brazierCount))
        playSound(SoundEffect::BRAZIER_IGNITE);

    if (areAllBrazierPuzzleBraziersLit(state.litMask, state.brazierCount))
    {
        state.litMask = getBrazierActiveMask(state.brazierCount);
        state.progress = BRAZIER_PUZZLE_COMPLETE;
        room.completed = true;
        unlockCurrentRewardChest(state);
        setGameMessage("The braziers blaze together.");
    }
    else
    {
        setGameMessage("Light all the braziers.");
    }
    return true;
}

bool tryUnlockCurrentBrazierPuzzleExit(Direction direction)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isBrazierPuzzleRoom(room) ||
        room.brazierPuzzle.lockedExitDirection != direction ||
        room.brazierPuzzle.progress == BRAZIER_PUZZLE_COMPLETE)
        return true;
    setGameMessage("The exit is locked.");
    return false;
}

bool isCurrentBrazierPuzzleRewardAt(int x, int y)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    return isBrazierPuzzleRoom(room) &&
        room.brazierPuzzle.rewardX == x && room.brazierPuzzle.rewardY == y;
}

bool isCurrentBrazierPuzzleRewardSealed()
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    return isBrazierPuzzleRoom(room) &&
        room.brazierPuzzle.progress != BRAZIER_PUZZLE_COMPLETE;
}
