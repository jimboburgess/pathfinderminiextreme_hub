//
// Created by james on 7/12/2026.
//

#include "roomgen.h"
#include <Arduino.h>


constexpr uint8_t SMALL_ROOM_MIN_SIZE = 6;
constexpr uint8_t SMALL_ROOM_MAX_SIZE = 10;
constexpr uint8_t L_ROOM_MIN_SIZE = 8;
constexpr uint8_t L_ROOM_MAX_SIZE = 11;
constexpr uint8_t L_ROOM_MIN_ARM = 4;
constexpr uint8_t L_ROOM_MAX_ARM = 6;
constexpr uint8_t CORRIDOR_MIN_WIDTH = 1;
constexpr uint8_t CORRIDOR_MAX_WIDTH = 2;
constexpr uint8_t CORRIDOR_MIN_SEGMENT_LENGTH = 2;
constexpr uint8_t CORRIDOR_TURN_MIN = 3;
constexpr uint8_t CORRIDOR_TURN_MAX = ROOM_SIZE - 4;
constexpr uint8_t MAX_CORRIDOR_POINTS = 6;
constexpr uint8_t MAX_CORRIDOR_GENERATION_ATTEMPTS = 8;
constexpr uint8_t CAVE_MIN_TARGET_COVERAGE = 50;
constexpr uint8_t CAVE_MAX_TARGET_COVERAGE = 65;
constexpr uint8_t CAVE_MIN_ACCEPTED_COVERAGE = 48;
constexpr uint8_t CAVE_MAX_ACCEPTED_COVERAGE = 70;
constexpr uint8_t CAVE_COVERAGE_TOLERANCE = 8;
constexpr uint8_t CAVE_MAX_CHAMBERS = 4;
constexpr uint8_t MAX_CAVE_GENERATION_ATTEMPTS = 16;
constexpr uint8_t CAVE_CONNECTOR_WIDTH = 2;
constexpr uint16_t ROOM_INTERIOR_AREA =
    (ROOM_SIZE - 2) * (ROOM_SIZE - 2);

static_assert(
    CORRIDOR_TURN_MIN <= CORRIDOR_TURN_MAX,
    "ROOM_SIZE is too small for winding corridor turns");

struct RoomFloorAnchor {
  uint8_t x;
  uint8_t y;
};

struct CorridorPoint {
  uint8_t x;
  uint8_t y;
};

struct CorridorPath {
  CorridorPoint points[MAX_CORRIDOR_POINTS];
  uint8_t pointCount;
  uint8_t width;
};

struct CaveBlob {
  uint8_t centerX;
  uint8_t centerY;
  uint8_t radiusX;
  uint8_t radiusY;
  uint8_t irregularitySeed;
};

static bool createSquareRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createSmallRectangleRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createLRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createWindingCorridorRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createCaveRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createCrossRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static bool createCircleRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor);
static void addDoors(DungeonRoom &room);
static bool buildRoomGeometry(DungeonRoom &room, RoomShape shape);
static bool placeContentMarkerNear(
    DungeonRoom &room,
    TileType marker,
    int preferredX,
    int preferredY,
    uint8_t width = 1,
    uint8_t height = 1);
static bool generateEntrance(DungeonRoom &room);
static void generateCombat(DungeonRoom &room);
static void generatePuzzle(DungeonRoom &room);
static void generateAmbush(DungeonRoom &room);
static void generateBoss(DungeonRoom &room);
static void generateTreasure(DungeonRoom &room);


static bool isValidRoomConnection(
    Direction direction,
    uint8_t x,
    uint8_t y)
{
  if (x >= ROOM_SIZE || y >= ROOM_SIZE)
    return false;

  switch (direction) {
    case DIR_NORTH:
      return y == 0 &&
             x >= ROOM_CONNECTION_MIN &&
             x <= ROOM_CONNECTION_MAX;

    case DIR_EAST:
      return x == ROOM_SIZE - 1 &&
             y >= ROOM_CONNECTION_MIN &&
             y <= ROOM_CONNECTION_MAX;

    case DIR_SOUTH:
      return y == ROOM_SIZE - 1 &&
             x >= ROOM_CONNECTION_MIN &&
             x <= ROOM_CONNECTION_MAX;

    case DIR_WEST:
      return x == 0 &&
             y >= ROOM_CONNECTION_MIN &&
             y <= ROOM_CONNECTION_MAX;

    default:
      return false;
  }
}

void clearRoomConnections(DungeonRoom &room) {
  room.connectionCount = 0;

  for (uint8_t i = 0; i < MAX_ROOM_CONNECTIONS; i++) {
    room.connections[i] = {DIR_NORTH, 0, 0};
  }
}

bool addRoomConnection(
    DungeonRoom &room,
    Direction direction,
    uint8_t x,
    uint8_t y) {
  if (room.connectionCount >= MAX_ROOM_CONNECTIONS ||
      !isValidRoomConnection(direction, x, y)) {
    return false;
  }

  room.connections[room.connectionCount++] = {direction, x, y};
  return true;
}

const RoomConnection* getRoomConnection(
    const DungeonRoom &room,
    Direction direction) {
  uint8_t count = room.connectionCount;

  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    if (room.connections[i].direction == direction)
      return &room.connections[i];
  }

  return nullptr;
}

uint8_t randomRoomConnectionOffset() {
  return static_cast<uint8_t>(random(
      ROOM_CONNECTION_MIN,
      ROOM_CONNECTION_MAX + 1));
}

bool isWindingCorridorEligible(const DungeonRoom &room) {
  return room.connectionCount == 2 && room.type != ROOM_ENTRANCE;
}

bool isCaveEligible(const DungeonRoom &room) {
  return room.connectionCount >= 1 &&
         room.connectionCount <= MAX_ROOM_CONNECTIONS;
}

uint8_t selectCorridorWidth(uint8_t roll) {
  return roll % 4 == 3 ? CORRIDOR_MAX_WIDTH : CORRIDOR_MIN_WIDTH;
}

uint8_t selectCaveChamberCount(uint8_t roll) {
  roll %= 100;

  if (roll < 35)
    return 1;

  if (roll < 70)
    return 2;

  if (roll < 90)
    return 3;

  return 4;
}

uint8_t selectCaveTargetCoverage(uint8_t roll) {
  return static_cast<uint8_t>(
      CAVE_MIN_TARGET_COVERAGE +
      roll % (CAVE_MAX_TARGET_COVERAGE -
              CAVE_MIN_TARGET_COVERAGE + 1));
}

RoomShape selectProductionRoomShape(
    const DungeonRoom &room,
    uint8_t roll) {
  roll %= 100;

  if (roll < 25)
    return SHAPE_SQUARE;

  if (roll < 50)
    return SHAPE_SMALL_RECTANGLE;

  if (roll < 70)
    return SHAPE_L;

  if (roll < 90)
    return isCaveEligible(room) ? SHAPE_CAVE : SHAPE_SQUARE;

  if (isWindingCorridorEligible(room))
    return SHAPE_WINDING_CORRIDOR;

  // One-connection entrance/end rooms cannot use winding corridors. Keep
  // that ten-percent band varied instead of silently converting it into full
  // rectangles.
  return roll < 95 ? SHAPE_SMALL_RECTANGLE : SHAPE_L;
}

RoomShape randomProductionRoomShape(const DungeonRoom &room) {
  return selectProductionRoomShape(
      room,
      static_cast<uint8_t>(random(100)));
}

void populateRoomConnections(DungeonRoom &room) {
  clearRoomConnections(room);

  if (room.type == ROOM_ENTRANCE) {
    // The fixed entrance is deliberately oriented west-to-east. Its stored
    // RoomConnection remains the authoritative transition point.
    if (room.east != NO_ROOM) {
      addRoomConnection(
          room,
          DIR_EAST,
          ROOM_SIZE - 1,
          ENTRANCE_EAST_CONNECTION_Y);
    }
    return;
  }

  if (room.north != NO_ROOM)
    addRoomConnection(
        room, DIR_NORTH, randomRoomConnectionOffset(), 0);

  if (room.south != NO_ROOM)
    addRoomConnection(
        room,
        DIR_SOUTH,
        randomRoomConnectionOffset(),
        ROOM_SIZE - 1);

  if (room.west != NO_ROOM)
    addRoomConnection(
        room, DIR_WEST, 0, randomRoomConnectionOffset());

  if (room.east != NO_ROOM)
    addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        randomRoomConnectionOffset());
}

bool findNearestRoomFloor(
    const DungeonRoom &room,
    int originX,
    int originY,
    uint8_t &x,
    uint8_t &y) {
  int bestX = -1;
  int bestY = -1;
  int bestDistance = 32767;

  for (int tileY = 1; tileY < ROOM_SIZE - 1; tileY++) {
    for (int tileX = 1; tileX < ROOM_SIZE - 1; tileX++) {
      if (room.map.tiles[tileY][tileX] != TILE_FLOOR)
        continue;

      const int dx = tileX > originX
          ? tileX - originX
          : originX - tileX;
      const int dy = tileY > originY
          ? tileY - originY
          : originY - tileY;
      const int distance = dx + dy;

      if (distance < bestDistance) {
        bestDistance = distance;
        bestX = tileX;
        bestY = tileY;
      }
    }
  }

  if (bestX < 0)
    return false;

  x = static_cast<uint8_t>(bestX);
  y = static_cast<uint8_t>(bestY);
  return true;
}

bool getRoomEntryPosition(
    const DungeonRoom &room,
    RoomEntry entry,
    uint8_t &x,
    uint8_t &y) {
  if (entry == ENTRY_START) {
    if (room.type == ROOM_ENTRANCE &&
        room.map.tiles[ENTRANCE_PLAYER_START_Y]
                      [ENTRANCE_PLAYER_START_X] == TILE_FLOOR) {
      x = ENTRANCE_PLAYER_START_X;
      y = ENTRANCE_PLAYER_START_Y;
      return true;
    }

    const uint8_t center = ROOM_SIZE / 2;

    if (room.map.tiles[center][center] == TILE_FLOOR) {
      x = center;
      y = center;
      return true;
    }

    return findNearestRoomFloor(room, center, center, x, y);
  }

  Direction direction;

  switch (entry) {
    case ENTRY_NORTH:
      direction = DIR_NORTH;
      break;

    case ENTRY_EAST:
      direction = DIR_EAST;
      break;

    case ENTRY_SOUTH:
      direction = DIR_SOUTH;
      break;

    case ENTRY_WEST:
      direction = DIR_WEST;
      break;

    default:
      return false;
  }

  const RoomConnection* connection =
      getRoomConnection(room, direction);

  if (connection == nullptr ||
      !isValidRoomConnection(
          connection->direction,
          connection->x,
          connection->y)) {
    return false;
  }

  switch (direction) {
    case DIR_NORTH:
      x = connection->x;
      y = 1;
      break;

    case DIR_EAST:
      x = ROOM_SIZE - 2;
      y = connection->y;
      break;

    case DIR_SOUTH:
      x = connection->x;
      y = ROOM_SIZE - 2;
      break;

    case DIR_WEST:
      x = 1;
      y = connection->y;
      break;

    default:
      return false;
  }

  return room.map.tiles[y][x] == TILE_FLOOR;
}




void fillRoom(DungeonRoom &room, TileType tile) {
  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      room.map.tiles[y][x] = tile;
    }
  }
}

bool carveFloorTile(DungeonRoom &room, int x, int y) {
  if (x < 0 || x >= ROOM_SIZE || y < 0 || y >= ROOM_SIZE)
    return false;

  room.map.tiles[y][x] = TILE_FLOOR;
  return true;
}

bool carveRectangle(
    DungeonRoom &room,
    int x,
    int y,
    int width,
    int height) {
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > ROOM_SIZE || y + height > ROOM_SIZE) {
    return false;
  }

  for (int tileY = y; tileY < y + height; tileY++) {
    for (int tileX = x; tileX < x + width; tileX++) {
      room.map.tiles[tileY][tileX] = TILE_FLOOR;
    }
  }

  return true;
}

static bool isInsideRoomInterior(int x, int y) {
  return x >= 1 && x < ROOM_SIZE - 1 &&
         y >= 1 && y < ROOM_SIZE - 1;
}

static bool carveCorridorPoint(
    DungeonRoom &room,
    int x,
    int y,
    uint8_t width) {
  if (!isInsideRoomInterior(x, y) ||
      width < CORRIDOR_MIN_WIDTH || width > CORRIDOR_MAX_WIDTH) {
    return false;
  }

  if (width == 1)
    return carveFloorTile(room, x, y);

  // Keep widening inside the outer wall. Near the south/east interior edge,
  // shift the second tile inward instead of overwriting the boundary.
  const int carveX = x == ROOM_SIZE - 2 ? x - 1 : x;
  const int carveY = y == ROOM_SIZE - 2 ? y - 1 : y;
  return carveRectangle(room, carveX, carveY, 2, 2);
}

bool carveCorridorSegment(
    DungeonRoom &room,
    int startX,
    int startY,
    int endX,
    int endY,
    uint8_t width) {
  if (!isInsideRoomInterior(startX, startY) ||
      !isInsideRoomInterior(endX, endY) ||
      width < CORRIDOR_MIN_WIDTH || width > CORRIDOR_MAX_WIDTH ||
      (startX != endX && startY != endY) ||
      (startX == endX && startY == endY)) {
    return false;
  }

  const int stepX = endX > startX ? 1 : endX < startX ? -1 : 0;
  const int stepY = endY > startY ? 1 : endY < startY ? -1 : 0;
  int x = startX;
  int y = startY;

  while (true) {
    if (!carveCorridorPoint(room, x, y, width))
      return false;

    if (x == endX && y == endY)
      return true;

    x += stepX;
    y += stepY;
  }
}

static bool isRoomGeometryWalkable(TileType tile) {
  switch (tile) {
    case TILE_FLOOR:
    case TILE_RUBBLE:
    case TILE_DOOR:
    case TILE_CHEST_SPAWN:
    case TILE_LOOT_SPAWN:
    case TILE_NPC_SPAWN:
    case TILE_PLAYER_START:
    case TILE_ENEMY_START:
    case TILE_GIANT_SPIDER_START:
    case TILE_SKELETON_MAGE_START:
    case TILE_SKELETON_START:
      return true;

    default:
      return false;
  }
}

static bool getConnectionInteriorPosition(
    const RoomConnection &connection,
    int &x,
    int &y) {
  if (!isValidRoomConnection(
        connection.direction,
        connection.x,
        connection.y)) {
    return false;
  }

  switch (connection.direction) {
    case DIR_NORTH:
      x = connection.x;
      y = 1;
      return true;

    case DIR_EAST:
      x = ROOM_SIZE - 2;
      y = connection.y;
      return true;

    case DIR_SOUTH:
      x = connection.x;
      y = ROOM_SIZE - 2;
      return true;

    case DIR_WEST:
      x = 1;
      y = connection.y;
      return true;

    default:
      return false;
  }
}

static bool carveManhattanPath(
    DungeonRoom &room,
    int startX,
    int startY,
    int endX,
    int endY,
    bool verticalFirst) {
  if (!carveFloorTile(room, startX, startY))
    return false;

  int x = startX;
  int y = startY;

  if (verticalFirst) {
    while (y != endY) {
      y += endY > y ? 1 : -1;
      if (!carveFloorTile(room, x, y))
        return false;
    }

    while (x != endX) {
      x += endX > x ? 1 : -1;
      if (!carveFloorTile(room, x, y))
        return false;
    }
  } else {
    while (x != endX) {
      x += endX > x ? 1 : -1;
      if (!carveFloorTile(room, x, y))
        return false;
    }

    while (y != endY) {
      y += endY > y ? 1 : -1;
      if (!carveFloorTile(room, x, y))
        return false;
    }
  }

  return true;
}

bool connectRoomConnectionToFloor(
    DungeonRoom &room,
    const RoomConnection &connection,
    uint8_t targetX,
    uint8_t targetY) {
  if (targetX >= ROOM_SIZE || targetY >= ROOM_SIZE ||
      room.map.tiles[targetY][targetX] != TILE_FLOOR) {
    return false;
  }

  int startX = 0;
  int startY = 0;

  if (!getConnectionInteriorPosition(connection, startX, startY))
    return false;

  const bool verticalFirst =
      connection.direction == DIR_NORTH ||
      connection.direction == DIR_SOUTH;

  return carveManhattanPath(
      room,
      startX,
      startY,
      targetX,
      targetY,
      verticalFirst);
}

bool validateRoomConnectivity(const DungeonRoom &room) {
  bool visited[ROOM_SIZE][ROOM_SIZE] = {};
  uint8_t queue[ROOM_SIZE * ROOM_SIZE] = {};
  uint16_t head = 0;
  uint16_t tail = 0;
  int startX = -1;
  int startY = -1;

  for (int y = 0; y < ROOM_SIZE && startX < 0; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      if (isRoomGeometryWalkable(room.map.tiles[y][x])) {
        startX = x;
        startY = y;
        break;
      }
    }
  }

  if (startX < 0)
    return false;

  visited[startY][startX] = true;
  queue[tail++] = static_cast<uint8_t>(startY * ROOM_SIZE + startX);

  static constexpr int8_t neighborX[4] = {1, -1, 0, 0};
  static constexpr int8_t neighborY[4] = {0, 0, 1, -1};

  while (head < tail) {
    const uint8_t index = queue[head++];
    const int x = index % ROOM_SIZE;
    const int y = index / ROOM_SIZE;

    for (uint8_t direction = 0; direction < 4; direction++) {
      const int nextX = x + neighborX[direction];
      const int nextY = y + neighborY[direction];

      if (nextX < 0 || nextX >= ROOM_SIZE ||
          nextY < 0 || nextY >= ROOM_SIZE ||
          visited[nextY][nextX] ||
          !isRoomGeometryWalkable(room.map.tiles[nextY][nextX])) {
        continue;
      }

      visited[nextY][nextX] = true;
      queue[tail++] = static_cast<uint8_t>(nextY * ROOM_SIZE + nextX);
    }
  }

  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      if (isRoomGeometryWalkable(room.map.tiles[y][x]) &&
          !visited[y][x]) {
        return false;
      }
    }
  }

  uint8_t count = room.connectionCount;

  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    const RoomConnection &connection = room.connections[i];
    int interiorX = 0;
    int interiorY = 0;

    if (!getConnectionInteriorPosition(
          connection, interiorX, interiorY) ||
        room.map.tiles[connection.y][connection.x] != TILE_DOOR ||
        !isRoomGeometryWalkable(room.map.tiles[interiorY][interiorX]) ||
        !visited[connection.y][connection.x] ||
        !visited[interiorY][interiorX]) {
      return false;
    }
  }

  return true;
}

static uint8_t randomInclusive(uint8_t minimum, uint8_t maximum) {
  return static_cast<uint8_t>(random(minimum, maximum + 1));
}

static uint8_t coordinateDistance(int first, int second) {
  return static_cast<uint8_t>(
      first > second ? first - second : second - first);
}

static bool isNorthSouthConnection(Direction direction) {
  return direction == DIR_NORTH || direction == DIR_SOUTH;
}

static bool areOppositeConnections(
    Direction first,
    Direction second) {
  return (first == DIR_NORTH && second == DIR_SOUTH) ||
         (first == DIR_SOUTH && second == DIR_NORTH) ||
         (first == DIR_WEST && second == DIR_EAST) ||
         (first == DIR_EAST && second == DIR_WEST);
}

static bool chooseTurnCoordinate(
    int avoidFirst,
    int avoidSecond,
    uint8_t minimumDistance,
    uint8_t &coordinate) {
  uint8_t candidates[
      CORRIDOR_TURN_MAX - CORRIDOR_TURN_MIN + 1] = {};
  uint8_t candidateCount = 0;

  for (uint8_t value = CORRIDOR_TURN_MIN;
       value <= CORRIDOR_TURN_MAX;
       value++) {
    if (coordinateDistance(value, avoidFirst) >= minimumDistance &&
        coordinateDistance(value, avoidSecond) >= minimumDistance) {
      candidates[candidateCount++] = value;
    }
  }

  if (candidateCount == 0)
    return false;

  coordinate = candidates[random(candidateCount)];
  return true;
}

static bool chooseProgressiveTurns(
    int start,
    int end,
    uint8_t &firstTurn,
    uint8_t &secondTurn) {
  if (start == 1 && end == ROOM_SIZE - 2) {
    firstTurn = randomInclusive(3, 5);
    secondTurn = randomInclusive(9, ROOM_SIZE - 4);
    return true;
  }

  if (start == ROOM_SIZE - 2 && end == 1) {
    firstTurn = randomInclusive(9, ROOM_SIZE - 4);
    secondTurn = randomInclusive(3, 5);
    return true;
  }

  return false;
}

static bool rangesOverlap(
    int firstStart,
    int firstEnd,
    int secondStart,
    int secondEnd) {
  if (firstStart > firstEnd) {
    const int temporary = firstStart;
    firstStart = firstEnd;
    firstEnd = temporary;
  }

  if (secondStart > secondEnd) {
    const int temporary = secondStart;
    secondStart = secondEnd;
    secondEnd = temporary;
  }

  return firstStart <= secondEnd && secondStart <= firstEnd;
}

static bool corridorSegmentsIntersect(
    const CorridorPoint &firstStart,
    const CorridorPoint &firstEnd,
    const CorridorPoint &secondStart,
    const CorridorPoint &secondEnd) {
  const bool firstHorizontal = firstStart.y == firstEnd.y;
  const bool secondHorizontal = secondStart.y == secondEnd.y;

  if (firstHorizontal && secondHorizontal) {
    return firstStart.y == secondStart.y &&
           rangesOverlap(
               firstStart.x,
               firstEnd.x,
               secondStart.x,
               secondEnd.x);
  }

  if (!firstHorizontal && !secondHorizontal) {
    return firstStart.x == secondStart.x &&
           rangesOverlap(
               firstStart.y,
               firstEnd.y,
               secondStart.y,
               secondEnd.y);
  }

  const CorridorPoint &horizontalStart =
      firstHorizontal ? firstStart : secondStart;
  const CorridorPoint &horizontalEnd =
      firstHorizontal ? firstEnd : secondEnd;
  const CorridorPoint &verticalStart =
      firstHorizontal ? secondStart : firstStart;
  const CorridorPoint &verticalEnd =
      firstHorizontal ? secondEnd : firstEnd;

  return rangesOverlap(
             horizontalStart.x,
             horizontalEnd.x,
             verticalStart.x,
             verticalStart.x) &&
         rangesOverlap(
             verticalStart.y,
             verticalEnd.y,
             horizontalStart.y,
             horizontalStart.y);
}

static uint8_t getCorridorBendCount(const CorridorPath &path) {
  return path.pointCount >= 2 ? path.pointCount - 2 : 0;
}

static bool validateCorridorPath(const CorridorPath &path) {
  if (path.pointCount < 4 || path.pointCount > MAX_CORRIDOR_POINTS ||
      path.width < CORRIDOR_MIN_WIDTH ||
      path.width > CORRIDOR_MAX_WIDTH) {
    return false;
  }

  const uint8_t bendCount = getCorridorBendCount(path);
  if (bendCount < 2 || bendCount > 4)
    return false;

  bool previousHorizontal = false;

  for (uint8_t i = 0; i < path.pointCount; i++) {
    if (!isInsideRoomInterior(path.points[i].x, path.points[i].y))
      return false;

    if (i == 0)
      continue;

    const CorridorPoint &start = path.points[i - 1];
    const CorridorPoint &end = path.points[i];
    const bool horizontal = start.y == end.y;
    const bool vertical = start.x == end.x;

    if (horizontal == vertical ||
        coordinateDistance(
            horizontal ? start.x : start.y,
            horizontal ? end.x : end.y) <
            CORRIDOR_MIN_SEGMENT_LENGTH ||
        (i > 1 && horizontal == previousHorizontal)) {
      return false;
    }

    previousHorizontal = horizontal;
  }

  const uint8_t segmentCount = path.pointCount - 1;
  for (uint8_t first = 0; first < segmentCount; first++) {
    for (uint8_t second = first + 2;
         second < segmentCount;
         second++) {
      if (corridorSegmentsIntersect(
            path.points[first],
            path.points[first + 1],
            path.points[second],
            path.points[second + 1])) {
        return false;
      }
    }
  }

  return true;
}

static bool generateWindingCorridorPath(
    const RoomConnection &startConnection,
    const RoomConnection &endConnection,
    uint8_t width,
    CorridorPath &path) {
  if (width < CORRIDOR_MIN_WIDTH || width > CORRIDOR_MAX_WIDTH)
    return false;

  int startX = 0;
  int startY = 0;
  int endX = 0;
  int endY = 0;

  if (!getConnectionInteriorPosition(
        startConnection, startX, startY) ||
      !getConnectionInteriorPosition(
        endConnection, endX, endY)) {
    return false;
  }

  path = {};
  path.width = width;
  const bool startVertical =
      isNorthSouthConnection(startConnection.direction);
  const bool endVertical =
      isNorthSouthConnection(endConnection.direction);
  const uint8_t minimumTurnDistance =
      CORRIDOR_MIN_SEGMENT_LENGTH + width - 1;

  if (startVertical == endVertical) {
    if (!areOppositeConnections(
          startConnection.direction,
          endConnection.direction)) {
      return false;
    }

    uint8_t firstTurn = 0;
    uint8_t secondTurn = 0;
    uint8_t crossTurn = 0;

    if (startVertical) {
      if (!chooseProgressiveTurns(
            startY, endY, firstTurn, secondTurn) ||
          !chooseTurnCoordinate(
              startX,
              endX,
              minimumTurnDistance,
              crossTurn)) {
        return false;
      }

      path.points[0] = {
          static_cast<uint8_t>(startX),
          static_cast<uint8_t>(startY)};
      path.points[1] = {static_cast<uint8_t>(startX), firstTurn};
      path.points[2] = {crossTurn, firstTurn};
      path.points[3] = {crossTurn, secondTurn};
      path.points[4] = {static_cast<uint8_t>(endX), secondTurn};
      path.points[5] = {
          static_cast<uint8_t>(endX),
          static_cast<uint8_t>(endY)};
    } else {
      if (!chooseProgressiveTurns(
            startX, endX, firstTurn, secondTurn) ||
          !chooseTurnCoordinate(
              startY,
              endY,
              minimumTurnDistance,
              crossTurn)) {
        return false;
      }

      path.points[0] = {
          static_cast<uint8_t>(startX),
          static_cast<uint8_t>(startY)};
      path.points[1] = {firstTurn, static_cast<uint8_t>(startY)};
      path.points[2] = {firstTurn, crossTurn};
      path.points[3] = {secondTurn, crossTurn};
      path.points[4] = {secondTurn, static_cast<uint8_t>(endY)};
      path.points[5] = {
          static_cast<uint8_t>(endX),
          static_cast<uint8_t>(endY)};
    }

    path.pointCount = 6;
    return validateCorridorPath(path);
  }

  uint8_t turnX = 0;
  uint8_t turnY = 0;
  if (!chooseTurnCoordinate(
        startX, endX, minimumTurnDistance, turnX) ||
      !chooseTurnCoordinate(
        startY, endY, minimumTurnDistance, turnY)) {
    return false;
  }

  path.points[0] = {
      static_cast<uint8_t>(startX),
      static_cast<uint8_t>(startY)};

  if (startVertical) {
    path.points[1] = {static_cast<uint8_t>(startX), turnY};
    path.points[2] = {turnX, turnY};
    path.points[3] = {turnX, static_cast<uint8_t>(endY)};
  } else {
    path.points[1] = {turnX, static_cast<uint8_t>(startY)};
    path.points[2] = {turnX, turnY};
    path.points[3] = {static_cast<uint8_t>(endX), turnY};
  }

  path.points[4] = {
      static_cast<uint8_t>(endX),
      static_cast<uint8_t>(endY)};
  path.pointCount = 5;
  return validateCorridorPath(path);
}

static bool carveCorridorPath(
    DungeonRoom &room,
    const CorridorPath &path) {
  if (!validateCorridorPath(path))
    return false;

  for (uint8_t i = 1; i < path.pointCount; i++) {
    if (!carveCorridorSegment(
          room,
          path.points[i - 1].x,
          path.points[i - 1].y,
          path.points[i].x,
          path.points[i].y,
          path.width)) {
      return false;
    }
  }

  return true;
}

static bool createWindingCorridorRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  if (!isWindingCorridorEligible(room))
    return false;

  for (uint8_t attempt = 0;
       attempt < MAX_CORRIDOR_GENERATION_ATTEMPTS;
       attempt++) {
    CorridorPath path{};
    const uint8_t width = selectCorridorWidth(
        static_cast<uint8_t>(random(4)));

    if (!generateWindingCorridorPath(
          room.connections[0],
          room.connections[1],
          width,
          path)) {
      continue;
    }

    fillRoom(room, TILE_WALL);
    if (!carveCorridorPath(room, path))
      continue;

    const CorridorPoint &middle = path.points[path.pointCount / 2];
    anchor = {middle.x, middle.y};
    return true;
  }

  return false;
}

static uint16_t countInteriorWalkableTiles(const DungeonRoom &room) {
  uint16_t count = 0;

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      if (isRoomGeometryWalkable(room.map.tiles[y][x]))
        count++;
    }
  }

  return count;
}

uint8_t getRoomFloorCoveragePercent(const DungeonRoom &room) {
  const uint16_t floorCount = countInteriorWalkableTiles(room);
  return static_cast<uint8_t>(
      (floorCount * 100U + ROOM_INTERIOR_AREA / 2U) /
      ROOM_INTERIOR_AREA);
}

static bool isCaveFloor(const DungeonRoom &room, int x, int y) {
  return isInsideRoomInterior(x, y) &&
         isRoomGeometryWalkable(room.map.tiles[y][x]);
}

bool caveHasLongOneTileTunnel(const DungeonRoom &room) {
  for (int x = 1; x < ROOM_SIZE - 1; x++) {
    uint8_t narrowRun = 0;

    for (int y = 1; y < ROOM_SIZE - 1; y++) {
      const bool verticallyNarrow =
          isCaveFloor(room, x, y) &&
          !isCaveFloor(room, x - 1, y) &&
          !isCaveFloor(room, x + 1, y);

      narrowRun = verticallyNarrow
          ? static_cast<uint8_t>(narrowRun + 1)
          : 0;

      if (narrowRun > 1)
        return true;
    }
  }

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    uint8_t narrowRun = 0;

    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      const bool horizontallyNarrow =
          isCaveFloor(room, x, y) &&
          !isCaveFloor(room, x, y - 1) &&
          !isCaveFloor(room, x, y + 1);

      narrowRun = horizontallyNarrow
          ? static_cast<uint8_t>(narrowRun + 1)
          : 0;

      if (narrowRun > 1)
        return true;
    }
  }

  return false;
}

static uint8_t clampCaveCenter(int value) {
  constexpr uint8_t minimum = 3;
  constexpr uint8_t maximum = ROOM_SIZE - 4;

  if (value < minimum)
    return minimum;

  if (value > maximum)
    return maximum;

  return static_cast<uint8_t>(value);
}

static uint8_t caveBlobBaseRadius(
    uint8_t chamberCount,
    uint8_t targetCoverage) {
  uint8_t radius = 3;

  if (chamberCount == 1)
    radius = 5;
  else if (chamberCount == 2)
    radius = 4;

  if (targetCoverage >= 60 && chamberCount <= 3)
    radius++;

  return radius;
}

static bool carveCaveBlob(
    DungeonRoom &room,
    const CaveBlob &blob) {
  if (!isInsideRoomInterior(blob.centerX, blob.centerY) ||
      blob.radiusX < 2 || blob.radiusY < 2) {
    return false;
  }

  const int radiusXSquared = blob.radiusX * blob.radiusX;
  const int radiusYSquared = blob.radiusY * blob.radiusY;
  const int ellipseLimit = radiusXSquared * radiusYSquared;
  bool carvedAny = false;

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      const int dx = x - blob.centerX;
      const int dy = y - blob.centerY;
      const int ellipseValue =
          dx * dx * radiusYSquared +
          dy * dy * radiusXSquared;

      if (ellipseValue > ellipseLimit)
        continue;

      // Keep the inner three quarters solid. On the outer rim, omit a small
      // deterministic subset of tiles so overlapping ovals read as an
      // irregular cave rather than a collection of perfect circles.
      const bool solidCore = ellipseValue * 4 <= ellipseLimit * 3;
      const uint8_t boundaryHash = static_cast<uint8_t>(
          x * 17 + y * 31 + blob.irregularitySeed * 13);

      if (solidCore || boundaryHash % 5 != 0) {
        carveFloorTile(room, x, y);
        carvedAny = true;
      }
    }
  }

  carveFloorTile(room, blob.centerX, blob.centerY);
  return carvedAny;
}

static bool carveWideManhattanPath(
    DungeonRoom &room,
    int startX,
    int startY,
    int endX,
    int endY,
    bool verticalFirst) {
  if (!isInsideRoomInterior(startX, startY) ||
      !isInsideRoomInterior(endX, endY) ||
      !carveCorridorPoint(
          room, startX, startY, CAVE_CONNECTOR_WIDTH)) {
    return false;
  }

  if (verticalFirst) {
    if (startY != endY &&
        !carveCorridorSegment(
            room,
            startX,
            startY,
            startX,
            endY,
            CAVE_CONNECTOR_WIDTH)) {
      return false;
    }

    if (startX != endX &&
        !carveCorridorSegment(
            room,
            startX,
            endY,
            endX,
            endY,
            CAVE_CONNECTOR_WIDTH)) {
      return false;
    }
  } else {
    if (startX != endX &&
        !carveCorridorSegment(
            room,
            startX,
            startY,
            endX,
            startY,
            CAVE_CONNECTOR_WIDTH)) {
      return false;
    }

    if (startY != endY &&
        !carveCorridorSegment(
            room,
            endX,
            startY,
            endX,
            endY,
            CAVE_CONNECTOR_WIDTH)) {
      return false;
    }
  }

  return true;
}

static bool findNearestExistingCaveFloor(
    const DungeonRoom &room,
    int originX,
    int originY,
    uint8_t &x,
    uint8_t &y) {
  return findNearestRoomFloor(room, originX, originY, x, y);
}

static bool connectCaveConnection(
    DungeonRoom &room,
    const RoomConnection &connection) {
  int startX = 0;
  int startY = 0;

  if (!getConnectionInteriorPosition(connection, startX, startY))
    return false;

  uint8_t targetX = 0;
  uint8_t targetY = 0;
  if (!findNearestExistingCaveFloor(
        room, startX, startY, targetX, targetY)) {
    return false;
  }

  const bool verticalFirst =
      connection.direction == DIR_NORTH ||
      connection.direction == DIR_SOUTH;
  return carveWideManhattanPath(
      room,
      startX,
      startY,
      targetX,
      targetY,
      verticalFirst);
}

static uint8_t countOrthogonalCaveNeighbors(
    const DungeonRoom &room,
    int x,
    int y) {
  uint8_t count = 0;
  count += isCaveFloor(room, x + 1, y) ? 1 : 0;
  count += isCaveFloor(room, x - 1, y) ? 1 : 0;
  count += isCaveFloor(room, x, y + 1) ? 1 : 0;
  count += isCaveFloor(room, x, y - 1) ? 1 : 0;
  return count;
}

static bool growCaveTowardCoverage(
    DungeonRoom &room,
    uint8_t desiredCoverage,
    uint8_t scanSeed) {
  const uint16_t desiredTiles = static_cast<uint16_t>(
      (ROOM_INTERIOR_AREA * desiredCoverage + 99U) / 100U);
  uint16_t floorCount = countInteriorWalkableTiles(room);

  while (floorCount < desiredTiles) {
    bool carved = false;

    for (uint16_t offset = 0;
         offset < ROOM_INTERIOR_AREA;
         offset++) {
      const uint16_t index = static_cast<uint16_t>(
          (offset + scanSeed) % ROOM_INTERIOR_AREA);
      const int x = 1 + index % (ROOM_SIZE - 2);
      const int y = 1 + index / (ROOM_SIZE - 2);

      if (room.map.tiles[y][x] == TILE_WALL &&
          countOrthogonalCaveNeighbors(room, x, y) >= 2) {
        carveFloorTile(room, x, y);
        scanSeed = static_cast<uint8_t>(index + 17);
        floorCount++;
        carved = true;
        break;
      }
    }

    if (!carved)
      return false;
  }

  return true;
}

static bool caveHasOpenCombatArea(const DungeonRoom &room) {
  for (int y = 1; y <= ROOM_SIZE - 4; y++) {
    for (int x = 1; x <= ROOM_SIZE - 4; x++) {
      bool open = true;

      for (int offsetY = 0; offsetY < 3 && open; offsetY++) {
        for (int offsetX = 0; offsetX < 3; offsetX++) {
          if (!isCaveFloor(room, x + offsetX, y + offsetY)) {
            open = false;
            break;
          }
        }
      }

      if (open)
        return true;
    }
  }

  return false;
}

static bool caveCoverageIsAcceptable(
    const DungeonRoom &room,
    uint8_t targetCoverage) {
  const uint8_t actualCoverage = getRoomFloorCoveragePercent(room);
  const uint8_t difference = actualCoverage > targetCoverage
      ? actualCoverage - targetCoverage
      : targetCoverage - actualCoverage;

  return actualCoverage >= CAVE_MIN_ACCEPTED_COVERAGE &&
         actualCoverage <= CAVE_MAX_ACCEPTED_COVERAGE &&
         difference <= CAVE_COVERAGE_TOLERANCE;
}

static bool createCaveAttempt(
    DungeonRoom &room,
    RoomFloorAnchor &anchor,
    uint8_t chamberCount,
    uint8_t targetCoverage,
    uint8_t layoutSeed) {
  if (!isCaveEligible(room) ||
      chamberCount < 1 || chamberCount > CAVE_MAX_CHAMBERS ||
      targetCoverage < CAVE_MIN_TARGET_COVERAGE ||
      targetCoverage > CAVE_MAX_TARGET_COVERAGE) {
    return false;
  }

  fillRoom(room, TILE_WALL);

  CaveBlob blobs[CAVE_MAX_CHAMBERS] = {};
  const uint8_t baseRadius = caveBlobBaseRadius(
      chamberCount, targetCoverage);
  blobs[0].centerX = randomInclusive(6, 8);
  blobs[0].centerY = randomInclusive(6, 8);
  blobs[0].radiusX = baseRadius;
  blobs[0].radiusY = static_cast<uint8_t>(
      baseRadius - (layoutSeed % 2));
  blobs[0].irregularitySeed = layoutSeed;
  anchor = {blobs[0].centerX, blobs[0].centerY};

  static constexpr int8_t directionX[8] =
      {1, 1, 0, -1, -1, -1, 0, 1};
  static constexpr int8_t directionY[8] =
      {0, 1, 1, 1, 0, -1, -1, -1};
  const uint8_t directionOffset = layoutSeed % 8;

  for (uint8_t i = 1; i < chamberCount; i++) {
    const uint8_t direction = static_cast<uint8_t>(
        (directionOffset + (i - 1) * 2) % 8);
    const uint8_t distance = static_cast<uint8_t>(
        3 + (layoutSeed + i) % 2);
    const int perpendicularJitter =
        static_cast<int>((layoutSeed + i * 3) % 3) - 1;
    const int offsetX =
        directionX[direction] * distance -
        directionY[direction] * perpendicularJitter;
    const int offsetY =
        directionY[direction] * distance +
        directionX[direction] * perpendicularJitter;

    blobs[i].centerX = clampCaveCenter(blobs[0].centerX + offsetX);
    blobs[i].centerY = clampCaveCenter(blobs[0].centerY + offsetY);
    blobs[i].radiusX = static_cast<uint8_t>(
        baseRadius - ((layoutSeed + i) % 2));
    blobs[i].radiusY = static_cast<uint8_t>(
        baseRadius - ((layoutSeed + i + 1) % 2));
    blobs[i].irregularitySeed = static_cast<uint8_t>(
        layoutSeed + i * 11);
  }

  for (uint8_t i = 0; i < chamberCount; i++) {
    if (!carveCaveBlob(room, blobs[i]))
      return false;

    if (i > 0 &&
        !carveWideManhattanPath(
            room,
            blobs[i].centerX,
            blobs[i].centerY,
            blobs[0].centerX,
            blobs[0].centerY,
            (layoutSeed + i) % 2 == 0)) {
      return false;
    }
  }

  uint8_t connectionCount = room.connectionCount;
  if (connectionCount > MAX_ROOM_CONNECTIONS)
    connectionCount = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < connectionCount; i++) {
    if (!connectCaveConnection(room, room.connections[i]))
      return false;
  }

  const uint8_t desiredCoverage = targetCoverage > 51
      ? static_cast<uint8_t>(targetCoverage - 3)
      : CAVE_MIN_ACCEPTED_COVERAGE;
  if (!growCaveTowardCoverage(
        room,
        desiredCoverage,
        static_cast<uint8_t>(layoutSeed * 19))) {
    return false;
  }

  addDoors(room);
  return caveCoverageIsAcceptable(room, targetCoverage) &&
         caveHasOpenCombatArea(room) &&
         !caveHasLongOneTileTunnel(room) &&
         validateRoomConnectivity(room);
}

static bool createCaveRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  if (!isCaveEligible(room))
    return false;

  for (uint8_t attempt = 0;
       attempt < MAX_CAVE_GENERATION_ATTEMPTS;
       attempt++) {
    const uint8_t chamberCount = selectCaveChamberCount(
        static_cast<uint8_t>(random(100)));
    const uint8_t targetCoverage = selectCaveTargetCoverage(
        static_cast<uint8_t>(random(256)));
    const uint8_t layoutSeed = static_cast<uint8_t>(
        random(256) + attempt * 23);

    if (createCaveAttempt(
          room,
          anchor,
          chamberCount,
          targetCoverage,
          layoutSeed)) {
      return true;
    }
  }

  return false;
}

static bool createSquareRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  fillRoom(room, TILE_WALL);
  anchor = {ROOM_SIZE / 2, ROOM_SIZE / 2};
  return carveRectangle(room, 1, 1, ROOM_SIZE - 2, ROOM_SIZE - 2);
}

static bool createSmallRectangleRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  fillRoom(room, TILE_WALL);

  const uint8_t width = randomInclusive(
      SMALL_ROOM_MIN_SIZE, SMALL_ROOM_MAX_SIZE);
  const uint8_t height = randomInclusive(
      SMALL_ROOM_MIN_SIZE, SMALL_ROOM_MAX_SIZE);
  const uint8_t x = randomInclusive(1, ROOM_SIZE - 1 - width);
  const uint8_t y = randomInclusive(1, ROOM_SIZE - 1 - height);

  anchor = {
      static_cast<uint8_t>(x + width / 2),
      static_cast<uint8_t>(y + height / 2)
  };

  return carveRectangle(room, x, y, width, height);
}

static bool createLRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  fillRoom(room, TILE_WALL);

  const uint8_t width = randomInclusive(
      L_ROOM_MIN_SIZE, L_ROOM_MAX_SIZE);
  const uint8_t height = randomInclusive(
      L_ROOM_MIN_SIZE, L_ROOM_MAX_SIZE);
  const uint8_t x = randomInclusive(1, ROOM_SIZE - 1 - width);
  const uint8_t y = randomInclusive(1, ROOM_SIZE - 1 - height);
  const uint8_t verticalArmWidth = randomInclusive(
      L_ROOM_MIN_ARM, L_ROOM_MAX_ARM);
  const uint8_t horizontalArmHeight = randomInclusive(
      L_ROOM_MIN_ARM, L_ROOM_MAX_ARM);
  const uint8_t orientation = static_cast<uint8_t>(random(4));
  const bool armOnRight = orientation == 1 || orientation == 3;
  const bool armOnBottom = orientation == 0 || orientation == 1;

  const uint8_t verticalX = armOnRight
      ? static_cast<uint8_t>(x + width - verticalArmWidth)
      : x;
  const uint8_t horizontalY = armOnBottom
      ? static_cast<uint8_t>(y + height - horizontalArmHeight)
      : y;

  const bool verticalCarved = carveRectangle(
      room, verticalX, y, verticalArmWidth, height);
  const bool horizontalCarved = carveRectangle(
      room, x, horizontalY, width, horizontalArmHeight);

  anchor = {
      static_cast<uint8_t>(verticalX + verticalArmWidth / 2),
      static_cast<uint8_t>(horizontalY + horizontalArmHeight / 2)
  };

  return verticalCarved && horizontalCarved;
}

static bool createCrossRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  fillRoom(room, TILE_WALL);
  const uint8_t center = ROOM_SIZE / 2;
  anchor = {center, center};

  return carveRectangle(room, center - 1, 1, 3, ROOM_SIZE - 2) &&
         carveRectangle(room, 1, center - 1, ROOM_SIZE - 2, 3);
}

static bool createCircleRoom(
    DungeonRoom &room,
    RoomFloorAnchor &anchor) {
  fillRoom(room, TILE_WALL);
  const uint8_t center = ROOM_SIZE / 2;
  anchor = {center, center};

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      const int dx = x - center;
      const int dy = y - center;

      if (dx * dx + dy * dy <= 42)
        carveFloorTile(room, x, y);
    }
  }

  return room.map.tiles[anchor.y][anchor.x] == TILE_FLOOR;
}

static void addDoors(DungeonRoom &room) {
  uint8_t count = room.connectionCount;

  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    const RoomConnection &connection = room.connections[i];

    if (isValidRoomConnection(
          connection.direction,
          connection.x,
          connection.y)) {
      room.map.tiles[connection.y][connection.x] = TILE_DOOR;
    }
  }
}

static bool buildRoomGeometry(DungeonRoom &room, RoomShape shape) {
  RoomFloorAnchor anchor = {0, 0};
  bool carved = false;
  bool connectionsAlreadyJoined = false;

  switch (shape) {
    case SHAPE_SQUARE:
      carved = createSquareRoom(room, anchor);
      break;

    case SHAPE_SMALL_RECTANGLE:
      carved = createSmallRectangleRoom(room, anchor);
      break;

    case SHAPE_L:
      carved = createLRoom(room, anchor);
      break;

    case SHAPE_WINDING_CORRIDOR:
      carved = createWindingCorridorRoom(room, anchor);
      connectionsAlreadyJoined = carved;
      break;

    case SHAPE_CAVE:
      carved = createCaveRoom(room, anchor);
      connectionsAlreadyJoined = carved;
      break;

    case SHAPE_CROSS:
      carved = createCrossRoom(room, anchor);
      break;

    case SHAPE_CIRCLE:
      carved = createCircleRoom(room, anchor);
      break;

    default:
      return false;
  }

  if (!carved)
    return false;

  if (!connectionsAlreadyJoined) {
    uint8_t count = room.connectionCount;

    if (count > MAX_ROOM_CONNECTIONS)
      count = MAX_ROOM_CONNECTIONS;

    for (uint8_t i = 0; i < count; i++) {
      if (!connectRoomConnectionToFloor(
            room,
            room.connections[i],
            anchor.x,
            anchor.y)) {
        return false;
      }
    }
  }

  addDoors(room);
  return validateRoomConnectivity(room);
}

static bool isReservedContentTile(
    const DungeonRoom &room,
    int x,
    int y) {
  if (room.type == ROOM_ENTRANCE) {
    uint8_t startX = 0;
    uint8_t startY = 0;

    if (getRoomEntryPosition(
          room, ENTRY_START, startX, startY) &&
        x == startX && y == startY) {
      return true;
    }
  }

  uint8_t count = room.connectionCount;

  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    int interiorX = 0;
    int interiorY = 0;

    if (getConnectionInteriorPosition(
          room.connections[i], interiorX, interiorY) &&
        x == interiorX && y == interiorY) {
      return true;
    }
  }

  return false;
}

static bool isNearRoomConnection(
    const DungeonRoom &room,
    int x,
    int y) {
  uint8_t count = room.connectionCount;
  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    int entryX = 0;
    int entryY = 0;
    if (!getConnectionInteriorPosition(
          room.connections[i], entryX, entryY)) {
      continue;
    }

    const int dx = x > entryX ? x - entryX : entryX - x;
    const int dy = y > entryY ? y - entryY : entryY - y;
    if (dx <= 1 && dy <= 1)
      return true;
  }

  return false;
}

struct PillarOffset {
  int8_t x;
  int8_t y;
};

static bool getPillarFloorBounds(
    const DungeonRoom &room,
    int &minimumX,
    int &minimumY,
    int &maximumX,
    int &maximumY,
    uint16_t &floorCount) {
  minimumX = ROOM_SIZE;
  minimumY = ROOM_SIZE;
  maximumX = -1;
  maximumY = -1;
  floorCount = 0;

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      if (room.map.tiles[y][x] != TILE_FLOOR)
        continue;

      floorCount++;
      if (x < minimumX) minimumX = x;
      if (x > maximumX) maximumX = x;
      if (y < minimumY) minimumY = y;
      if (y > maximumY) maximumY = y;
    }
  }

  return floorCount > 0;
}

bool isRoomEligibleForPillars(const DungeonRoom &room) {
  if (room.type == ROOM_ENTRANCE || room.type == ROOM_TREASURE ||
      room.shape == SHAPE_ENTRANCE ||
      room.shape == SHAPE_WINDING_CORRIDOR ||
      room.shape == SHAPE_SMALL_RECTANGLE) {
    return false;
  }

  int minimumX = 0;
  int minimumY = 0;
  int maximumX = 0;
  int maximumY = 0;
  uint16_t floorCount = 0;
  if (!getPillarFloorBounds(
          room, minimumX, minimumY, maximumX, maximumY, floorCount)) {
    return false;
  }

  const int width = maximumX - minimumX + 1;
  const int height = maximumY - minimumY + 1;
  return floorCount >= PILLAR_MIN_WALKABLE_TILES &&
         width >= PILLAR_MIN_INTERIOR_SPAN &&
         height >= PILLAR_MIN_INTERIOR_SPAN;
}

static bool canPlacePillarAt(
    const DungeonRoom &room,
    int x,
    int y) {
  for (const TrapInstance &trap : room.traps) {
    if (trap.id != TRAP_NONE && trap.x == x && trap.y == y)
      return false;
  }

  for (const SuspicionInstance &suspicion : room.suspicions) {
    if (suspicion.type != SUSPICION_NONE &&
        suspicion.x == x && suspicion.y == y) {
      return false;
    }
  }

  return x > 0 && x < ROOM_SIZE - 1 &&
         y > 0 && y < ROOM_SIZE - 1 &&
         room.map.tiles[y][x] == TILE_FLOOR &&
         !isReservedContentTile(room, x, y) &&
         !isNearRoomConnection(room, x, y);
}

uint8_t placePillarLayout(
    DungeonRoom &room,
    PillarLayoutType layout,
    int centerX,
    int centerY) {
  static constexpr PillarOffset square[] = {
      {-3, -3}, {3, -3}, {-3, 3}, {3, 3}};
  static constexpr PillarOffset horizontalRow[] = {
      {-3, 0}, {-1, 0}, {1, 0}, {3, 0}};
  static constexpr PillarOffset verticalRow[] = {
      {0, -3}, {0, -1}, {0, 1}, {0, 3}};
  static constexpr PillarOffset hexagon[] = {
      {-2, -3}, {2, -3}, {-3, 0}, {3, 0}, {-2, 3}, {2, 3}};

  const PillarOffset *offsets = nullptr;
  uint8_t pillarCount = 0;
  switch (layout) {
    case PILLAR_LAYOUT_SQUARE:
      offsets = square;
      pillarCount = sizeof(square) / sizeof(square[0]);
      break;
    case PILLAR_LAYOUT_ROW_HORIZONTAL:
      offsets = horizontalRow;
      pillarCount = sizeof(horizontalRow) / sizeof(horizontalRow[0]);
      break;
    case PILLAR_LAYOUT_ROW_VERTICAL:
      offsets = verticalRow;
      pillarCount = sizeof(verticalRow) / sizeof(verticalRow[0]);
      break;
    case PILLAR_LAYOUT_HEXAGON:
      offsets = hexagon;
      pillarCount = sizeof(hexagon) / sizeof(hexagon[0]);
      break;
    default:
      return 0;
  }

  for (uint8_t i = 0; i < pillarCount; i++) {
    if (!canPlacePillarAt(
            room, centerX + offsets[i].x, centerY + offsets[i].y)) {
      return 0;
    }
  }

  for (uint8_t i = 0; i < pillarCount; i++) {
    room.map.tiles[centerY + offsets[i].y][centerX + offsets[i].x] =
        TILE_PILLAR;
  }

  if (validateRoomConnectivity(room))
    return pillarCount;

  // Placement is transactional: an invalid arrangement never survives into
  // the generated dungeon.
  for (uint8_t i = 0; i < pillarCount; i++) {
    room.map.tiles[centerY + offsets[i].y][centerX + offsets[i].x] =
        TILE_FLOOR;
  }
  return 0;
}

uint8_t populatePillarTerrain(
    DungeonRoom &room,
    uint8_t chanceRoll,
    uint8_t layoutRoll) {
  if (!isRoomEligibleForPillars(room))
    return 0;

  const uint8_t chance = room.type == ROOM_BOSS
      ? BOSS_PILLAR_ROOM_CHANCE_PERCENT
      : PILLAR_ROOM_CHANCE_PERCENT;
  if (chanceRoll >= chance)
    return 0;

  int minimumX = 0;
  int minimumY = 0;
  int maximumX = 0;
  int maximumY = 0;
  uint16_t floorCount = 0;
  if (!getPillarFloorBounds(
          room, minimumX, minimumY, maximumX, maximumY, floorCount)) {
    return 0;
  }

  const int centerX = (minimumX + maximumX) / 2;
  const int centerY = (minimumY + maximumY) / 2;
  static constexpr PillarOffset translations[] = {
      {0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1},
      {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
      {-2, 0}, {2, 0}, {0, -2}, {0, 2}};

  // Start with the rolled pattern, then try rotations/alternatives and small
  // room-aware translations before giving up safely.
  for (uint8_t patternAttempt = 0;
       patternAttempt < PILLAR_LAYOUT_COUNT;
       patternAttempt++) {
    const PillarLayoutType layout = static_cast<PillarLayoutType>(
        (layoutRoll + patternAttempt) % PILLAR_LAYOUT_COUNT);

    for (const PillarOffset &translation : translations) {
      const uint8_t placed = placePillarLayout(
          room,
          layout,
          centerX + translation.x,
          centerY + translation.y);
      if (placed > 0)
        return placed;
    }
  }

  return 0;
}

bool isRoomEligibleForFurniture(const DungeonRoom& room)
{
  if (room.type == ROOM_ENTRANCE || room.type == ROOM_TREASURE ||
      room.shape == SHAPE_ENTRANCE || room.shape == SHAPE_WINDING_CORRIDOR ||
      room.shape == SHAPE_SMALL_RECTANGLE)
  {
    return false;
  }

  return countInteriorWalkableTiles(room) >= 42;
}

static bool placeFurnitureAt(
    DungeonRoom& room,
    DungeonFurnitureType type,
    int x,
    int y)
{
  if (x <= 0 || x >= ROOM_SIZE - 1 || y <= 0 || y >= ROOM_SIZE - 1 ||
      room.map.tiles[y][x] != TILE_FLOOR || isReservedContentTile(room, x, y) ||
      isNearRoomConnection(room, x, y))
  {
    return false;
  }

  if (!addDungeonFurniture(room, type, x, y))
    return false;

  if (validateRoomConnectivity(room))
    return true;

  DungeonFurnitureInstance* furniture = getDungeonFurnitureAt(room, x, y);
  if (furniture != nullptr)
    *furniture = DungeonFurnitureInstance{};
  room.map.tiles[y][x] = TILE_FLOOR;
  return false;
}

static uint8_t placeFurnitureCluster(
    DungeonRoom& room,
    DungeonFurnitureType type,
    uint8_t desiredCount)
{
  static constexpr PillarOffset clusterOffsets[] = {
      {0, 0}, {1, 0}, {0, 1}, {1, 1}, {-1, 0}, {0, -1}};

  for (uint8_t attempt = 0; attempt < 24; attempt++)
  {
    const int originX = randomInclusive(2, ROOM_SIZE - 3);
    const int originY = randomInclusive(2, ROOM_SIZE - 3);
    uint8_t placed = 0;

    for (const PillarOffset& offset : clusterOffsets)
    {
      if (placed >= desiredCount)
        break;
      if (placeFurnitureAt(
              room, type, originX + offset.x, originY + offset.y))
      {
        placed++;
      }
    }

    if (placed > 0)
      return placed;
  }

  return 0;
}

uint8_t populateDungeonFurniture(
    DungeonRoom& room,
    uint8_t barrelRoll,
    uint8_t crateRoll,
    uint8_t statueRoll,
    uint8_t brazierRoll)
{
  if (!isRoomEligibleForFurniture(room))
    return 0;

  uint8_t placed = 0;
  if (barrelRoll < BARREL_CLUSTER_CHANCE_PERCENT)
    placed += placeFurnitureCluster(room, FURNITURE_BARREL, randomInclusive(2, 3));
  if (crateRoll < CRATE_CLUSTER_CHANCE_PERCENT)
    placed += placeFurnitureCluster(room, FURNITURE_CRATE, randomInclusive(2, 3));
  if (statueRoll < STATUE_CHANCE_PERCENT)
    placed += placeFurnitureCluster(room, FURNITURE_STATUE, 1);
  if (brazierRoll < BRAZIER_CHANCE_PERCENT)
    placed += placeFurnitureCluster(room, FURNITURE_BRAZIER, randomInclusive(1, 2));
  return placed;
}

static bool canPlaceRubbleAt(
    const DungeonRoom &room,
    int x,
    int y) {
  return x > 0 && x < ROOM_SIZE - 1 &&
         y > 0 && y < ROOM_SIZE - 1 &&
         room.map.tiles[y][x] == TILE_FLOOR &&
         !isReservedContentTile(room, x, y) &&
         !isNearRoomConnection(room, x, y);
}

uint8_t placeRubblePatch(
    DungeonRoom &room,
    uint8_t originX,
    uint8_t originY,
    uint8_t maximumTiles) {
  if (maximumTiles == 0 ||
      !canPlaceRubbleAt(room, originX, originY)) {
    return 0;
  }

  // A compact, deterministic growth pattern makes patches readable and keeps
  // the helper testable. Random generation supplies different origins/sizes.
  static constexpr int8_t offsetX[] = {0, 1, 0, -1, 1, -1, 0, 2, -2};
  static constexpr int8_t offsetY[] = {0, 0, 1, 0, 1, 1, -1, 0, 0};
  const uint8_t candidateCount = sizeof(offsetX) / sizeof(offsetX[0]);
  uint8_t placed = 0;

  for (uint8_t i = 0; i < candidateCount && placed < maximumTiles; i++) {
    const int x = originX + offsetX[i];
    const int y = originY + offsetY[i];
    if (!canPlaceRubbleAt(room, x, y))
      continue;

    room.map.tiles[y][x] = TILE_RUBBLE;
    placed++;
  }

  return placed;
}

uint8_t populateRubbleTerrain(DungeonRoom &room) {
  // Dungeon-level planning decides which ordinary rooms receive terrain.
  // Entrance, final treasure, and boss use dedicated generation rules.
  if (room.type == ROOM_ENTRANCE || room.type == ROOM_TREASURE ||
      room.type == ROOM_BOSS) {
    return 0;
  }

  const uint8_t patchSize = randomInclusive(2, 5);
  for (uint8_t attempt = 0; attempt < 24; attempt++) {
    const uint8_t x = randomInclusive(2, ROOM_SIZE - 3);
    const uint8_t y = randomInclusive(2, ROOM_SIZE - 3);
    const uint8_t placed = placeRubblePatch(room, x, y, patchSize);
    if (placed > 0)
      return placed;
  }

  // Narrow corridors can make random origins unlucky. Scan deterministically
  // so every selected middle room receives at least one valid patch.
  for (uint8_t y = 1; y < ROOM_SIZE - 1; y++) {
    for (uint8_t x = 1; x < ROOM_SIZE - 1; x++) {
      const uint8_t placed = placeRubblePatch(room, x, y, patchSize);
      if (placed > 0)
        return placed;
    }
  }

  return 0;
}

uint8_t populateBossRubbleTerrain(DungeonRoom &room) {
  if (room.type != ROOM_BOSS)
    return 0;

  // Two separated patches make the boss room feel deliberately broken and
  // create tactical rough-ground choices without covering door approaches or
  // authored creature markers.
  static constexpr uint8_t preferredX[] = {4, 10};
  static constexpr uint8_t preferredY[] = {6, 9};
  static constexpr uint8_t patchSize[] = {5, 4};
  uint8_t totalPlaced = 0;

  for (uint8_t patch = 0; patch < 2; patch++) {
    uint8_t placed = placeRubblePatch(
        room, preferredX[patch], preferredY[patch], patchSize[patch]);

    if (placed == 0) {
      // Shape variants may wall off a preferred origin. Find the closest safe
      // interior floor rather than skipping the mandatory boss terrain.
      for (uint8_t radius = 1; radius < ROOM_SIZE && placed == 0; radius++) {
        for (uint8_t y = 1; y < ROOM_SIZE - 1 && placed == 0; y++) {
          for (uint8_t x = 1; x < ROOM_SIZE - 1; x++) {
            const int dx = x > preferredX[patch]
                ? x - preferredX[patch] : preferredX[patch] - x;
            const int dy = y > preferredY[patch]
                ? y - preferredY[patch] : preferredY[patch] - y;
            if (dx + dy != radius)
              continue;

            placed = placeRubblePatch(room, x, y, patchSize[patch]);
            if (placed > 0)
              break;
          }
        }
      }
    }

    totalPlaced += placed;
  }

  return totalPlaced;
}

static bool isFloorAreaAvailable(
    const DungeonRoom &room,
    int x,
    int y,
    uint8_t width,
    uint8_t height) {
  if (x < 1 || y < 1 || width == 0 || height == 0 ||
      x + width > ROOM_SIZE - 1 ||
      y + height > ROOM_SIZE - 1) {
    return false;
  }

  for (int tileY = y; tileY < y + height; tileY++) {
    for (int tileX = x; tileX < x + width; tileX++) {
      if (room.map.tiles[tileY][tileX] != TILE_FLOOR ||
          isReservedContentTile(room, tileX, tileY)) {
        return false;
      }
    }
  }

  return true;
}

static bool placeContentMarkerNear(
    DungeonRoom &room,
    TileType marker,
    int preferredX,
    int preferredY,
    uint8_t width,
    uint8_t height) {
  int bestX = -1;
  int bestY = -1;
  int bestDistance = 32767;

  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      if (!isFloorAreaAvailable(room, x, y, width, height))
        continue;

      const int dx = x > preferredX
          ? x - preferredX
          : preferredX - x;
      const int dy = y > preferredY
          ? y - preferredY
          : preferredY - y;
      const int distance = dx + dy;

      if (distance < bestDistance) {
        bestDistance = distance;
        bestX = x;
        bestY = y;
      }
    }
  }

  if (bestX < 0)
    return false;

  room.map.tiles[bestY][bestX] = marker;
  return true;
}

static bool generateEntrance(DungeonRoom &room) {
  fillRoom(room, TILE_WALL);

  if (!carveRectangle(
          room,
          ENTRANCE_HALL_X,
          ENTRANCE_HALL_Y,
          ENTRANCE_HALL_WIDTH,
          ENTRANCE_HALL_HEIGHT) ||
      !carveRectangle(
          room,
          ENTRANCE_ALCOVE_X,
          ENTRANCE_FOUNTAIN_ALCOVE_Y,
          ENTRANCE_ALCOVE_WIDTH,
          ENTRANCE_ALCOVE_HEIGHT) ||
      !carveRectangle(
          room,
          ENTRANCE_ALCOVE_X,
          ENTRANCE_SERVICE_ALCOVE_Y,
          ENTRANCE_ALCOVE_WIDTH,
          ENTRANCE_ALCOVE_HEIGHT)) {
    return false;
  }

  uint8_t count = room.connectionCount;
  if (count > MAX_ROOM_CONNECTIONS)
    count = MAX_ROOM_CONNECTIONS;

  for (uint8_t i = 0; i < count; i++) {
    const RoomConnection &connection = room.connections[i];
    if (!connectRoomConnectionToFloor(
            room,
            connection,
            ROOM_SIZE - 2,
            ENTRANCE_EAST_CONNECTION_Y)) {
      return false;
    }
  }

  addDoors(room);
  return validateRoomConnectivity(room);
}

bool placeGiantSpiderEncounter(DungeonRoom &room) {
  // Giant Spider entities occupy a real 2x2 tile footprint. Reuse the normal
  // content placement path so all four tiles are floor and entry tiles,
  // doors, and existing content markers remain clear.
  return placeContentMarkerNear(
      room,
      TILE_GIANT_SPIDER_START,
      ROOM_SIZE / 2 - 1,
      ROOM_SIZE / 2 - 1,
      2,
      2);
}

static void generateCombat(DungeonRoom &room) {
  const int center = ROOM_SIZE / 2;
  // A direct encounter occupies central connected floor rather than corners.
  placeContentMarkerNear(room, TILE_ENEMY_START, center - 2, center);
  placeContentMarkerNear(room, TILE_ENEMY_START, center + 2, center);
}


static void generatePuzzle(DungeonRoom &room) {
  // Later:
  // Hidden doors
  // Switches
  // Pressure plates
}

static void generateAmbush(DungeonRoom &room) {
  // Favor the far side/periphery, leaving the central entry area clear.
  placeContentMarkerNear(
      room, TILE_ENEMY_START, ROOM_SIZE - 3, 3);
  placeContentMarkerNear(
      room, TILE_ENEMY_START, ROOM_SIZE - 3, ROOM_SIZE - 3);
}

static void generateBoss(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // The final encounter deliberately replaces normal random population:
  // Skeleton Mage with two existing longsword Skeleton guards.
  placeContentMarkerNear(room, TILE_SKELETON_START, 2, 2);
  placeContentMarkerNear(room, TILE_SKELETON_MAGE_START, center, 2);
  placeContentMarkerNear(room, TILE_SKELETON_START, 12, 2);
}

static void generateTreasure(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // Treasure chest.
  placeContentMarkerNear(room, TILE_CHEST_SPAWN, center, 3);

  // Loose loot.
  placeContentMarkerNear(room, TILE_LOOT_SPAWN, center - 2, 3);
  placeContentMarkerNear(room, TILE_LOOT_SPAWN, center + 2, 3);
}



void generateRoom(DungeonRoom &room) {
  for (DungeonFurnitureInstance& furniture : room.furniture)
    furniture = DungeonFurnitureInstance{};

  if (room.type == ROOM_ENTRANCE) {
    room.shape = SHAPE_ENTRANCE;
    generateEntrance(room);
    return;
  }

  if (!buildRoomGeometry(room, room.shape)) {
    // Keep the already-generated connection coordinates, but replace any
    // unusable geometry with the always-traversable full room.
    room.shape = SHAPE_SQUARE;
    if (!buildRoomGeometry(room, room.shape)) {
      return;
    }
  }

  switch (room.type) {
    case ROOM_COMBAT:
      generateCombat(room);
      break;

    case ROOM_PUZZLE:
      generatePuzzle(room);
      break;

    case ROOM_AMBUSH:
      generateAmbush(room);
      break;

    case ROOM_BOSS:
      generateBoss(room);
      break;

    case ROOM_TREASURE:
      generateTreasure(room);
      break;

    case ROOM_EMPTY:
      break;
  }

}
