//
// Created by james on 7/20/2026.
//

#include "forest.h"
#include "data/game.h"
#include "graphics/tiles.h"
#include "graphics/display.h"
#include "dungeonplayer.h"
#include "dungeon.h"
#include "turns.h"
#include "graphics/sprites.h"
#include "graphics/monstersprites.h"
#include "audio/audio.h"
#include "data/entityspawn.h"
#include "graphics/messagelog.h"
#include "input/menu.h"
#include "characters/characters.h"



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
    //--------------------------------------------------
    // Fill the map with grass.
    //--------------------------------------------------

    for (int y = 0; y < FOREST_HEIGHT; y++)
    {
        for (int x = 0; x < FOREST_WIDTH; x++)
        {
            forestMap[y][x] = TILE_GRASS;
        }
    }

    //--------------------------------------------------
    // Scatter random trees.
    //--------------------------------------------------

    const int NUM_TREES = 35;

    for (int i = 0; i < NUM_TREES; i++)
    {
        int x = random(2, FOREST_WIDTH - 5);
        int y = random(0, FOREST_HEIGHT);

        forestMap[y][x] = TILE_TREE;
    }

    //--------------------------------------------------
    // Spawn entities.
    //--------------------------------------------------

    clearEntities(
        forestEntities,
        forestEntityCount);

    Entity* playerEntity = spawnEntity(
        forestEntities,
        forestEntityCount,
        ENTITY_PLAYER,
        FOREST_WIDTH / 2,
        FOREST_HEIGHT - 2);

    if (playerEntity)
    {
        playerEntity->character = player;

        playerEntity->sprite =
            getPlayerSprite(player.characterClass);
    }

    spawnMonster(
        forestEntities,
        forestEntityCount,
        MONSTER_GOBLIN_SCIMITAR,
        2,
        2);

    spawnMonster(
        forestEntities,
        forestEntityCount,
        MONSTER_GOBLIN_ARCHER,
        12,
        3);
}

int getForestPlayerX()
{
    Entity* player = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    return player ? player->x : 0;
}

int getForestPlayerY()
{
    Entity* player = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    return player ? player->y : 0;
}

void enterForest()
{
    gameState = GAME_FOREST;
    initForest();
    setGameMessage("Entered forest");

    Entity* player = getPlayerEntity(
    forestEntities,
    forestEntityCount);

    if (player)
    {
        previousPlayerPosition.x = player->x;
        previousPlayerPosition.y = player->y;
    }

    previousMoveDirection = moveDirection;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

Entity forestEntities[MAX_ENTITIES];
uint8_t forestEntityCount = 0;