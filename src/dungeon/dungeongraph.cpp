#include "dungeon/dungeongraph.h"

#include <Arduino.h>

#include "dungeon/dungeongraph_core.h"

namespace
{
class ArduinoDungeonRandom
{
public:
    uint32_t next(uint32_t upperExclusive)
    {
        return static_cast<uint32_t>(
            random(static_cast<long>(upperExclusive)));
    }
};
}

uint8_t getRoomNeighbor(const DungeonRoom& room, Direction direction)
{
    return dungeon_graph_core::getRoomNeighbor(room, direction);
}

uint8_t getRoomDegree(const DungeonRoom& room)
{
    return dungeon_graph_core::getRoomDegree(room);
}

bool isDeadEndRoom(const DungeonRoom& room)
{
    return dungeon_graph_core::isDeadEndRoom(room);
}

int8_t findRoomAtGraphCoordinate(
    const Dungeon& dungeon, int8_t dungeonX, int8_t dungeonY)
{
    return dungeon_graph_core::findRoomAtGraphCoordinate(
        dungeon, dungeonX, dungeonY);
}

uint8_t getRoomDistanceFromEntrance(
    const Dungeon& dungeon, uint8_t roomIndex)
{
    return dungeon_graph_core::getRoomDistanceFromEntrance(
        dungeon, roomIndex);
}

bool dungeonGraphHasBranch(const Dungeon& dungeon)
{
    return dungeon_graph_core::dungeonGraphHasBranch(dungeon);
}

bool dungeonGraphHasLoop(const Dungeon& dungeon)
{
    return dungeon_graph_core::dungeonGraphHasLoop(dungeon);
}

bool validateDungeonTopology(const Dungeon& dungeon)
{
    return dungeon_graph_core::validateTopologyDetailed(dungeon) ==
        DUNGEON_TOPOLOGY_VALID;
}

bool validateDungeonLayout(const Dungeon& dungeon)
{
    return dungeon_graph_core::validateLayoutDetailed(dungeon) ==
        DUNGEON_TOPOLOGY_VALID;
}

bool generateDungeonTopology(Dungeon& dungeon, uint8_t targetRoomCount)
{
    ArduinoDungeonRandom randomSource;
    return dungeon_graph_core::generateTopology(
        dungeon, targetRoomCount, randomSource);
}

bool generateDungeonLayout(
    Dungeon& dungeon, Direction& riddleExitDirection)
{
    ArduinoDungeonRandom randomSource;
    DungeonGenerationReport report;
    if (!dungeon_graph_core::generateLayout(
            dungeon, randomSource, report))
        return false;
    riddleExitDirection = report.riddleExitDirection;
    return true;
}

void dumpDungeonTopology(const Dungeon& dungeon)
{
#if defined(DEBUG) || defined(_DEBUG)
    Serial.printf("Dungeon rooms: %u\n", dungeon.roomCount);
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
    {
        const DungeonRoom& room = dungeon.rooms[i];
        Serial.printf("%u %s (%d,%d): N=%d E=%d S=%d W=%d\n",
            i, roomTypeName(room.type), room.dungeonX, room.dungeonY,
            room.north, room.east, room.south, room.west);
    }
#else
    (void)dungeon;
#endif
}
