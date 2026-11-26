#ifndef PATHFINDERMINIEXTREME_025_PARTY_COHESION_H
#define PATHFINDERMINIEXTREME_025_PARTY_COHESION_H

#include <stdint.h>

#include "dungeon/dungeontopology.h"

constexpr uint8_t PARTY_COHESION_MEMBER_CAPACITY = 4;
constexpr const char* PARTY_COHESION_REJECTION_MESSAGE =
    "You cannot stray that far from your party.";

struct PartyCohesionDecision
{
    bool allowed = false;
    uint8_t currentAnchor = NO_ROOM;
    uint8_t nextAnchor = NO_ROOM;
};

namespace party_cohesion_detail
{
template <typename Room>
uint8_t neighbor(const Room& room, uint8_t direction)
{
    switch (direction)
    {
        case 0: return room.north;
        case 1: return room.east;
        case 2: return room.south;
        case 3: return room.west;
        default: return NO_ROOM;
    }
}

template <typename Graph>
uint8_t distance(const Graph& graph, uint8_t start, uint8_t goal)
{
    if (start >= graph.roomCount || goal >= graph.roomCount) return NO_ROOM;
    if (start == goal) return 0;

    uint8_t distances[MAX_ROOMS];
    uint8_t queue[MAX_ROOMS];
    for (uint8_t room = 0; room < MAX_ROOMS; ++room)
        distances[room] = NO_ROOM;
    distances[start] = 0;
    uint8_t head = 0;
    uint8_t tail = 0;
    queue[tail++] = start;
    while (head < tail)
    {
        const uint8_t room = queue[head++];
        for (uint8_t direction = 0; direction < 4; ++direction)
        {
            const uint8_t next = neighbor(graph.rooms[room], direction);
            if (next >= graph.roomCount || distances[next] != NO_ROOM)
                continue;
            distances[next] = static_cast<uint8_t>(distances[room] + 1);
            if (next == goal) return distances[next];
            queue[tail++] = next;
        }
    }
    return NO_ROOM;
}

inline uint8_t activeCount(uint8_t activeMask)
{
    uint8_t count = 0;
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
        if ((activeMask & (1u << member)) != 0) ++count;
    return count;
}

inline uint8_t roomPopulation(
    const uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t room)
{
    uint8_t count = 0;
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
        if ((activeMask & (1u << member)) != 0 && rooms[member] == room)
            ++count;
    return count;
}

inline uint8_t majorityRoom(
    const uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t previousAnchor)
{
    uint8_t largest = 0;
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
    {
        if ((activeMask & (1u << member)) == 0) continue;
        const uint8_t count = roomPopulation(rooms, activeMask, rooms[member]);
        if (count > largest) largest = count;
    }
    if (largest == 0) return NO_ROOM;
    if (roomPopulation(rooms, activeMask, previousAnchor) == largest)
        return previousAnchor;
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
        if ((activeMask & (1u << member)) != 0 &&
            roomPopulation(rooms, activeMask, rooms[member]) == largest)
            return rooms[member];
    return NO_ROOM;
}

template <typename Graph>
bool allWithinOneRoom(
    const Graph& graph,
    const uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t anchor)
{
    if (anchor >= graph.roomCount) return false;
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
        if ((activeMask & (1u << member)) != 0 &&
            distance(graph, anchor, rooms[member]) > 1)
            return false;
    return true;
}

template <typename Graph>
uint8_t stableAnchor(
    const Graph& graph,
    const uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t previousAnchor)
{
    const uint8_t candidate = majorityRoom(rooms, activeMask, previousAnchor);
    if (candidate == NO_ROOM) return NO_ROOM;
    if (previousAnchor >= graph.roomCount || candidate == previousAnchor ||
        allWithinOneRoom(graph, rooms, activeMask, candidate))
        return candidate;
    return previousAnchor;
}
}

template <typename Graph>
PartyCohesionDecision evaluatePartyRoomTransition(
    const Graph& graph,
    const uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t movingMember,
    uint8_t destinationRoom,
    uint8_t previousAnchor)
{
    PartyCohesionDecision result{};
    result.currentAnchor = party_cohesion_detail::stableAnchor(
        graph, rooms, activeMask, previousAnchor);
    result.nextAnchor = result.currentAnchor;
    if (movingMember >= PARTY_COHESION_MEMBER_CAPACITY ||
        (activeMask & (1u << movingMember)) == 0 ||
        rooms[movingMember] >= graph.roomCount ||
        destinationRoom >= graph.roomCount)
        return result;

    if (party_cohesion_detail::activeCount(activeMask) <= 1)
    {
        result.allowed = true;
        result.nextAnchor = destinationRoom;
        return result;
    }

    uint8_t movedRooms[PARTY_COHESION_MEMBER_CAPACITY];
    for (uint8_t member = 0; member < PARTY_COHESION_MEMBER_CAPACITY;
         ++member)
        movedRooms[member] = rooms[member];
    movedRooms[movingMember] = destinationRoom;
    result.nextAnchor = party_cohesion_detail::stableAnchor(
        graph, movedRooms, activeMask, result.currentAnchor);

    const uint8_t destinationDistance = party_cohesion_detail::distance(
        graph, result.nextAnchor, destinationRoom);
    if (destinationDistance <= 1)
    {
        result.allowed = true;
        return result;
    }

    const uint8_t sourceDistance = party_cohesion_detail::distance(
        graph, result.currentAnchor, rooms[movingMember]);
    result.allowed = destinationDistance < sourceDistance;
    if (!result.allowed) result.nextAnchor = result.currentAnchor;
    return result;
}

#endif
