//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ROOMGEN_H
#define PATHFINDERMINIEXTREME_025_ROOMGEN_H

#include "dungeon.h"

// Fixed ROOM_ENTRANCE geometry. The main passage is four tiles high, while
// the paired alcoves remain off the critical left-to-right route.
constexpr uint8_t ENTRANCE_HALL_X = 1;
constexpr uint8_t ENTRANCE_HALL_Y = 5;
constexpr uint8_t ENTRANCE_HALL_WIDTH = 13;
constexpr uint8_t ENTRANCE_HALL_HEIGHT = 4;
constexpr uint8_t ENTRANCE_ALCOVE_X = 8;
constexpr uint8_t ENTRANCE_ALCOVE_WIDTH = 4;
constexpr uint8_t ENTRANCE_ALCOVE_HEIGHT = 4;
constexpr uint8_t ENTRANCE_FOUNTAIN_ALCOVE_Y = 1;
constexpr uint8_t ENTRANCE_SERVICE_ALCOVE_Y = 9;
constexpr uint8_t ENTRANCE_FOUNTAIN_X = 9;
constexpr uint8_t ENTRANCE_FOUNTAIN_Y = 1;
constexpr uint8_t ENTRANCE_PLAYER_START_X = 2;
constexpr uint8_t ENTRANCE_PLAYER_START_Y = 6;
constexpr uint8_t ENTRANCE_EAST_CONNECTION_Y = 6;

void clearRoomConnections(DungeonRoom& room);
bool addRoomConnection(
    DungeonRoom& room,
    Direction direction,
    uint8_t x,
    uint8_t y);
const RoomConnection* getRoomConnection(
    const DungeonRoom& room,
    Direction direction);
uint8_t randomRoomConnectionOffset();
void populateRoomConnections(DungeonRoom& room);
bool getRoomEntryPosition(
    const DungeonRoom& room,
    RoomEntry entry,
    uint8_t& x,
    uint8_t& y);
bool findNearestRoomFloor(
    const DungeonRoom& room,
    int originX,
    int originY,
    uint8_t& x,
    uint8_t& y);

void fillRoom(DungeonRoom& room, TileType tile);
bool carveFloorTile(DungeonRoom& room, int x, int y);
bool carveRectangle(
    DungeonRoom& room,
    int x,
    int y,
    int width,
    int height);
bool connectRoomConnectionToFloor(
    DungeonRoom& room,
    const RoomConnection& connection,
    uint8_t targetX,
    uint8_t targetY);
bool carveCorridorSegment(
    DungeonRoom& room,
    int startX,
    int startY,
    int endX,
    int endY,
    uint8_t width);
bool validateRoomConnectivity(const DungeonRoom& room);

bool isWindingCorridorEligible(const DungeonRoom& room);
bool isCaveEligible(const DungeonRoom& room);
uint8_t selectCorridorWidth(uint8_t roll);
uint8_t selectCaveChamberCount(uint8_t roll);
uint8_t selectCaveTargetCoverage(uint8_t roll);
uint8_t getRoomFloorCoveragePercent(const DungeonRoom& room);
bool caveHasLongOneTileTunnel(const DungeonRoom& room);
RoomShape selectProductionRoomShape(
    const DungeonRoom& room,
    uint8_t roll);
RoomShape randomProductionRoomShape(const DungeonRoom& room);

bool placeGiantSpiderEncounter(DungeonRoom& room);

void generateRoom(DungeonRoom& room);
#endif //PATHFINDERMINIEXTREME_025_ROOMGEN_H
