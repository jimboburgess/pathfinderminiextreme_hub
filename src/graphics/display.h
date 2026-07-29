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
void redrawForestMessage();
void drawDungeonScreen();
void drawEntity(const Entity& entity);
void drawForestTile(int x, int y);
void redrawForestTile(int x, int y);

void markTileDirty(int x, int y);
void redrawDirtyTiles();

const uint8_t MAX_DIRTY_TILES = 32;

struct DirtyTile
{
    int8_t x;
    int8_t y;
};

extern DirtyTile dirtyTiles[MAX_DIRTY_TILES];
extern uint8_t dirtyTileCount;


void drawCriticalHit();

void drawSpriteTransparent(int x,int y,const uint16_t* sprite);
void drawSpriteTransparent(int x,int y,const uint16_t* sprite,uint8_t width,uint8_t height);
void drawSpriteTransparent64(int x, int y, const uint16_t* sprite);

void refreshDisplay();

#endif