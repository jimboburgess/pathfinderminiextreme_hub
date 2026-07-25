#ifndef PATHFINDERMINIEXTREME_025_DISPLAY_H
#define PATHFINDERMINIEXTREME_025_DISPLAY_H

#include <stdint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "data/entities.h"

extern Adafruit_ST7789 tft;

void drawStartScreen();
void drawStartAnimation();
void drawTownScreen();

void drawForestScreen();
void drawDungeonScreen();
void drawEntity(const Entity& entity);
void drawForestTile(int x, int y);
void redrawForestCursor();
void redrawForestTile(int x, int y);


void drawCriticalHit();

void drawSpriteTransparent(int x, int y, const uint16_t* sprite);
void drawSpriteTransparent64(int x, int y, const uint16_t* sprite);

void refreshDisplay();

#endif