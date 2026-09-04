#ifndef PATHFINDERMINIEXTREME_025_DUNGEON_GRAPH_H
#define PATHFINDERMINIEXTREME_025_DUNGEON_GRAPH_H

#include "dungeon/dungeon.h"

uint8_t getRoomNeighbor(const DungeonRoom& room, Direction direction);
uint8_t getRoomDegree(const DungeonRoom& room);
bool isDeadEndRoom(const DungeonRoom& room);
int8_t findRoomAtGraphCoordinate(
    const Dungeon& dungeon, int8_t dungeonX, int8_t dungeonY);
uint8_t getRoomDistanceFromEntrance(
    const Dungeon& dungeon, uint8_t roomIndex);
bool dungeonGraphHasBranch(const Dungeon& dungeon);
bool dungeonGraphHasLoop(const Dungeon& dungeon);
bool validateDungeonTopology(const Dungeon& dungeon);

// Builds only room topology and special-room indices. Individual 15x15 room
// geometry and contents remain owned by the normal room generator.
bool generateDungeonTopology(Dungeon& dungeon, uint8_t targetRoomCount);
void dumpDungeonTopology(const Dungeon& dungeon);

#endif
