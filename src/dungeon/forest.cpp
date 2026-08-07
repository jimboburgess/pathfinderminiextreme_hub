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

enum ForestEncounterTheme
{
    FOREST_ENCOUNTER_GOBLINS,
    FOREST_ENCOUNTER_UNDEAD,
    FOREST_ENCOUNTER_GIANT_SPIDER
};

static const MonsterID goblinForestMonsters[] =
{
    MONSTER_GOBLIN_SCIMITAR,
    MONSTER_GOBLIN_ARCHER
};

static const MonsterID undeadForestMonsters[] =
{
    MONSTER_SKELETON,
    MONSTER_ZOMBIE,
    MONSTER_GHOUL,
    MONSTER_WIGHT
};

static MonsterID chooseForestMonster(ForestEncounterTheme theme)
{
    switch (theme)
    {
        case FOREST_ENCOUNTER_GOBLINS:
            return goblinForestMonsters[random(
                sizeof(goblinForestMonsters) / sizeof(MonsterID))];

        case FOREST_ENCOUNTER_UNDEAD:
            return undeadForestMonsters[random(
                sizeof(undeadForestMonsters) / sizeof(MonsterID))];

        case FOREST_ENCOUNTER_GIANT_SPIDER:
            return MONSTER_GIANT_SPIDER;
    }

    return MONSTER_GOBLIN_SCIMITAR;
}

static bool canSpawnForestMonster(MonsterID monsterID, int x, int y)
{
    int footprint = monsterID == MONSTER_GIANT_SPIDER ? 2 : 1;

    for (int offsetY = 0; offsetY < footprint; offsetY++)
    {
        for (int offsetX = 0; offsetX < footprint; offsetX++)
        {
            int tileX = x + offsetX;
            int tileY = y + offsetY;

            if (getForestTile(tileX, tileY) == TILE_TREE ||
                getEntityAt(forestEntities, forestEntityCount,
                            tileX, tileY) != nullptr)
            {
                return false;
            }
        }
    }

    return true;
}

static void spawnForestEncounter()
{
    ForestEncounterTheme theme = static_cast<ForestEncounterTheme>(random(3));
    uint8_t monsterCount = theme == FOREST_ENCOUNTER_GIANT_SPIDER
        ? 1
        : random(1, 6);

    for (uint8_t spawned = 0; spawned < monsterCount; )
    {
        MonsterID monsterID = chooseForestMonster(theme);
        int footprint = monsterID == MONSTER_GIANT_SPIDER ? 2 : 1;
        bool placed = false;

        for (uint8_t attempt = 0; attempt < 80; attempt++)
        {
            int x = random(0, FOREST_WIDTH - footprint + 1);
            int y = random(0, FOREST_HEIGHT - footprint + 1);

            if (!canSpawnForestMonster(monsterID, x, y))
                continue;

            if (spawnMonster(forestEntities, forestEntityCount,
                             monsterID, x, y) != nullptr)
            {
                placed = true;
                spawned++;
            }

            break;
        }

        if (!placed)
            return;
    }
}


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

    forestMap[FOREST_HEIGHT - 2][FOREST_WIDTH / 2] = TILE_GRASS;

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

    spawnForestEncounter();
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

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

Entity forestEntities[MAX_ENTITIES];
uint8_t forestEntityCount = 0;
