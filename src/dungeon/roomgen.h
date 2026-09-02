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

constexpr uint8_t PILLAR_ROOM_CHANCE_PERCENT = 30;
constexpr uint8_t BOSS_PILLAR_ROOM_CHANCE_PERCENT = 50;
constexpr uint8_t PILLAR_MIN_WALKABLE_TILES = 72;
constexpr uint8_t PILLAR_MIN_INTERIOR_SPAN = 9;
constexpr uint8_t BARREL_CLUSTER_CHANCE_PERCENT = 30;
constexpr uint8_t CRATE_CLUSTER_CHANCE_PERCENT = 30;
constexpr uint8_t STATUE_CHANCE_PERCENT = 20;
constexpr uint8_t BRAZIER_CHANCE_PERCENT = 20;

enum PillarLayoutType : uint8_t
{
    PILLAR_LAYOUT_SQUARE,
    PILLAR_LAYOUT_ROW_HORIZONTAL,
    PILLAR_LAYOUT_ROW_VERTICAL,
    PILLAR_LAYOUT_HEXAGON,
    PILLAR_LAYOUT_COUNT
};

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

bool isRoomEligibleForPillars(const DungeonRoom& room);
uint8_t placePillarLayout(
    DungeonRoom& room,
    PillarLayoutType layout,
    int centerX,
    int centerY);
uint8_t populatePillarTerrain(
    DungeonRoom& room,
    uint8_t chanceRoll,
    uint8_t layoutRoll);
bool isRoomEligibleForFurniture(const DungeonRoom& room);
uint8_t populateDungeonFurniture(
    DungeonRoom& room,
    uint8_t barrelRoll,
    uint8_t crateRoll,
    uint8_t statueRoll,
    uint8_t brazierRoll);

// Places a small authored/generated patch without replacing doors, content
// markers, or protected room-entry squares. Returns the tiles converted.
uint8_t placeRubblePatch(
    DungeonRoom& room,
    uint8_t originX,
    uint8_t originY,
    uint8_t maximumTiles);
uint8_t populateRubbleTerrain(DungeonRoom& room);
uint8_t populateBossRubbleTerrain(DungeonRoom& room);

void generateRoom(DungeonRoom& room);
#endif //PATHFINDERMINIEXTREME_025_ROOMGEN_H
