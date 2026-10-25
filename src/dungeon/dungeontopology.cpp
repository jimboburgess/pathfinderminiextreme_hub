#include "dungeon/dungeontopology.h"

#include "dungeon/dungeongraph_core.h"

SeededDungeonRandom::SeededDungeonRandom(uint32_t seed)
    : state(seed == 0 ? 0x6d2b79f5U : seed)
{
}

uint32_t SeededDungeonRandom::next(uint32_t upperExclusive)
{
    if (upperExclusive == 0)
        return 0;

    uint32_t value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value;
    return value % upperExclusive;
}

uint8_t getRoomNeighbor(
    const DungeonTopologyRoom& room, Direction direction)
{
    return dungeon_graph_core::getRoomNeighbor(room, direction);
}

uint8_t getRoomDegree(const DungeonTopologyRoom& room)
{
    return dungeon_graph_core::getRoomDegree(room);
}

bool isDeadEndRoom(const DungeonTopologyRoom& room)
{
    return dungeon_graph_core::isDeadEndRoom(room);
}

int8_t findRoomAtGraphCoordinate(
    const DungeonTopology& dungeon, int8_t dungeonX, int8_t dungeonY)
{
    return dungeon_graph_core::findRoomAtGraphCoordinate(
        dungeon, dungeonX, dungeonY);
}

uint8_t getRoomDistanceFromEntrance(
    const DungeonTopology& dungeon, uint8_t roomIndex)
{
    return dungeon_graph_core::getRoomDistanceFromEntrance(
        dungeon, roomIndex);
}

bool dungeonGraphHasBranch(const DungeonTopology& dungeon)
{
    return dungeon_graph_core::dungeonGraphHasBranch(dungeon);
}

bool dungeonGraphHasLoop(const DungeonTopology& dungeon)
{
    return dungeon_graph_core::dungeonGraphHasLoop(dungeon);
}

DungeonTopologyValidationError validateDungeonTopologyDetailed(
    const DungeonTopology& dungeon)
{
    return dungeon_graph_core::validateTopologyDetailed(dungeon);
}

DungeonTopologyValidationError validateDungeonLayoutDetailed(
    const DungeonTopology& dungeon)
{
    return dungeon_graph_core::validateLayoutDetailed(dungeon);
}

const char* dungeonTopologyValidationErrorName(
    DungeonTopologyValidationError error)
{
    switch (error)
    {
        case DUNGEON_TOPOLOGY_VALID: return "valid";
        case DUNGEON_TOPOLOGY_INVALID_ROOM_COUNT:
            return "invalid room count";
        case DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM:
            return "invalid special-room index";
        case DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_TYPE:
            return "invalid special-room type";
        case DUNGEON_TOPOLOGY_UNPLACED_COORDINATE:
            return "unplaced logical coordinate";
        case DUNGEON_TOPOLOGY_DUPLICATE_COORDINATE:
            return "duplicate logical coordinate";
        case DUNGEON_TOPOLOGY_INVALID_NEIGHBOR:
            return "invalid neighbor index";
        case DUNGEON_TOPOLOGY_SELF_LINK:
            return "self-link";
        case DUNGEON_TOPOLOGY_NONRECIPROCAL_LINK:
            return "nonreciprocal connection";
        case DUNGEON_TOPOLOGY_INVALID_NEIGHBOR_COORDINATE:
            return "neighbor coordinate mismatch";
        case DUNGEON_TOPOLOGY_UNREACHABLE_ROOM:
            return "unreachable room";
        case DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_COUNT:
            return "invalid special-room count";
        case DUNGEON_TOPOLOGY_MISSING_BRANCH:
            return "missing graph branch";
        case DUNGEON_TOPOLOGY_BOSS_TOO_CLOSE:
            return "boss too close to entrance";
        case DUNGEON_TOPOLOGY_INVALID_BOSS_DEGREE:
            return "boss does not connect only to its approach and treasure";
        case DUNGEON_TOPOLOGY_INVALID_TREASURE_DEGREE:
            return "treasure room is not a dead end";
        case DUNGEON_TOPOLOGY_TREASURE_NOT_CONNECTED_TO_BOSS:
            return "treasure room is not connected to boss";
        case DUNGEON_TOPOLOGY_INVALID_RIDDLE_ROOM:
            return "invalid Riddleman room";
        case DUNGEON_TOPOLOGY_RIDDLE_NOT_ON_BOSS_ROUTE:
            return "Riddleman room is not on the boss route";
        default: return "unknown topology validation error";
    }
}

bool generateDungeonTopology(
    DungeonTopology& dungeon,
    uint8_t targetRoomCount,
    SeededDungeonRandom& randomSource)
{
    return dungeon_graph_core::generateTopology(
        dungeon, targetRoomCount, randomSource);
}

bool generateDungeonLayout(
    DungeonTopology& dungeon,
    SeededDungeonRandom& randomSource,
    DungeonGenerationReport& report)
{
    return dungeon_graph_core::generateLayout(
        dungeon, randomSource, report);
}
