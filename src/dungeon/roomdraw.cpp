//
// Created by james on 7/12/2026.
//

#include "roomdraw.h"
#include <Adafruit_ST7789.h>
#include "config.h"

extern Adafruit_ST7789 tft;

constexpr uint16_t COLOR_WALL = 0x7BEF;   // Medium Gray
constexpr uint16_t COLOR_FLOOR = 0xC618;  // Light Gray
constexpr uint16_t COLOR_DOOR = 0xA145;   // Brown
constexpr uint16_t COLOR_TRAP = ST77XX_RED;
constexpr uint16_t COLOR_VOID = ST77XX_BLACK;

void drawRoom(const DungeonRoom &room) {
  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      drawTile(x, y, room.map.tiles[y][x]);
    }
  }
}
void drawTile(int tileX, int tileY, TileType tile) {
  uint16_t color = COLOR_VOID;

  switch (tile) {
    case TILE_WALL:
      color = COLOR_WALL;
      break;

    case TILE_FLOOR:
      color = COLOR_FLOOR;
      break;

    case TILE_DOOR:
      color = COLOR_DOOR;
      break;

    case TILE_TRAP:
      color = COLOR_TRAP;
      break;

    default:
      color = COLOR_VOID;
      break;
  }

  tft.fillRect(
    tileX * TILE_SIZE,
    tileY * TILE_SIZE,
    TILE_SIZE,
    TILE_SIZE,
    color);
}
