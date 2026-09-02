#ifndef PATHFINDERMINIEXTREME_025_MOVEMENT_H
#define PATHFINDERMINIEXTREME_025_MOVEMENT_H

#include <stdint.h>

#include "characters/conditions.h"
#include "graphics/tiles.h"

struct Entity;

constexpr uint8_t RUBBLE_ACROBATICS_DC = 12;

// Base-terrain movement costs are independent from temporary map effects.
// Future terrain can extend this table without changing player or monster
// movement callers.
inline uint8_t getTerrainMovementCost(TileType tile)
{
    return tile == TILE_RUBBLE ? 2 : 1;
}

inline uint8_t resolveTerrainMovementCost(
    TileType tile,
    bool terrainCheckSucceeded)
{
    return tile == TILE_RUBBLE && terrainCheckSucceeded
        ? 1
        : getTerrainMovementCost(tile);
}

inline bool canPayMovementCost(uint8_t movementRemaining, uint8_t cost)
{
    return cost > 0 && movementRemaining >= cost;
}

enum StandForMovementResult : uint8_t
{
    STAND_NOT_PRONE,
    STAND_COMPLETED,
    STAND_NO_MOVEMENT
};

// One tile normally costs one movement point. If any square of a creature's
// destination footprint is difficult, the whole step costs two.
uint8_t getMovementCost(
    const Entity& mover,
    int targetX,
    int targetY);

// Recalculates the complete movement cost while substituting the result of a
// single terrain check. Temporary difficult map effects still apply.
uint8_t getMovementCostWithTerrainCheck(
    const Entity& mover,
    int targetX,
    int targetY,
    TileType checkedTerrain,
    bool terrainCheckSucceeded);

bool canAffordMovementCost(
    const Entity& mover,
    int targetX,
    int targetY);

bool canAffordMovementCost(
    const Entity& mover,
    uint8_t resolvedCost);

void spendMovementCost(
    Entity& mover,
    int targetX,
    int targetY);

void spendMovementCost(Entity& mover, uint8_t resolvedCost);

// Automatic stand-up is shared by player and monster movement. In combat it
// costs one movement point and consumes the movement input without changing
// position; outside combat it simply consumes the input.
StandForMovementResult tryStandForMovement(
    Entity& mover,
    bool spendMovementPoint);

// Shared post-commit terrain hook. It runs exactly once for each successful
// movement step, never once per display/update frame.
ConditionType handleEnteredTile(
    Entity& mover,
    int targetX,
    int targetY,
    bool* trapTriggered = nullptr);

#endif // PATHFINDERMINIEXTREME_025_MOVEMENT_H
