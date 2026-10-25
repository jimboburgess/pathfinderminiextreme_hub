#ifndef PATHFINDERMINIEXTREME_025_DUNGEON_TOPOLOGY_H
#define PATHFINDERMINIEXTREME_025_DUNGEON_TOPOLOGY_H

#include <stdint.h>

#include "data/direction.h"

constexpr uint8_t NO_ROOM = 255;
constexpr int8_t NO_DUNGEON_COORDINATE = 127;
constexpr uint8_t MIN_DUNGEON_ROOMS = 7;
constexpr uint8_t MAX_DUNGEON_ROOMS = 12;
constexpr uint8_t MAX_ROOMS = MAX_DUNGEON_ROOMS;
constexpr uint8_t MIN_BOSS_GRAPH_DISTANCE = 4;
constexpr uint8_t DUNGEON_LOOP_CHANCE_PERCENT = 25;
constexpr uint8_t MAX_DUNGEON_GENERATION_ATTEMPTS = 24;

enum RoomType : uint8_t
{
    ROOM_ENTRANCE,
    ROOM_COMBAT,
    ROOM_AMBUSH,
    ROOM_PUZZLE,
    ROOM_TREASURE,
    ROOM_EMPTY,
    ROOM_BOSS
};

enum DungeonTopologyValidationError : uint8_t
{
    DUNGEON_TOPOLOGY_VALID,
    DUNGEON_TOPOLOGY_INVALID_ROOM_COUNT,
    DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM,
    DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_TYPE,
    DUNGEON_TOPOLOGY_UNPLACED_COORDINATE,
    DUNGEON_TOPOLOGY_DUPLICATE_COORDINATE,
    DUNGEON_TOPOLOGY_INVALID_NEIGHBOR,
    DUNGEON_TOPOLOGY_SELF_LINK,
    DUNGEON_TOPOLOGY_NONRECIPROCAL_LINK,
    DUNGEON_TOPOLOGY_INVALID_NEIGHBOR_COORDINATE,
    DUNGEON_TOPOLOGY_UNREACHABLE_ROOM,
    DUNGEON_TOPOLOGY_INVALID_SPECIAL_ROOM_COUNT,
    DUNGEON_TOPOLOGY_MISSING_BRANCH,
    DUNGEON_TOPOLOGY_BOSS_TOO_CLOSE,
    DUNGEON_TOPOLOGY_INVALID_BOSS_DEGREE,
    DUNGEON_TOPOLOGY_INVALID_TREASURE_DEGREE,
    DUNGEON_TOPOLOGY_TREASURE_NOT_CONNECTED_TO_BOSS,
    DUNGEON_TOPOLOGY_INVALID_RIDDLE_ROOM,
    DUNGEON_TOPOLOGY_RIDDLE_NOT_ON_BOSS_ROUTE
};

struct DungeonTopologyRoom
{
    RoomType type = ROOM_EMPTY;
    uint8_t north = NO_ROOM;
    uint8_t south = NO_ROOM;
    uint8_t east = NO_ROOM;
    uint8_t west = NO_ROOM;
    int8_t dungeonX = NO_DUNGEON_COORDINATE;
    int8_t dungeonY = NO_DUNGEON_COORDINATE;
};

struct DungeonTopology
{
    DungeonTopologyRoom rooms[MAX_ROOMS];
    uint8_t roomCount = 0;
    uint8_t bossRoom = NO_ROOM;
    uint8_t treasureRoom = NO_ROOM;
    uint8_t riddleRoom = NO_ROOM;
};

struct DungeonGenerationReport
{
    uint8_t attempts = 0;
    Direction riddleExitDirection = DIR_NORTH;
};

class SeededDungeonRandom
{
public:
    explicit SeededDungeonRandom(uint32_t seed);
    uint32_t next(uint32_t upperExclusive);

private:
    uint32_t state;
};

uint8_t getRoomNeighbor(
    const DungeonTopologyRoom& room, Direction direction);
uint8_t getRoomDegree(const DungeonTopologyRoom& room);
bool isDeadEndRoom(const DungeonTopologyRoom& room);
int8_t findRoomAtGraphCoordinate(
    const DungeonTopology& dungeon, int8_t dungeonX, int8_t dungeonY);
uint8_t getRoomDistanceFromEntrance(
    const DungeonTopology& dungeon, uint8_t roomIndex);
bool dungeonGraphHasBranch(const DungeonTopology& dungeon);
bool dungeonGraphHasLoop(const DungeonTopology& dungeon);
DungeonTopologyValidationError validateDungeonTopologyDetailed(
    const DungeonTopology& dungeon);
DungeonTopologyValidationError validateDungeonLayoutDetailed(
    const DungeonTopology& dungeon);
const char* dungeonTopologyValidationErrorName(
    DungeonTopologyValidationError error);

bool generateDungeonTopology(
    DungeonTopology& dungeon,
    uint8_t targetRoomCount,
    SeededDungeonRandom& randomSource);
bool generateDungeonLayout(
    DungeonTopology& dungeon,
    SeededDungeonRandom& randomSource,
    DungeonGenerationReport& report);

#endif
