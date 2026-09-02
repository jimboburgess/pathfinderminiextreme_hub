#include "map/movement.h"

#include "map/activemap.h"
#include "map/mapeffects.h"
#include "characters/conditions.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/traps.h"

namespace
{
uint8_t calculateMovementCost(
    const Entity& mover,
    int targetX,
    int targetY,
    TileType checkedTerrain,
    bool terrainCheckSucceeded,
    bool hasTerrainCheck)
{
    if (hasCondition(mover.character, CONDITION_WEBBED))
        return 0;

    uint8_t width = getEntityTileWidth(mover);
    uint8_t height = getEntityTileHeight(mover);

    if (targetX < 0 || targetY < 0 ||
        targetX + width > getActiveMapWidth() ||
        targetY + height > getActiveMapHeight())
    {
        return 0;
    }

    uint8_t movementCost = 1;

    for (uint8_t offsetY = 0; offsetY < height; offsetY++)
    {
        for (uint8_t offsetX = 0; offsetX < width; offsetX++)
        {
            int x = targetX + offsetX;
            int y = targetY + offsetY;

            const TileType tile = getActiveMapTile(x, y);
            const uint8_t terrainCost =
                hasTerrainCheck && tile == checkedTerrain
                    ? resolveTerrainMovementCost(
                          tile, terrainCheckSucceeded)
                    : getTerrainMovementCost(tile);

            if (terrainCost > movementCost)
                movementCost = terrainCost;

            if (hasDifficultMapEffectAt(x, y) && movementCost < 2)
                movementCost = 2;
        }
    }

    return movementCost;
}
}

uint8_t getMovementCost(
    const Entity& mover,
    int targetX,
    int targetY)
{
    return calculateMovementCost(
        mover, targetX, targetY, TILE_VOID, false, false);
}

uint8_t getMovementCostWithTerrainCheck(
    const Entity& mover,
    int targetX,
    int targetY,
    TileType checkedTerrain,
    bool terrainCheckSucceeded)
{
    return calculateMovementCost(
        mover, targetX, targetY,
        checkedTerrain, terrainCheckSucceeded, true);
}

bool canAffordMovementCost(
    const Entity& mover,
    int targetX,
    int targetY)
{
    uint8_t cost = getMovementCost(mover, targetX, targetY);
    return canAffordMovementCost(mover, cost);
}

bool canAffordMovementCost(
    const Entity& mover,
    uint8_t resolvedCost)
{
    return canPayMovementCost(
        mover.turn.movementRemaining, resolvedCost);
}

void spendMovementCost(
    Entity& mover,
    int targetX,
    int targetY)
{
    uint8_t cost = getMovementCost(mover, targetX, targetY);

    spendMovementCost(mover, cost);
}

void spendMovementCost(Entity& mover, uint8_t resolvedCost)
{
    const uint8_t cost = resolvedCost;

    if (cost == 0)
        return;

    if (mover.turn.movementRemaining <= cost)
        mover.turn.movementRemaining = 0;
    else
        mover.turn.movementRemaining -= cost;
}

StandForMovementResult tryStandForMovement(
    Entity& mover,
    bool spendMovementPoint)
{
    if (!hasCondition(mover.character, CONDITION_PRONE))
        return STAND_NOT_PRONE;

    if (spendMovementPoint && mover.turn.movementRemaining == 0)
        return STAND_NO_MOVEMENT;

    if (spendMovementPoint)
        mover.turn.movementRemaining--;

    removeCondition(mover.character, CONDITION_PRONE);
    return STAND_COMPLETED;
}

ConditionType handleEnteredTile(
    Entity& mover,
    int targetX,
    int targetY,
    bool* trapTriggered)
{
    const ConditionType conditionApplied = handleEnteredMapEffects(
        mover, targetX, targetY).conditionApplied;

    // Persistent room features share the same post-commit movement seam as
    // Grease and Web. This runs once per successful step, never per frame.
    const bool triggered = triggerTrapForEntityAt(mover, targetX, targetY);
    if (trapTriggered != nullptr)
        *trapTriggered = triggered;

    return conditionApplied;
}
