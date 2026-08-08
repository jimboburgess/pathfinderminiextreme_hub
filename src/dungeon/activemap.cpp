#include "activemap.h"

#include <stdlib.h>

#include "dungeon.h"
#include "forest.h"
#include "data/entityspawn.h"
#include "data/game.h"

namespace
{
bool blocksSight(TileType tile)
{
    return tile == TILE_TREE || tile == TILE_WALL;
}
}

Entity* getActiveMapEntities(uint8_t& entityCount)
{
    switch (gameState)
    {
        case GAME_FOREST:
            entityCount = forestEntityCount;
            return forestEntities;

        case GAME_DUNGEON:
            entityCount = dungeon.entityCount;
            return dungeon.entities;

        default:
            entityCount = 0;
            return nullptr;
    }
}

Entity* getActiveMapPlayer()
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    return entities != nullptr
        ? getPlayerEntity(entities, entityCount)
        : nullptr;
}

int getActiveMapWidth()
{
    switch (gameState)
    {
        case GAME_FOREST:
            return FOREST_WIDTH;

        case GAME_DUNGEON:
            return ROOM_SIZE;

        default:
            return 0;
    }
}

int getActiveMapHeight()
{
    switch (gameState)
    {
        case GAME_FOREST:
            return FOREST_HEIGHT;

        case GAME_DUNGEON:
            return ROOM_SIZE;

        default:
            return 0;
    }
}

bool isInsideActiveMap(int x, int y)
{
    return x >= 0 && x < getActiveMapWidth() &&
           y >= 0 && y < getActiveMapHeight();
}

TileType getActiveMapTile(int x, int y)
{
    if (!isInsideActiveMap(x, y))
        return TILE_WALL;

    if (gameState == GAME_FOREST)
        return getForestTile(x, y);

    if (gameState == GAME_DUNGEON)
        return dungeon.rooms[dungeon.currentRoom].map.tiles[y][x];

    return TILE_WALL;
}

bool hasLineOfSight(int startX, int startY, int endX, int endY)
{
    if (!isInsideActiveMap(startX, startY) ||
        !isInsideActiveMap(endX, endY))
    {
        return false;
    }

    int currentX = startX;
    int currentY = startY;
    int deltaX = abs(endX - startX);
    int deltaY = abs(endY - startY);
    int stepX = startX < endX ? 1 : -1;
    int stepY = startY < endY ? 1 : -1;
    int error = deltaX - deltaY;

    while (true)
    {
        if (!(currentX == startX && currentY == startY) &&
            !(currentX == endX && currentY == endY) &&
            blocksSight(getActiveMapTile(currentX, currentY)))
        {
            return false;
        }

        if (currentX == endX && currentY == endY)
            return true;

        int doubledError = error * 2;

        if (doubledError > -deltaY)
        {
            error -= deltaY;
            currentX += stepX;
        }

        if (doubledError < deltaX)
        {
            error += deltaX;
            currentY += stepY;
        }
    }
}

bool hasLineOfSightBetweenFootprintsAt(
    const Entity& attacker,
    int attackerX,
    int attackerY,
    const Entity& target)
{
    for (uint8_t attackerOffsetY = 0;
         attackerOffsetY < getEntityTileHeight(attacker);
         attackerOffsetY++)
    {
        for (uint8_t attackerOffsetX = 0;
             attackerOffsetX < getEntityTileWidth(attacker);
             attackerOffsetX++)
        {
            for (uint8_t targetOffsetY = 0;
                 targetOffsetY < getEntityTileHeight(target);
                 targetOffsetY++)
            {
                for (uint8_t targetOffsetX = 0;
                     targetOffsetX < getEntityTileWidth(target);
                     targetOffsetX++)
                {
                    if (hasLineOfSight(
                            attackerX + attackerOffsetX,
                            attackerY + attackerOffsetY,
                            target.x + targetOffsetX,
                            target.y + targetOffsetY))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}
