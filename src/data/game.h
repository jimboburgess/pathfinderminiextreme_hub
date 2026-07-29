//
// Created by james on 7/14/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_GAME_H
#define PATHFINDERMINIEXTREME_025_GAME_H

#include <Adafruit_ST7789.h>

#include "characters/characters.h"

//======================================================
// Game State
//======================================================

enum GameState
{
    GAME_START,
    GAME_CHARACTER_CREATION,
    GAME_TOWN,
    GAME_FOREST,
    GAME_DUNGEON
};

enum TownOption
{
    TOWN_GOBLINS,
    TOWN_STAY_HOME,
    TOWN_DUNGEON,
    TOWN_OPTION_COUNT
};

enum MapType
{
    MAP_TOWN,
    MAP_FOREST,
    MAP_DUNGEON
};

enum Direction
{
    DIR_NORTH,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST
};

struct DirectionOffset
{
    int8_t dx;
    int8_t dy;
};

struct MapPosition
{
    int x;
    int y;
};


//======================================================
// Global Game Objects
//======================================================

extern Adafruit_ST7789 tft;

extern GameState gameState;
extern TownOption townSelection;

enum RedrawType
{
    REDRAW_NONE,
    REDRAW_FULL,
    REDRAW_CURSOR,
    REDRAW_PLAYER,
    REDRAW_TILE,
    REDRAW_ENTITY,
};

extern RedrawType redrawType;

extern const DirectionOffset directionOffsets[];

extern bool lastMoveWasDiagonal;

extern Direction moveDirection;
extern MapPosition previousPlayerPosition;
extern Direction previousMoveDirection;

void rotateDirectionCW();
void rotateDirectionCCW();

extern Character player;

extern bool isCharacterSheetOpen;
extern bool fullRedrawNeeded;
extern bool needsRedraw;

#endif // PATHFINDERMINIEXTREME_025_GAME_H