//
// Created by james on 7/12/2026.
//

#include "roomdraw.h"
#include <Adafruit_ST7789.h>
#include "config.h"
#include "graphics/tiles.h"
#include "traps.h"

extern Adafruit_ST7789 tft;

constexpr uint16_t COLOR_WALL = 0x7BEF;   // Medium Gray
constexpr uint16_t COLOR_FLOOR = 0xC618;  // Light Gray
constexpr uint16_t COLOR_VOID = ST77XX_BLACK;

namespace
{
// Muted colors keep environmental clues readable at 16x16 without turning
// them into trap icons. The confirmed marker below is intentionally brighter.
constexpr uint16_t COLOR_CLUE_DARK = 0x39E7;
constexpr uint16_t COLOR_CLUE_SHADOW = 0x4228;
constexpr uint16_t COLOR_CLUE_LIGHT = 0x8C71;
constexpr uint16_t COLOR_CLUE_BLOOD = 0x6002;
constexpr uint16_t COLOR_CLUE_BONE = 0xBDF7;
constexpr uint16_t COLOR_CLUE_DUST = 0x5269;

constexpr uint16_t COLOR_TRAP_ACTIVE = 0xF800;
constexpr uint16_t COLOR_TRAP_DISABLED = 0x07E0;
constexpr uint16_t COLOR_TRAP_TRIGGERED = 0xFDC0;
constexpr uint16_t COLOR_TRAP_DESTROYED = 0x7BEF;

void drawSuspicionClue(
    int tileX,
    int tileY,
    SuspicionType suspicion)
{
  const int x = tileX * TILE_SIZE;
  const int y = tileY * TILE_SIZE;

  switch (suspicion)
  {
    case SUSPICION_RAISED_TILE:
      // A short highlight and offset shadow suggest one lifted slab edge.
      tft.drawLine(x + 5, y + 4, x + 11, y + 4, COLOR_CLUE_LIGHT);
      tft.drawLine(x + 4, y + 5, x + 4, y + 10, COLOR_CLUE_LIGHT);
      tft.drawLine(x + 5, y + 11, x + 11, y + 11, COLOR_CLUE_SHADOW);
      tft.drawLine(x + 12, y + 5, x + 12, y + 10, COLOR_CLUE_SHADOW);
      break;

    case SUSPICION_CRACKED_FLOOR:
      tft.drawLine(x + 4, y + 3, x + 7, y + 7, COLOR_CLUE_DARK);
      tft.drawLine(x + 7, y + 7, x + 6, y + 11, COLOR_CLUE_DARK);
      tft.drawLine(x + 7, y + 7, x + 11, y + 9, COLOR_CLUE_DARK);
      tft.drawPixel(x + 12, y + 10, COLOR_CLUE_DARK);
      break;

    case SUSPICION_FLOOR_GROOVES:
      tft.drawLine(x + 3, y + 5, x + 10, y + 5, COLOR_CLUE_SHADOW);
      tft.drawLine(x + 5, y + 10, x + 12, y + 10, COLOR_CLUE_SHADOW);
      tft.drawPixel(x + 11, y + 6, COLOR_CLUE_DARK);
      tft.drawPixel(x + 4, y + 9, COLOR_CLUE_DARK);
      break;

    case SUSPICION_BLOODSTAIN:
      tft.drawLine(x + 7, y + 7, x + 9, y + 8, COLOR_CLUE_BLOOD);
      tft.drawPixel(x + 6, y + 8, COLOR_CLUE_BLOOD);
      tft.drawPixel(x + 8, y + 9, COLOR_CLUE_BLOOD);
      tft.drawPixel(x + 11, y + 6, COLOR_CLUE_BLOOD);
      break;

    case SUSPICION_BONES:
      tft.drawLine(x + 5, y + 6, x + 10, y + 10, COLOR_CLUE_BONE);
      tft.drawLine(x + 10, y + 5, x + 6, y + 10, COLOR_CLUE_BONE);
      tft.drawPixel(x + 4, y + 5, COLOR_CLUE_BONE);
      tft.drawPixel(x + 11, y + 11, COLOR_CLUE_BONE);
      break;

    case SUSPICION_DISTURBED_DUST:
      tft.drawPixel(x + 4, y + 5, COLOR_CLUE_DUST);
      tft.drawPixel(x + 7, y + 4, COLOR_CLUE_DUST);
      tft.drawPixel(x + 10, y + 6, COLOR_CLUE_DUST);
      tft.drawPixel(x + 5, y + 10, COLOR_CLUE_DUST);
      tft.drawLine(x + 8, y + 11, x + 11, y + 10, COLOR_CLUE_DUST);
      break;

    default:
      break;
  }
}

void drawCornerMarker(int x, int y, uint16_t color)
{
  constexpr int edge = 4;
  constexpr int inset = 1;
  const int left = x + inset;
  const int top = y + inset;
  const int right = x + TILE_SIZE - 1 - inset;
  const int bottom = y + TILE_SIZE - 1 - inset;

  tft.drawLine(left, top, left + edge, top, color);
  tft.drawLine(left, top, left, top + edge, color);
  tft.drawLine(right - edge, top, right, top, color);
  tft.drawLine(right, top, right, top + edge, color);
  tft.drawLine(left, bottom, left + edge, bottom, color);
  tft.drawLine(left, bottom - edge, left, bottom, color);
  tft.drawLine(right - edge, bottom, right, bottom, color);
  tft.drawLine(right, bottom - edge, right, bottom, color);
}
}

void drawRoom(const DungeonRoom &room) {
  for (int y = 0; y < ROOM_SIZE; y++) {
    for (int x = 0; x < ROOM_SIZE; x++) {
      drawRoomTile(room, x, y);
    }
  }
}

void drawTile(int tileX, int tileY, TileType tile) {
  uint16_t color = COLOR_VOID;

  switch (tile) {
    case TILE_WALL:
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonWallTiles[(tileX * 7 + tileY * 11) % 3],
                        TILE_SIZE, TILE_SIZE);
      return;

    case TILE_FLOOR:
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                        TILE_SIZE, TILE_SIZE);
      return;

    case TILE_DOOR:
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonDoor16x16, TILE_SIZE, TILE_SIZE);
      return;

    case TILE_TRAP:
      // A trap marker is a hidden feature, not visible terrain.
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                        TILE_SIZE, TILE_SIZE);
      return;

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

void drawRoomTile(const DungeonRoom& room, int tileX, int tileY)
{
  if (tileX < 0 || tileX >= ROOM_SIZE ||
      tileY < 0 || tileY >= ROOM_SIZE)
  {
    return;
  }

  drawTile(tileX, tileY, room.map.tiles[tileY][tileX]);
  drawSuspicionClue(tileX, tileY, getSuspicionAt(room, tileX, tileY));
}

void drawTrapDiscoveryMarker(
    const DungeonRoom& room,
    int tileX,
    int tileY)
{
  if (tileX < 0 || tileX >= ROOM_SIZE ||
      tileY < 0 || tileY >= ROOM_SIZE)
  {
    return;
  }

  const TrapInstance* trap = getTrapAt(room, tileX, tileY);

  if (trap == nullptr || !trap->discovered)
    return;

  uint16_t color = COLOR_TRAP_ACTIVE;

  if (trap->destroyed)
    color = COLOR_TRAP_DESTROYED;
  else if (trap->disabled)
    color = COLOR_TRAP_DISABLED;
  else if (trap->triggered)
    color = COLOR_TRAP_TRIGGERED;

  drawCornerMarker(
      tileX * TILE_SIZE,
      tileY * TILE_SIZE,
      color);
}
