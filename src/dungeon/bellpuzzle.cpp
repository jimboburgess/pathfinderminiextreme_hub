#include "dungeon/bellpuzzle.h"

#include <Arduino.h>

#include "audio/audio.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/furniture.h"
#include "dungeon/roomgen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"

namespace
{
constexpr uint8_t MAX_SEQUENCE_GENERATION_ATTEMPTS = 8;

BellToneID furnitureTone(DungeonFurnitureType type)
{
    switch (type)
    {
        case FURNITURE_BELL_LOW: return BELL_LOW;
        case FURNITURE_BELL_MID: return BELL_MID;
        case FURNITURE_BELL_HIGH: return BELL_HIGH;
        default: return BELL_TONE_COUNT;
    }
}

bool isReservedBellTile(const DungeonRoom& room, int x, int y)
{
    if (x <= 0 || x >= ROOM_WIDTH - 1 || y <= 0 || y >= ROOM_HEIGHT - 1 ||
        room.map.tiles[y][x] != TILE_FLOOR)
        return true;
    for (uint8_t i = 0; i < room.connectionCount; ++i)
    {
        const RoomConnection& door = room.connections[i];
        if (abs(door.x - x) + abs(door.y - y) <= 1) return true;
    }
    return false;
}

bool findRuneTile(DungeonRoom& room, Direction lockedDirection, int8_t& outX,
                  int8_t& outY)
{
    int originX = ROOM_WIDTH / 2;
    int originY = ROOM_HEIGHT / 2;
    for (uint8_t i = 0; i < room.connectionCount; ++i)
    {
        const RoomConnection& connection = room.connections[i];
        if (connection.direction == lockedDirection) continue;
        originX = connection.x;
        originY = connection.y;
        if (connection.direction == DIR_NORTH) ++originY;
        else if (connection.direction == DIR_SOUTH) --originY;
        else if (connection.direction == DIR_WEST) ++originX;
        else if (connection.direction == DIR_EAST) --originX;
        break;
    }
    for (int distance = 0; distance < ROOM_WIDTH + ROOM_HEIGHT; ++distance)
        for (int y = 1; y < ROOM_HEIGHT - 1; ++y)
            for (int x = 1; x < ROOM_WIDTH - 1; ++x)
                if (abs(x - originX) + abs(y - originY) == distance &&
                    !isReservedBellTile(room, x, y) &&
                    getDungeonFurnitureAt(room, x, y) == nullptr)
                {
                    outX = static_cast<int8_t>(x);
                    outY = static_cast<int8_t>(y);
                    return true;
                }
    return false;
}

bool findBellKeyTile(const DungeonRoom& room, int8_t& outX, int8_t& outY)
{
    for (int distance = 0; distance < ROOM_WIDTH + ROOM_HEIGHT; ++distance)
        for (int y = 1; y < ROOM_HEIGHT - 1; ++y)
            for (int x = 1; x < ROOM_WIDTH - 1; ++x)
                if (abs(x - ROOM_WIDTH / 2) + abs(y - (ROOM_HEIGHT / 2 + 2)) == distance &&
                    room.map.tiles[y][x] == TILE_FLOOR &&
                    (x != room.bellPuzzle.runeX || y != room.bellPuzzle.runeY) &&
                    getDungeonFurnitureAt(room, x, y) == nullptr)
                {
                    outX = static_cast<int8_t>(x);
                    outY = static_cast<int8_t>(y);
                    return true;
                }
    return false;
}

bool spawnBellKey(DungeonRoom& room)
{
    if (room.bellPuzzle.progress != BELL_PUZZLE_UNSOLVED) return false;
    int keyX = room.bellPuzzle.keyX;
    int keyY = room.bellPuzzle.keyY;
    Entity* occupant = getEntityAt(
        dungeon.entities, dungeon.entityCount,
        static_cast<uint8_t>(keyX), static_cast<uint8_t>(keyY));
    if (occupant != nullptr)
    {
        bool found = false;
        for (int distance = 0; distance < ROOM_WIDTH + ROOM_HEIGHT && !found; ++distance)
            for (int y = 1; y < ROOM_HEIGHT - 1 && !found; ++y)
                for (int x = 1; x < ROOM_WIDTH - 1; ++x)
                {
                    if (abs(x - keyX) + abs(y - keyY) != distance ||
                        room.map.tiles[y][x] != TILE_FLOOR ||
                        getDungeonFurnitureAt(room, x, y) != nullptr ||
                        getEntityAt(dungeon.entities, dungeon.entityCount,
                            static_cast<uint8_t>(x), static_cast<uint8_t>(y)) != nullptr)
                        continue;
                    keyX = x;
                    keyY = y;
                    found = true;
                    break;
                }
        if (!found) return false;
    }
    Entity* key = spawnEntity(dungeon.entities, dungeon.entityCount,
                              ENTITY_PUZZLE_KEY, keyX, keyY);
    if (key == nullptr) return false;
    room.bellPuzzle.keyX = static_cast<int8_t>(keyX);
    room.bellPuzzle.keyY = static_cast<int8_t>(keyY);
    room.bellPuzzle.progress = BELL_PUZZLE_KEY_PRESENTED;
    markTileDirty(keyX, keyY);
    return true;
}
}

uint8_t getBellSequenceLengthForLevel(uint8_t level)
{
    if (level <= 3) return 3;
    if (level <= 6) return 4;
    if (level <= 10) return 5;
    if (level <= 15) return 6;
    return 7;
}

uint16_t getBellToneFrequency(BellToneID tone)
{
    switch (tone)
    {
        case BELL_LOW: return BELL_LOW_FREQUENCY;
        case BELL_MID: return BELL_MID_FREQUENCY;
        case BELL_HIGH: return BELL_HIGH_FREQUENCY;
        default: return 0;
    }
}

bool isValidBellSequence(const BellToneID* sequence, uint8_t length)
{
    if (sequence == nullptr || length < 3 || length > MAX_BELL_SEQUENCE)
        return false;
    bool used[BELL_TONE_COUNT] = {};
    uint8_t run = 0;
    BellToneID previous = BELL_TONE_COUNT;
    for (uint8_t i = 0; i < length; ++i)
    {
        if (sequence[i] >= BELL_TONE_COUNT) return false;
        used[sequence[i]] = true;
        run = sequence[i] == previous ? static_cast<uint8_t>(run + 1) : 1;
        if (run > 2) return false;
        previous = sequence[i];
    }
    uint8_t distinct = 0;
    for (bool present : used) if (present) ++distinct;
    return distinct >= 2;
}

void buildBellSequenceFromRolls(BellPuzzleState& state, uint8_t level,
                                const uint8_t* rolls, uint8_t rollCount)
{
    state.sequenceLength = getBellSequenceLengthForLevel(level);
    const uint8_t candidateCount = state.sequenceLength == 0 || rolls == nullptr
        ? 0 : static_cast<uint8_t>(rollCount / state.sequenceLength);
    const uint8_t attempts = candidateCount < MAX_SEQUENCE_GENERATION_ATTEMPTS
        ? candidateCount : MAX_SEQUENCE_GENERATION_ATTEMPTS;
    for (uint8_t attempt = 0; attempt < attempts; ++attempt)
    {
        for (uint8_t i = 0; i < state.sequenceLength; ++i)
            state.sequence[i] = static_cast<BellToneID>(
                rolls[attempt * state.sequenceLength + i] % BELL_TONE_COUNT);
        if (isValidBellSequence(state.sequence, state.sequenceLength)) return;
    }
    const BellToneID fallback[3] = {BELL_LOW, BELL_MID, BELL_HIGH};
    for (uint8_t i = 0; i < state.sequenceLength; ++i)
        state.sequence[i] = fallback[i % 3];
}

BellInputResult submitBellTone(BellPuzzleState& state, uint8_t& enteredCount,
                               BellToneID tone)
{
    if (state.progress != BELL_PUZZLE_UNSOLVED || tone >= BELL_TONE_COUNT ||
        enteredCount >= state.sequenceLength)
        return BELL_INPUT_IGNORED;
    if (state.sequence[enteredCount] != tone)
    {
        enteredCount = 0;
        return BELL_INPUT_WRONG;
    }
    ++enteredCount;
    if (enteredCount < state.sequenceLength) return BELL_INPUT_CORRECT_PREFIX;
    enteredCount = 0;
    return BELL_INPUT_SOLVED;
}

bool configureBellPuzzleRoom(DungeonRoom& room, Direction lockedDirection,
                             uint8_t level, const uint8_t* rolls,
                             uint8_t rollCount)
{
    if (room.type != ROOM_PUZZLE ||
        getRoomConnection(room, lockedDirection) == nullptr) return false;
    DungeonRoom candidate = room;
    candidate.puzzleType = PUZZLE_BELLS;
    candidate.npcSpawn = DungeonNPCSpawn{};
    candidate.bellPuzzle = BellPuzzleState{};
    candidate.bellPuzzle.progress = BELL_PUZZLE_UNSOLVED;
    candidate.bellPuzzle.lockedExitDirection = lockedDirection;
    buildBellSequenceFromRolls(candidate.bellPuzzle, level, rolls, rollCount);

    bool bellsPlaced = false;
    const int bellXs[3] = {4, ROOM_WIDTH / 2, ROOM_WIDTH - 5};
    for (int y = 3; y <= 6 && !bellsPlaced; ++y)
    {
        DungeonRoom placement = candidate;
        bellsPlaced = addDungeonFurniture(placement, FURNITURE_BELL_LOW, bellXs[0], y) &&
            addDungeonFurniture(placement, FURNITURE_BELL_MID, bellXs[1], y) &&
            addDungeonFurniture(placement, FURNITURE_BELL_HIGH, bellXs[2], y);
        if (bellsPlaced) candidate = placement;
    }
    if (!bellsPlaced || !findRuneTile(candidate, lockedDirection,
                                      candidate.bellPuzzle.runeX,
                                      candidate.bellPuzzle.runeY) ||
        !findBellKeyTile(candidate, candidate.bellPuzzle.keyX,
                        candidate.bellPuzzle.keyY)) return false;
    candidate.map.tiles[candidate.bellPuzzle.runeY][candidate.bellPuzzle.runeX] =
        TILE_BELL_LISTEN_RUNE;
    if (!validateRoomConnectivity(candidate)) return false;
    room = candidate;
    return true;
}

bool isBellPuzzleRoom(const DungeonRoom& room)
{
    return room.type == ROOM_PUZZLE && room.puzzleType == PUZZLE_BELLS &&
        room.bellPuzzle.progress != BELL_PUZZLE_NONE;
}

bool strikeCurrentBellAt(int x, int y)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isBellPuzzleRoom(room)) return false;
    const DungeonFurnitureInstance* furniture = getDungeonFurnitureAt(room, x, y);
    if (furniture == nullptr) return false;
    const BellToneID tone = furnitureTone(furniture->type);
    if (tone >= BELL_TONE_COUNT) return false;
    const uint16_t frequency = getBellToneFrequency(tone);
    playToneSequence(&frequency, 1, BELL_TONE_DURATION_MS, 0);
    uint8_t& entered = dungeon.roomRuntime[dungeon.currentRoom].bellEnteredCount;
    const BellInputResult result = submitBellTone(room.bellPuzzle, entered, tone);
    if (result == BELL_INPUT_WRONG)
        setGameMessage("That was not the sequence.");
    else if (result == BELL_INPUT_SOLVED)
    {
        if (!spawnBellKey(room)) return false;
        setGameMessage("The bells ring in harmony.");
    }
    return true;
}

bool handleCurrentBellListeningRuneEntry(Entity& mover, int x, int y)
{
    if (mover.type != ENTITY_PLAYER || dungeon.currentRoom >= dungeon.roomCount)
        return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isBellPuzzleRoom(room) || room.bellPuzzle.runeX != x ||
        room.bellPuzzle.runeY != y) return false;
    uint16_t frequencies[MAX_BELL_SEQUENCE] = {};
    for (uint8_t i = 0; i < room.bellPuzzle.sequenceLength; ++i)
        frequencies[i] = getBellToneFrequency(room.bellPuzzle.sequence[i]);
    playToneSequence(frequencies, room.bellPuzzle.sequenceLength,
                     BELL_TONE_DURATION_MS, BELL_PAUSE_DURATION_MS);
    setGameMessage("You hear a sequence of bells.");
    return true;
}

bool collectCurrentBellKey(Entity& keyEntity)
{
    if (dungeon.currentRoom >= dungeon.roomCount ||
        keyEntity.type != ENTITY_PUZZLE_KEY || !keyEntity.active) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isBellPuzzleRoom(room) ||
        room.bellPuzzle.progress != BELL_PUZZLE_KEY_PRESENTED ||
        keyEntity.x != room.bellPuzzle.keyX || keyEntity.y != room.bellPuzzle.keyY)
        return false;
    const int x = keyEntity.x;
    const int y = keyEntity.y;
    keyEntity.active = false;
    room.bellPuzzle.progress = BELL_PUZZLE_KEY_COLLECTED;
    markTileDirty(x, y);
    setGameMessage("You take the puzzle key.");
    return true;
}

bool tryUnlockCurrentBellExit(Direction direction)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isBellPuzzleRoom(room) ||
        room.bellPuzzle.lockedExitDirection != direction ||
        room.bellPuzzle.progress == BELL_PUZZLE_COMPLETE) return true;
    if (room.bellPuzzle.progress != BELL_PUZZLE_KEY_COLLECTED)
    {
        setGameMessage("The exit is locked.");
        return false;
    }
    room.bellPuzzle.progress = BELL_PUZZLE_COMPLETE;
    room.completed = true;
    setGameMessage("The key unlocks the door.");
    return true;
}
