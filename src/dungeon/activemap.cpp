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

bool isBaseTerrainDifficultAt(int x, int y)
{
    if (!isInsideActiveMap(x, y))
        return false;

    // No current dungeon or forest tile carries a difficult-terrain rule.
    // Keeping this query separate lets later terrain add that property
    // without coupling it to Grease or any other map effect.
    return false;
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

bool hasLineOfSightFromFootprintAt(
    const Entity& entity,
    int entityX,
    int entityY,
    int targetX,
    int targetY)
{
    if (!isInsideActiveMap(targetX, targetY))
        return false;

    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(entity);
         offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(entity);
             offsetX++)
        {
            if (hasLineOfSight(
                    entityX + offsetX,
                    entityY + offsetY,
                    targetX,
                    targetY))
            {
                return true;
            }
        }
    }

    return false;
}

int getEntityGridDistance(const Entity& first, const Entity& second)
{
    int firstRight = first.x + getEntityTileWidth(first) - 1;
    int firstBottom = first.y + getEntityTileHeight(first) - 1;
    int secondRight = second.x + getEntityTileWidth(second) - 1;
    int secondBottom = second.y + getEntityTileHeight(second) - 1;
    int horizontalDistance = 0;
    int verticalDistance = 0;

    if (firstRight < second.x)
        horizontalDistance = second.x - firstRight;
    else if (secondRight < first.x)
        horizontalDistance = first.x - secondRight;

    if (firstBottom < second.y)
        verticalDistance = second.y - firstBottom;
    else if (secondBottom < first.y)
        verticalDistance = first.y - secondBottom;

    return horizontalDistance > verticalDistance
        ? horizontalDistance
        : verticalDistance;
}

int getEntityGridDistanceToTile(
    const Entity& entity,
    int tileX,
    int tileY)
{
    int right = entity.x + getEntityTileWidth(entity) - 1;
    int bottom = entity.y + getEntityTileHeight(entity) - 1;
    int horizontalDistance = 0;
    int verticalDistance = 0;

    if (tileX < entity.x)
        horizontalDistance = entity.x - tileX;
    else if (tileX > right)
        horizontalDistance = tileX - right;

    if (tileY < entity.y)
        verticalDistance = entity.y - tileY;
    else if (tileY > bottom)
        verticalDistance = tileY - bottom;

    return horizontalDistance > verticalDistance
        ? horizontalDistance
        : verticalDistance;
}
