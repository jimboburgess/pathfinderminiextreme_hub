//
// Created by james on 7/12/2026.
//

#include "roomdraw.h"
#include <Adafruit_ST7789.h>
#include "config.h"
#include "graphics/display.h"
#include "graphics/tiles.h"
#include "traps.h"
#include "fountain.h"

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

constexpr uint16_t COLOR_TRAP_METAL_DARK = 0x4208;
constexpr uint16_t COLOR_TRAP_METAL = 0x8C71;
constexpr uint16_t COLOR_TRAP_METAL_LIGHT = 0xD69A;
constexpr uint16_t COLOR_TRAP_BRASS = 0xC5A0;
constexpr uint16_t COLOR_TRAP_RUST = 0x7800;
constexpr uint16_t COLOR_TRAP_DEBRIS = 0x5AEB;
constexpr uint16_t COLOR_FOUNTAIN_STONE_DARK = 0x4A49;
constexpr uint16_t COLOR_FOUNTAIN_STONE = 0x8C71;
constexpr uint16_t COLOR_FOUNTAIN_STONE_LIGHT = 0xC618;
constexpr uint16_t COLOR_FOUNTAIN_WATER = 0x041F;
constexpr uint16_t COLOR_FOUNTAIN_WATER_LIGHT = 0x5DDF;
constexpr uint16_t COLOR_FOUNTAIN_SPENT = 0x2945;

// Draw one rectangle from the shared 32x48 fountain canvas, clipped to the
// tile currently being repainted. Every tile therefore sees the same object
// coordinates, which keeps seams from becoming six disconnected drawings.
void drawFountainCanvasRect(
    int tileX,
    int tileY,
    int localX,
    int localY,
    int width,
    int height,
    int rectX,
    int rectY,
    int rectWidth,
    int rectHeight,
    uint16_t color)
{
  const int left = rectX > localX ? rectX : localX;
  const int top = rectY > localY ? rectY : localY;
  const int right = (rectX + rectWidth) < (localX + width)
      ? (rectX + rectWidth) : (localX + width);
  const int bottom = (rectY + rectHeight) < (localY + height)
      ? (rectY + rectHeight) : (localY + height);

  if (left >= right || top >= bottom)
    return;

  tft.fillRect(tileX * TILE_SIZE + left - localX,
               tileY * TILE_SIZE + top - localY,
               right - left, bottom - top, color);
}

void drawHealingFountainTile(const DungeonRoom& room, int tileX, int tileY)
{
  const HealingFountain* fountain = getHealingFountainAt(room, tileX, tileY);
  if (fountain == nullptr)
    return;

  const int localX = (tileX - fountain->x) * TILE_SIZE;
  const int localY = (tileY - fountain->y) * TILE_SIZE;
  const uint16_t water = fountain->used ? COLOR_FOUNTAIN_SPENT : COLOR_FOUNTAIN_WATER;
  const uint16_t highlight = fountain->used ? COLOR_FOUNTAIN_STONE_DARK : COLOR_FOUNTAIN_WATER_LIGHT;

  tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                    dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                    TILE_SIZE, TILE_SIZE);

  const auto rect = [&](int x, int y, int w, int h, uint16_t color) {
    drawFountainCanvasRect(tileX, tileY, localX, localY, TILE_SIZE,
                           TILE_SIZE, x, y, w, h, color);
  };

  // One continuous silhouette: arched shrine, central falling water, a wide
  // basin, then a heavy stone base and pool spanning both lower tiles.
  rect(5, 2, 22, 20, COLOR_FOUNTAIN_STONE_DARK);
  rect(7, 3, 18, 17, COLOR_FOUNTAIN_STONE);
  rect(9, 5, 14, 12, COLOR_FOUNTAIN_STONE_LIGHT);
  rect(11, 7, 10, 11, COLOR_FOUNTAIN_STONE_DARK);
  rect(13, 8, 6, 15, water);
  rect(14, 9, 2, 12, highlight);
  rect(17, 11, 1, 8, COLOR_FOUNTAIN_STONE_LIGHT);

  rect(2, 21, 28, 15, COLOR_FOUNTAIN_STONE_DARK);
  rect(4, 23, 24, 11, COLOR_FOUNTAIN_STONE);
  rect(6, 25, 20, 7, water);
  rect(7, 26, 17, 2, highlight);
  rect(5, 33, 22, 2, COLOR_FOUNTAIN_STONE_LIGHT);

  rect(0, 36, 32, 10, COLOR_FOUNTAIN_STONE_DARK);
  rect(2, 38, 28, 7, COLOR_FOUNTAIN_STONE);
  rect(4, 39, 24, 5, water);
  rect(6, 40, 18, 1, highlight);
  rect(3, 46, 26, 2, COLOR_FOUNTAIN_STONE_DARK);
}

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

void drawTrapStateArt(int tileX, int tileY, TrapID trapID, TrapVisualState state)
{
  const int x = tileX * TILE_SIZE;
  const int y = tileY * TILE_SIZE;

  switch (state)
  {
    case TRAP_VISUAL_ARMED:
      // Intact pressure plate and exposed seam: ready to fire.
      tft.drawRect(x + 3, y + 5, 10, 7, COLOR_TRAP_METAL_DARK);
      tft.drawRect(x + 4, y + 6, 8, 5, COLOR_TRAP_METAL);
      tft.drawLine(x + 5, y + 7, x + 10, y + 7, COLOR_TRAP_METAL_LIGHT);
      tft.drawLine(x + 4, y + 11, x + 11, y + 11, COLOR_TRAP_RUST);
      tft.drawPixel(x + 7, y + 9, COLOR_TRAP_BRASS);
      break;

    case TRAP_VISUAL_TRIGGERED:
      if (trapID != TRAP_SPIKE_PLATE)
      {
        // A spent wall launcher is represented compactly at its linked plate.
        tft.drawRect(x + 4, y + 7, 8, 3, COLOR_TRAP_METAL_DARK);
        tft.drawLine(x + 5, y + 8, x + 10, y + 8, COLOR_TRAP_RUST);
        break;
      }
      // The supplied spike artwork is an overlay: the regular floor beneath
      // it stays intact and the persistent triggered flag selects it again
      // whenever this tile is redrawn or the room is revisited.
      drawSpriteTransparent(x, y, dungeonSpikes16x16);
      break;

    case TRAP_VISUAL_DISABLED:
      // Ordered loose spikes, plate pieces, and a tiny exposed gear read as
      // a deliberately dismantled mechanism rather than a sprung one.
      tft.drawLine(x + 3, y + 10, x + 8, y + 6, COLOR_TRAP_METAL_LIGHT);
      tft.drawLine(x + 7, y + 12, x + 12, y + 8, COLOR_TRAP_METAL);
      tft.drawRect(x + 4, y + 11, 4, 2, COLOR_TRAP_METAL_DARK);
      tft.drawPixel(x + 11, y + 11, COLOR_TRAP_BRASS);
      tft.drawPixel(x + 10, y + 12, COLOR_TRAP_BRASS);
      tft.drawPixel(x + 12, y + 12, COLOR_TRAP_BRASS);
      tft.drawPixel(x + 11, y + 13, COLOR_TRAP_BRASS);
      tft.drawPixel(x + 11, y + 12, COLOR_TRAP_METAL_DARK);
      break;

    case TRAP_VISUAL_DESTROYED:
      // Chaotic, smashed fragments distinguish destruction from careful work.
      tft.drawLine(x + 3, y + 6, x + 7, y + 12, COLOR_TRAP_DEBRIS);
      tft.drawLine(x + 6, y + 7, x + 12, y + 10, COLOR_TRAP_METAL_DARK);
      tft.drawLine(x + 4, y + 12, x + 10, y + 13, COLOR_TRAP_DEBRIS);
      tft.drawPixel(x + 11, y + 5, COLOR_TRAP_RUST);
      tft.drawPixel(x + 12, y + 12, COLOR_TRAP_RUST);
      tft.drawPixel(x + 5, y + 8, COLOR_TRAP_METAL_LIGHT);
      break;

    case TRAP_VISUAL_HIDDEN:
    default:
      break;
  }
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

    case TILE_RUBBLE:
      // The supplied rubble art is transparent, so retain the regular floor
      // beneath its broken-stone overlay rather than drawing magenta pixels.
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                        TILE_SIZE, TILE_SIZE);
      drawSpriteTransparent(
          tileX * TILE_SIZE, tileY * TILE_SIZE, dungeonRubble16x16);
      return;

    case TILE_PILLAR:
      // Pillars are transparent blocking dressing over the normal floor.
      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                        TILE_SIZE, TILE_SIZE);
      drawSpriteTransparent(
          tileX * TILE_SIZE, tileY * TILE_SIZE, dungeonPillar16x16);
      return;

    case TILE_STATUE:
    case TILE_BRAZIER:
    case TILE_CRATE:
    case TILE_BARREL:
    {
      const uint16_t* furnishing = dungeonStatue16x16;
      if (tile == TILE_BRAZIER)
        furnishing = dungeonBrazier16x16;
      else if (tile == TILE_CRATE)
        furnishing = dungeonCrate16x16;
      else if (tile == TILE_BARREL)
        furnishing = dungeonBarrel16x16;

      tft.drawRGBBitmap(tileX * TILE_SIZE, tileY * TILE_SIZE,
                        dungeonFloorTiles[(tileX * 13 + tileY * 5) % 3],
                        TILE_SIZE, TILE_SIZE);
      drawSpriteTransparent(
          tileX * TILE_SIZE, tileY * TILE_SIZE, furnishing);
      return;
    }

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

    case TILE_FOUNTAIN:
      // The room-aware painter supplies the proper multi-tile artwork.
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

  if (isHealingFountainTile(room, tileX, tileY))
    drawHealingFountainTile(room, tileX, tileY);
  else
    drawTile(tileX, tileY, room.map.tiles[tileY][tileX]);
  drawSuspicionClue(tileX, tileY, getSuspicionAt(room, tileX, tileY));

  const TrapInstance* trap = getTrapAt(room, tileX, tileY);
  if (trap != nullptr)
    drawTrapStateArt(tileX, tileY, trap->id, getTrapVisualState(*trap));
}
