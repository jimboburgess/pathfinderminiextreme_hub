//
// Created by james on 7/20/2026.
//

#include "forest.h"
#include "data/game.h"
#include "graphics/tiles.h"
#include "graphics/display.h"
#include "dungeonplayer.h"
#include "dungeon.h"
#include "graphics/sprites.h"
#include "graphics/monstersprites.h"
#include "audio/audio.h"
#include "graphics/messagelog.h"
#include "input/menu.h"



static TileType forestMap[FOREST_HEIGHT][FOREST_WIDTH];

TileType getForestTile(int x, int y){

    if (x < 0 || x >= FOREST_WIDTH ||
        y < 0 || y >= FOREST_HEIGHT)
    {
        return TILE_TREE;
    }

    return forestMap[y][x];
}


static void initForest()
{


    // Fill the map with grass.
    for (int y = 0; y < FOREST_HEIGHT; y++)
    {
        for (int x = 0; x < FOREST_WIDTH; x++)
        {
            forestMap[y][x] = TILE_GRASS;
        }
    }

    // Scatter random trees.
    const int NUM_TREES = 35;

    for (int i = 0; i < NUM_TREES; i++)
    {
        int x = random(2, FOREST_WIDTH - 5);   // Leave left and right sides clear.
        int y = random(0, FOREST_HEIGHT);

        forestMap[y][x] = TILE_TREE;
    }
    playerPosition.x = FOREST_WIDTH / 2;
    playerPosition.y = FOREST_HEIGHT / 2;

    clearEntities(
    forestEntities,
    forestEntityCount);

    spawnEntity(
        forestEntities,
        forestEntityCount,
        ENTITY_PLAYER,
        playerPosition.x,
        playerPosition.y);
    Entity* goblin;

    goblin = spawnEntity(
        forestEntities,
        forestEntityCount,
        ENTITY_ENEMY,
        5,
        5);

    if (goblin)
    {
        goblin->monsterID = MONSTER_GOBLIN_SCIMITAR;
    }

    goblin = spawnEntity(
        forestEntities,
        forestEntityCount,
        ENTITY_ENEMY,
        8,
        3);

    if (goblin)
    {
        goblin->monsterID = MONSTER_GOBLIN_ARCHER;
    }

    goblin = spawnEntity(
        forestEntities,
        forestEntityCount,
        ENTITY_ENEMY,
        10,
        10);

    if (goblin)
    {
        goblin->monsterID = MONSTER_BUGBEAR;
    }
}

int getForestPlayerX()
{
    return playerPosition.x;
}

int getForestPlayerY()
{
    return playerPosition.y;
}


void enterForest()
{
    gameState = GAME_FOREST;
    initForest();
    setGameMessage("Entered forest");

    previousPlayerPosition = playerPosition;
    previousMoveDirection = moveDirection;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

Entity forestEntities[MAX_ENTITIES];
uint8_t forestEntityCount = 0;