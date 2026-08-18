#include "map/movement.h"

#include "map/activemap.h"
#include "map/mapeffects.h"
#include "characters/conditions.h"
#include "data/entities.h"
#include "data/entityspawn.h"

uint8_t getMovementCost(
    const Entity& mover,
    int targetX,
    int targetY)
{
    uint8_t width = getEntityTileWidth(mover);
    uint8_t height = getEntityTileHeight(mover);

    if (targetX < 0 || targetY < 0 ||
        targetX + width > getActiveMapWidth() ||
        targetY + height > getActiveMapHeight())
    {
        return 0;
    }

    for (uint8_t offsetY = 0; offsetY < height; offsetY++)
    {
        for (uint8_t offsetX = 0; offsetX < width; offsetX++)
        {
            int x = targetX + offsetX;
            int y = targetY + offsetY;

            if (isBaseTerrainDifficultAt(x, y) ||
                hasDifficultMapEffectAt(x, y))
            {
                return 2;
            }
        }
    }

    return 1;
}

bool canAffordMovementCost(
    const Entity& mover,
    int targetX,
    int targetY)
{
    uint8_t cost = getMovementCost(mover, targetX, targetY);
    return cost > 0 && mover.turn.movementRemaining >= cost;
}

void spendMovementCost(
    Entity& mover,
    int targetX,
    int targetY)
{
    uint8_t cost = getMovementCost(mover, targetX, targetY);

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

ConditionType handleEnteredTile(Entity& mover, int targetX, int targetY)
{
    return handleEnteredMapEffects(
               mover, targetX, targetY).conditionApplied;
}
