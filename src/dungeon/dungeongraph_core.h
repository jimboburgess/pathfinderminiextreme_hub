#ifndef PATHFINDERMINIEXTREME_025_DUNGEON_GRAPH_CORE_H
#define PATHFINDERMINIEXTREME_025_DUNGEON_GRAPH_CORE_H

#include "dungeon/dungeontopology.h"

namespace dungeon_graph_core
{
struct GraphOffset
{
    int8_t x;
    int8_t y;
};

constexpr Direction CARDINAL_DIRECTIONS[4] = {
    DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};

inline GraphOffset graphOffset(Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH: return {0, -1};
        case DIR_EAST: return {1, 0};
        case DIR_SOUTH: return {0, 1};
        case DIR_WEST: return {-1, 0};
        default: return {0, 0};
    }
}

inline Direction oppositeDirection(Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH: return DIR_SOUTH;
        case DIR_EAST: return DIR_WEST;
        case DIR_SOUTH: return DIR_NORTH;
        case DIR_WEST: return DIR_EAST;
        default: return DIR_NORTH;
    }
}

template <typename Room>
uint8_t getRoomNeighbor(const Room& room, Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH: return room.north;
        case DIR_EAST: return room.east;
        case DIR_SOUTH: return room.south;
        case DIR_WEST: return room.west;
        default: return NO_ROOM;
    }
}

template <typename Room>
uint8_t& neighborReference(Room& room, Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH: return room.north;
        case DIR_EAST: return room.east;
        case DIR_SOUTH: return room.south;
        case DIR_WEST: return room.west;
        default: return room.north;
    }
}

template <typename Room>
void clearGraphRoom(Room& room)
{
    room.type = ROOM_EMPTY;
    room.north = NO_ROOM;
    room.east = NO_ROOM;
    room.south = NO_ROOM;
    room.west = NO_ROOM;
    room.dungeonX = NO_DUNGEON_COORDINATE;
    room.dungeonY = NO_DUNGEON_COORDINATE;
}

template <typename Room>
uint8_t getRoomDegree(const Room& room)
{
    uint8_t degree = 0;
    for (uint8_t direction = 0; direction < 4; direction++)
        if (getRoomNeighbor(room, CARDINAL_DIRECTIONS[direction]) != NO_ROOM)
            degree++;
    return degree;
}

template <typename Room>
bool isDeadEndRoom(const Room& room)
{
    return getRoomDegree(room) == 1;
}

template <typename Topology>
int8_t findRoomAtGraphCoordinate(
    const Topology& dungeon, int8_t dungeonX, int8_t dungeonY)
{
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
        if (dungeon.rooms[i].dungeonX != NO_DUNGEON_COORDINATE &&
            dungeon.rooms[i].dungeonX == dungeonX &&
            dungeon.rooms[i].dungeonY == dungeonY)
            return static_cast<int8_t>(i);
    return -1;
}

template <typename Topology>
uint8_t getRoomDistanceFromEntrance(
    const Topology& dungeon, uint8_t roomIndex)
{
    if (roomIndex >= dungeon.roomCount)
        return NO_ROOM;

    uint8_t distances[MAX_ROOMS];
    uint8_t queue[MAX_ROOMS];
    for (uint8_t i = 0; i < MAX_ROOMS; i++)
        distances[i] = NO_ROOM;

    uint8_t head = 0;
    uint8_t tail = 0;
    distances[0] = 0;
    queue[tail++] = 0;
    while (head < tail)
    {
        const uint8_t current = queue[head++];
        for (uint8_t direction = 0; direction < 4; direction++)
        {
            const uint8_t neighbor = getRoomNeighbor(
                dungeon.rooms[current], CARDINAL_DIRECTIONS[direction]);
            if (neighbor >= dungeon.roomCount || distances[neighbor] != NO_ROOM)
                continue;
            distances[neighbor] = distances[current] + 1;
            queue[tail++] = neighbor;
        }
    }
    return distances[roomIndex];
}

template <typename Topology>
bool dungeonGraphHasBranch(const Topology& dungeon)
{
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
        if (getRoomDegree(dungeon.rooms[i]) >= 3)
            return true;
    return false;
}

template <typename Topology>
bool dungeonGraphHasLoop(const Topology& dungeon)
{
    uint8_t edgeCount = 0;
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
        edgeCount += getRoomDegree(dungeon.rooms[i]);
    return edgeCount / 2 >= dungeon.roomCount;
}

template <typename Topology>
bool connectRooms(
    Topology& dungeon,
    uint8_t first,
    uint8_t second,
    Direction direction)
{
    if (first >= dungeon.roomCount || second >= dungeon.roomCount ||
        first == second ||
        getRoomNeighbor(dungeon.rooms[first], direction) != NO_ROOM ||
        getRoomNeighbor(
            dungeon.rooms[second], oppositeDirection(direction)) != NO_ROOM)
        return false;

    neighborReference(dungeon.rooms[first], direction) = second;
    neighborReference(
        dungeon.rooms[second], oppositeDirection(direction)) = first;
    return true;
}

template <typename Topology>
void disconnectRooms(
    Topology& dungeon, uint8_t first, uint8_t second, Direction direction)
{
    neighborReference(dungeon.rooms[first], direction) = NO_ROOM;
    neighborReference(
        dungeon.rooms[second], oppositeDirection(direction)) = NO_ROOM;
}

template <typename Topology>
bool placeConnectedRoom(
    Topology& dungeon,
    uint8_t roomIndex,
    uint8_t parentIndex,
    Direction direction)
{
    const GraphOffset offset = graphOffset(direction);
    const int8_t x = dungeon.rooms[parentIndex].dungeonX + offset.x;
    const int8_t y = dungeon.rooms[parentIndex].dungeonY + offset.y;
    if (findRoomAtGraphCoordinate(dungeon, x, y) >= 0)
        return false;

    dungeon.rooms[roomIndex].dungeonX = x;
    dungeon.rooms[roomIndex].dungeonY = y;
    return connectRooms(dungeon, parentIndex, roomIndex, direction);
}

template <typename Topology, typename RandomSource>
bool tryPlaceFromParent(
    Topology& dungeon,
    uint8_t roomIndex,
    uint8_t parentIndex,
    RandomSource& randomSource)
{
    const uint8_t firstDirection = static_cast<uint8_t>(randomSource.next(4));
    for (uint8_t offset = 0; offset < 4; offset++)
    {
        const Direction direction = CARDINAL_DIRECTIONS[
            (firstDirection + offset) % 4];
        if (getRoomNeighbor(dungeon.rooms[parentIndex], direction) == NO_ROOM &&
            placeConnectedRoom(
                dungeon, roomIndex, parentIndex, direction))
            return true;
    }
    return false;
}

template <typename Topology, typename RandomSource>
void addOptionalLoops(
    Topology& dungeon, uint8_t bossRoom, RandomSource& randomSource)
{
    for (uint8_t first = 1; first < dungeon.roomCount; first++)
    {
        if (first == bossRoom ||
            dungeon.rooms[first].dungeonX == NO_DUNGEON_COORDINATE)
            continue;
        for (uint8_t directionValue = 0; directionValue < 4; directionValue++)
        {
            const Direction direction = CARDINAL_DIRECTIONS[directionValue];
            if (getRoomNeighbor(dungeon.rooms[first], direction) != NO_ROOM)
                continue;
            const GraphOffset offset = graphOffset(direction);
            const int8_t adjacent = findRoomAtGraphCoordinate(
                dungeon,
                dungeon.rooms[first].dungeonX + offset.x,
                dungeon.rooms[first].dungeonY + offset.y);
            if (adjacent <= static_cast<int8_t>(first) ||
                adjacent == bossRoom ||
                randomSource.next(100) >= DUNGEON_LOOP_CHANCE_PERCENT)
                continue;
            if (!connectRooms(
                    dungeon, first, static_cast<uint8_t>(adjacent), direction))
                continue;
            if (getRoomDistanceFromEntrance(dungeon, bossRoom) <
                MIN_BOSS_GRAPH_DISTANCE)
                disconnectRooms(
                    dungeon,
                    first,
                    static_cast<uint8_t>(adjacent),
                    direction);
        }
    }
}

template <typename Topology, typename RandomSource>
bool findFreeTreasureDirection(
    const Topology& dungeon,
    uint8_t bossRoom,
    Direction& result,
    RandomSource& randomSource)
{
    const uint8_t firstDirection = static_cast<uint8_t>(randomSource.next(4));
    for (uint8_t offsetIndex = 0; offsetIndex < 4; offsetIndex++)
    {
        const Direction direction = CARDINAL_DIRECTIONS[
            (firstDirection + offsetIndex) % 4];
        const GraphOffset offset = graphOffset(direction);
        if (getRoomNeighbor(dungeon.rooms[bossRoom], direction) == NO_ROOM &&
            findRoomAtGraphCoordinate(
                dungeon,
                dungeon.rooms[bossRoom].dungeonX + offset.x,
                dungeon.rooms[bossRoom].dungeonY + offset.y) < 0)
        {
            result = direction;
            return true;
        }
    }
    return false;
}

template <typename Topology>
DungeonTopologyValidationError validateTopologyDetailed(
    const Topology& dungeon)
{
    if (dungeon.roomCount < MIN_DUNGEON_ROOMS ||
        dungeon.roomCount > MAX_DUNGEON_ROOMS)
        return DUNGEON_TOPOLOGY_INVALID_ROOM_COUNT;
    if (dungeon.bossRoom == NO_ROOM || dungeon.treasureRoom == NO_ROOM ||
        dungeon.bossRoom >= dungeon.roomCount ||
        dungeon.treasureRoom >= dungeon.roomCount)
        return DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM;
    if (dungeon.rooms[0].type != ROOM_ENTRANCE ||
        dungeon.rooms[dungeon.bossRoom].type != ROOM_BOSS ||
        dungeon.rooms[dungeon.treasureRoom].type != ROOM_TREASURE)
        return DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_TYPE;

    uint8_t entranceCount = 0;
    uint8_t bossCount = 0;
    uint8_t treasureCount = 0;
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
    {
        if (dungeon.rooms[i].dungeonX == NO_DUNGEON_COORDINATE ||
            dungeon.rooms[i].dungeonY == NO_DUNGEON_COORDINATE)
            return DUNGEON_TOPOLOGY_UNPLACED_COORDINATE;

        entranceCount += dungeon.rooms[i].type == ROOM_ENTRANCE;
        bossCount += dungeon.rooms[i].type == ROOM_BOSS;
        treasureCount += dungeon.rooms[i].type == ROOM_TREASURE;
        for (uint8_t other = i + 1; other < dungeon.roomCount; other++)
            if (dungeon.rooms[i].dungeonX == dungeon.rooms[other].dungeonX &&
                dungeon.rooms[i].dungeonY == dungeon.rooms[other].dungeonY)
                return DUNGEON_TOPOLOGY_DUPLICATE_COORDINATE;

        for (uint8_t direction = 0; direction < 4; direction++)
        {
            const Direction cardinal = CARDINAL_DIRECTIONS[direction];
            const uint8_t neighbor = getRoomNeighbor(
                dungeon.rooms[i], cardinal);
            if (neighbor == NO_ROOM)
                continue;
            if (neighbor >= dungeon.roomCount)
                return DUNGEON_TOPOLOGY_INVALID_NEIGHBOR;
            if (neighbor == i)
                return DUNGEON_TOPOLOGY_SELF_LINK;
            if (getRoomNeighbor(
                    dungeon.rooms[neighbor], oppositeDirection(cardinal)) != i)
                return DUNGEON_TOPOLOGY_NONRECIPROCAL_LINK;

            const GraphOffset offset = graphOffset(cardinal);
            if (dungeon.rooms[neighbor].dungeonX !=
                    dungeon.rooms[i].dungeonX + offset.x ||
                dungeon.rooms[neighbor].dungeonY !=
                    dungeon.rooms[i].dungeonY + offset.y)
                return DUNGEON_TOPOLOGY_INVALID_NEIGHBOR_COORDINATE;
        }
        if (getRoomDistanceFromEntrance(dungeon, i) == NO_ROOM)
            return DUNGEON_TOPOLOGY_UNREACHABLE_ROOM;
    }

    if (entranceCount != 1 || bossCount != 1 || treasureCount != 1)
        return DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_COUNT;
    if (!dungeonGraphHasBranch(dungeon))
        return DUNGEON_TOPOLOGY_MISSING_BRANCH;
    if (getRoomDistanceFromEntrance(dungeon, dungeon.bossRoom) <
        MIN_BOSS_GRAPH_DISTANCE)
        return DUNGEON_TOPOLOGY_BOSS_TOO_CLOSE;
    if (getRoomDegree(dungeon.rooms[dungeon.bossRoom]) != 2)
        return DUNGEON_TOPOLOGY_INVALID_BOSS_DEGREE;

    const auto& treasure = dungeon.rooms[dungeon.treasureRoom];
    if (getRoomDegree(treasure) != 1)
        return DUNGEON_TOPOLOGY_INVALID_TREASURE_DEGREE;
    const bool treasureConnectsToBoss =
        treasure.north == dungeon.bossRoom ||
        treasure.east == dungeon.bossRoom ||
        treasure.south == dungeon.bossRoom ||
        treasure.west == dungeon.bossRoom;
    if (!treasureConnectsToBoss)
        return DUNGEON_TOPOLOGY_TREASURE_NOT_CONNECTED_TO_BOSS;
    return DUNGEON_TOPOLOGY_VALID;
}

template <typename Topology>
bool findRiddleRoomTowardBoss(
    const Topology& dungeon,
    uint8_t& riddleRoom,
    Direction& exitDirection)
{
    for (uint8_t roomIndex = 1; roomIndex < dungeon.roomCount; roomIndex++)
    {
        if (roomIndex == dungeon.bossRoom ||
            roomIndex == dungeon.treasureRoom)
            continue;
        for (uint8_t direction = 0; direction < 4; direction++)
        {
            const Direction cardinal = CARDINAL_DIRECTIONS[direction];
            if (getRoomNeighbor(dungeon.rooms[roomIndex], cardinal) ==
                dungeon.bossRoom)
            {
                riddleRoom = roomIndex;
                exitDirection = cardinal;
                return true;
            }
        }
    }
    return false;
}

template <typename Topology>
DungeonTopologyValidationError validateLayoutDetailed(
    const Topology& dungeon)
{
    const DungeonTopologyValidationError topologyError =
        validateTopologyDetailed(dungeon);
    if (topologyError != DUNGEON_TOPOLOGY_VALID)
        return topologyError;
    if (dungeon.riddleRoom == NO_ROOM ||
        dungeon.riddleRoom == 0 ||
        dungeon.riddleRoom >= dungeon.roomCount ||
        dungeon.riddleRoom == dungeon.bossRoom ||
        dungeon.riddleRoom == dungeon.treasureRoom)
        return DUNGEON_TOPOLOGY_INVALID_RIDDLE_ROOM;

    uint8_t expectedRiddle = NO_ROOM;
    Direction expectedExit = DIR_NORTH;
    if (!findRiddleRoomTowardBoss(
            dungeon, expectedRiddle, expectedExit) ||
        expectedRiddle != dungeon.riddleRoom ||
        getRoomNeighbor(
            dungeon.rooms[dungeon.riddleRoom], expectedExit) !=
            dungeon.bossRoom)
        return DUNGEON_TOPOLOGY_RIDDLE_NOT_ON_BOSS_ROUTE;
    return DUNGEON_TOPOLOGY_VALID;
}

template <typename Topology, typename RandomSource>
bool generateTopology(
    Topology& dungeon,
    uint8_t targetRoomCount,
    RandomSource& randomSource)
{
    if (targetRoomCount < MIN_DUNGEON_ROOMS ||
        targetRoomCount > MAX_DUNGEON_ROOMS)
        return false;

    dungeon.roomCount = targetRoomCount;
    dungeon.bossRoom = NO_ROOM;
    dungeon.treasureRoom = NO_ROOM;
    for (uint8_t i = 0; i < MAX_ROOMS; i++)
        clearGraphRoom(dungeon.rooms[i]);

    dungeon.rooms[0].type = ROOM_ENTRANCE;
    dungeon.rooms[0].dungeonX = 0;
    dungeon.rooms[0].dungeonY = 0;

    if (!placeConnectedRoom(dungeon, 1, 0, DIR_EAST))
        return false;
    for (uint8_t roomIndex = 2; roomIndex <= 4; roomIndex++)
        if (!tryPlaceFromParent(
                dungeon, roomIndex, roomIndex - 1, randomSource))
            return false;

    bool branchPlaced = false;
    const uint8_t firstBranchParent =
        1 + static_cast<uint8_t>(randomSource.next(3));
    for (uint8_t parentOffset = 0;
         parentOffset < 3 && !branchPlaced;
         parentOffset++)
    {
        const uint8_t parent = 1 +
            (firstBranchParent - 1 + parentOffset) % 3;
        branchPlaced = tryPlaceFromParent(
            dungeon, 5, parent, randomSource);
    }
    if (!branchPlaced)
        return false;

    const uint8_t ordinaryRoomCount = targetRoomCount - 1;
    for (uint8_t roomIndex = 6;
         roomIndex < ordinaryRoomCount;
         roomIndex++)
    {
        bool placed = false;
        const uint8_t firstParent = 1 +
            static_cast<uint8_t>(randomSource.next(roomIndex - 1));
        for (uint8_t parentOffset = 0;
             parentOffset < roomIndex - 1 && !placed;
             parentOffset++)
        {
            const uint8_t parent = 1 +
                (firstParent - 1 + parentOffset) % (roomIndex - 1);
            placed = tryPlaceFromParent(
                dungeon, roomIndex, parent, randomSource);
        }
        if (!placed)
            return false;
    }

    uint8_t boss = NO_ROOM;
    uint8_t farthestDistance = 0;
    Direction treasureDirection = DIR_NORTH;
    for (uint8_t i = 1; i < ordinaryRoomCount; i++)
    {
        Direction candidateDirection = DIR_NORTH;
        const uint8_t distance = getRoomDistanceFromEntrance(dungeon, i);
        if (distance >= farthestDistance &&
            isDeadEndRoom(dungeon.rooms[i]) &&
            findFreeTreasureDirection(
                dungeon,
                i,
                candidateDirection,
                randomSource))
        {
            boss = i;
            farthestDistance = distance;
            treasureDirection = candidateDirection;
        }
    }
    if (boss == NO_ROOM || farthestDistance < MIN_BOSS_GRAPH_DISTANCE)
        return false;

    dungeon.bossRoom = boss;
    dungeon.rooms[boss].type = ROOM_BOSS;
    addOptionalLoops(dungeon, boss, randomSource);

    if (!findFreeTreasureDirection(
            dungeon, boss, treasureDirection, randomSource))
        return false;
    const uint8_t treasure = targetRoomCount - 1;
    if (!placeConnectedRoom(
            dungeon, treasure, boss, treasureDirection))
        return false;
    dungeon.treasureRoom = treasure;
    dungeon.rooms[treasure].type = ROOM_TREASURE;

    return validateTopologyDetailed(dungeon) == DUNGEON_TOPOLOGY_VALID;
}

template <typename Topology, typename RandomSource>
bool generateLayout(
    Topology& dungeon,
    RandomSource& randomSource,
    DungeonGenerationReport& report)
{
    dungeon.riddleRoom = NO_ROOM;
    report.attempts = 0;
    report.riddleExitDirection = DIR_NORTH;

    const uint8_t targetRoomCount = static_cast<uint8_t>(
        MIN_DUNGEON_ROOMS +
        randomSource.next(MAX_DUNGEON_ROOMS - MIN_DUNGEON_ROOMS + 1));
    bool topologyGenerated = false;
    for (uint8_t attempt = 1;
         attempt <= MAX_DUNGEON_GENERATION_ATTEMPTS && !topologyGenerated;
         attempt++)
    {
        report.attempts = attempt;
        topologyGenerated = generateTopology(
            dungeon, targetRoomCount, randomSource);
    }
    if (!topologyGenerated)
        return false;

    if (!findRiddleRoomTowardBoss(
            dungeon,
            dungeon.riddleRoom,
            report.riddleExitDirection))
        return false;
    return validateLayoutDetailed(dungeon) == DUNGEON_TOPOLOGY_VALID;
}
}

#endif
