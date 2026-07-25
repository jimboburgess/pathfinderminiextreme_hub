//
// Created by james on 7/12/2026.
//

#include "roomgen.h"
#include <Arduino.h>


static void createSquareRoom(DungeonRoom &room);
static void createCrossRoom(DungeonRoom &room);
static void createCircleRoom(DungeonRoom &room);
static void addDoors(DungeonRoom &room);
static void generateEntrance(DungeonRoom &room);
static void generateCombat(DungeonRoom &room);
static void generatePuzzle(DungeonRoom &room);
static void generateTrap(DungeonRoom &room);
static void generateAmbush(DungeonRoom &room);
static void generateBoss(DungeonRoom &room);
static void generateTreasure(DungeonRoom &room);




static void createSquareRoom(DungeonRoom &room) {
  // Fill the room with solid walls
  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      room.map.tiles[y][x] = TILE_WALL;
    }
  }

  // Carve out the interior floor
  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    for (int x = 1; x < ROOM_SIZE - 1; x++) {
      room.map.tiles[y][x] = TILE_FLOOR;
    }
  }

  // Calculate the center of the room
  int center = ROOM_SIZE / 2;

  addDoors(room);
}

static void createCrossRoom(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // Fill with walls
  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      room.map.tiles[y][x] = TILE_WALL;
    }
  }

  // Vertical corridor
  for (int y = 1; y < ROOM_SIZE - 1; y++) {
    room.map.tiles[y][center - 1] = TILE_FLOOR;
    room.map.tiles[y][center] = TILE_FLOOR;
    room.map.tiles[y][center + 1] = TILE_FLOOR;
  }

  // Horizontal corridor
  for (int x = 1; x < ROOM_SIZE - 1; x++) {
    room.map.tiles[center - 1][x] = TILE_FLOOR;
    room.map.tiles[center][x] = TILE_FLOOR;
    room.map.tiles[center + 1][x] = TILE_FLOOR;
  }

  addDoors(room);
}

static void createCircleRoom(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      int dx = x - center;
      int dy = y - center;

      if ((dx * dx + dy * dy) <= 42)
        room.map.tiles[y][x] = TILE_FLOOR;
      else
        room.map.tiles[y][x] = TILE_WALL;
    }
  }

  addDoors(room);
}

static void addDoors(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // North
  if (room.north != NO_ROOM) {
    room.map.tiles[0][center] = TILE_DOOR;
  }

  // South
  if (room.south != NO_ROOM) {
    room.map.tiles[ROOM_SIZE - 1][center - 1] = TILE_DOOR;
  }

  // West
  if (room.west != NO_ROOM) {
    room.map.tiles[center][0] = TILE_DOOR;
  }

  // East
  if (room.east != NO_ROOM) {
    room.map.tiles[center - 1][ROOM_SIZE - 1] = TILE_DOOR;
  }
}

static void generateEntrance(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;
}

static void generateCombat(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;


  // Corner enemies
  room.map.tiles[2][2] = TILE_ENEMY_START;
  room.map.tiles[2][12] = TILE_ENEMY_START;
  room.map.tiles[12][2] = TILE_ENEMY_START;
  room.map.tiles[12][12] = TILE_ENEMY_START;

  // Cardinal enemies
  room.map.tiles[2][center] = TILE_ENEMY_START;
  room.map.tiles[center][2] = TILE_ENEMY_START;
  room.map.tiles[center][12] = TILE_ENEMY_START;
  room.map.tiles[12][center] = TILE_ENEMY_START;
}


static void generatePuzzle(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // Later:
  // Hidden doors
  // Switches
  // Pressure plates
}

static void generateTrap(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // Later:
  // Entering this room may trigger a trap.
}

static void generateAmbush(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;


  // Enemy formation

  // Front row
  room.map.tiles[6][2] = TILE_ENEMY_START;
  room.map.tiles[6][center] = TILE_ENEMY_START;
  room.map.tiles[6][12] = TILE_ENEMY_START;

  // Middle row
  room.map.tiles[9][2] = TILE_ENEMY_START;
  room.map.tiles[9][center] = TILE_ENEMY_START;
  room.map.tiles[9][12] = TILE_ENEMY_START;

  // Rear row
  room.map.tiles[12][2] = TILE_ENEMY_START;
  room.map.tiles[12][center] = TILE_ENEMY_START;
  room.map.tiles[12][12] = TILE_ENEMY_START;
}

static void generateBoss(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;


  // Boss positions
  room.map.tiles[2][2] = TILE_ENEMY_START;
  room.map.tiles[2][center] = TILE_ENEMY_START;
  room.map.tiles[2][12] = TILE_ENEMY_START;
}

static void generateTreasure(DungeonRoom &room) {
  int center = ROOM_SIZE / 2;

  // Treasure chest.
  room.map.tiles[3][center] = TILE_CHEST_SPAWN;

  // Loose loot.
  room.map.tiles[3][center - 2] = TILE_LOOT_SPAWN;
  room.map.tiles[3][center + 2] = TILE_LOOT_SPAWN;
}



void generateRoom(DungeonRoom &room) {

  switch (room.shape) {
    case SHAPE_SQUARE:
      createSquareRoom(room);
      break;

    case SHAPE_CROSS:
      createCrossRoom(room);
      break;

    case SHAPE_CIRCLE:
      createCircleRoom(room);
      break;
  }

  switch (room.type) {
    case ROOM_ENTRANCE:
      generateEntrance(room);
      break;

    case ROOM_COMBAT:
      generateCombat(room);
      break;

    case ROOM_PUZZLE:
      generatePuzzle(room);
      break;

    case ROOM_TRAP:
      generateTrap(room);
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
  }
}