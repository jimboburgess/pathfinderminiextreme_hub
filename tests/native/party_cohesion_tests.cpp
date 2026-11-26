#include "dungeon/partycohesion.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void connect(DungeonTopology& graph, uint8_t left, uint8_t right,
             Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH:
            graph.rooms[left].north = right;
            graph.rooms[right].south = left;
            break;
        case DIR_EAST:
            graph.rooms[left].east = right;
            graph.rooms[right].west = left;
            break;
        case DIR_SOUTH:
            graph.rooms[left].south = right;
            graph.rooms[right].north = left;
            break;
        case DIR_WEST:
            graph.rooms[left].west = right;
            graph.rooms[right].east = left;
            break;
        default:
            throw std::runtime_error("cardinal graph connection");
    }
}

DungeonTopology fixture()
{
    DungeonTopology graph{};
    graph.roomCount = 5;
    connect(graph, 0, 1, DIR_EAST);
    connect(graph, 0, 2, DIR_NORTH);
    connect(graph, 1, 3, DIR_EAST);
    connect(graph, 3, 4, DIR_EAST);
    return graph;
}

PartyCohesionDecision move(
    const DungeonTopology& graph,
    uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY],
    uint8_t activeMask,
    uint8_t member,
    uint8_t destination,
    uint8_t& anchor)
{
    const PartyCohesionDecision decision = evaluatePartyRoomTransition(
        graph, rooms, activeMask, member, destination, anchor);
    anchor = decision.currentAnchor;
    if (decision.allowed)
    {
        rooms[member] = destination;
        anchor = decision.nextAnchor;
    }
    return decision;
}
}

int main()
{
    try
    {
        const DungeonTopology graph = fixture();
        uint8_t rooms[PARTY_COHESION_MEMBER_CAPACITY] = {0, 0, 0, 0};
        uint8_t anchor = NO_ROOM;
        const uint8_t everyone = 0x0f;

        check(move(graph, rooms, everyone, 0, 1, anchor).allowed,
              "party member enters neighboring room");
        check(anchor == 0, "single scout does not pull anchor");
        check(move(graph, rooms, everyone, 1, 2, anchor).allowed,
              "two scouts enter different neighboring rooms");
        check(rooms[0] == 1 && rooms[1] == 2 && rooms[2] == 0 &&
              rooms[3] == 0, "scouting never teleports another player");
        check(!move(graph, rooms, everyone, 0, 3, anchor).allowed,
              "scout cannot move two doors from anchor");
        check(rooms[0] == 1 && anchor == 0,
              "rejected transition preserves room and anchor");
        check(std::strcmp(PARTY_COHESION_REJECTION_MESSAGE,
                          "You cannot stray that far from your party.") == 0,
              "rejection message is exact");

        uint8_t tied[PARTY_COHESION_MEMBER_CAPACITY] = {0, 0, 1, 1};
        anchor = 0;
        check(evaluatePartyRoomTransition(
                  graph, tied, everyone, 0, 0, anchor).currentAnchor == 0,
              "tie retains previous anchor");
        anchor = 1;
        check(evaluatePartyRoomTransition(
                  graph, tied, everyone, 2, 1, anchor).currentAnchor == 1,
              "tie does not oscillate away from prior anchor");

        anchor = 0;
        check(move(graph, tied, everyone, 0, 1, anchor).allowed,
              "majority member can regroup");
        check(anchor == 1, "safe majority move shifts anchor");

        uint8_t deferred[PARTY_COHESION_MEMBER_CAPACITY] = {0, 1, 1, 2};
        anchor = 0;
        const PartyCohesionDecision deferredMove =
            evaluatePartyRoomTransition(
                graph, deferred, everyone, 0, 1, anchor);
        check(deferredMove.allowed && deferredMove.nextAnchor == 0,
              "unsafe anchor shift is deferred");

        uint8_t disconnected[PARTY_COHESION_MEMBER_CAPACITY] = {0, 0, 0, 3};
        anchor = 0;
        check(move(graph, disconnected, 0x08, 3, 4, anchor).allowed,
              "disconnected members do not restrict solo explorer");
        check(anchor == 4, "solo active member owns temporary anchor");
        check(move(graph, disconnected, everyone, 3, 3, anchor).allowed,
              "reconnected scout may move back toward party");
        check(move(graph, disconnected, everyone, 3, 1, anchor).allowed,
              "inconsistent scout can continue toward anchor");
        check(anchor == 0, "majority anchor restores after regrouping");

        uint8_t solo[PARTY_COHESION_MEMBER_CAPACITY] = {0, NO_ROOM,
                                                        NO_ROOM, NO_ROOM};
        anchor = 0;
        check(move(graph, solo, 0x01, 0, 4, anchor).allowed,
              "solo exploration is unrestricted");
        check(solo[0] == 4 && anchor == 4,
              "solo destination becomes anchor");

        std::cout << "PASS party cohesion graph policy\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
