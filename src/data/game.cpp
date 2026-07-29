//
// Created by james on 7/17/2026.
//

#include "game.h"
#include "dungeon/dungeonplayer.h"

GameState gameState = GAME_START;
TownOption townSelection = TOWN_GOBLINS;
Character player;

Direction moveDirection = DIR_NORTH;
MapPosition previousPlayerPosition = {0, 0};
Direction previousMoveDirection = DIR_NORTH;

bool lastMoveWasDiagonal = false;
bool fullRedrawNeeded = true;
bool needsRedraw = true;
RedrawType redrawType = REDRAW_FULL;


void rotateDirectionCW()
{
    moveDirection =
      (Direction)((moveDirection + 1) % 8);
}

void rotateDirectionCCW()
{
    moveDirection =
      (Direction)((moveDirection + 7) % 8);
}

const DirectionOffset directionOffsets[] =
{
    {  0, -1 }, // North
    {  1, -1 }, // Northeast
    {  1,  0 }, // East
    {  1,  1 }, // Southeast
    {  0,  1 }, // South
    { -1,  1 }, // Southwest
    { -1,  0 }, // West
    { -1, -1 }  // Northwest
};

