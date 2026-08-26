#ifndef PATHFINDERMINIEXTREME_025_MOVEMENT_H
#define PATHFINDERMINIEXTREME_025_MOVEMENT_H

#include <stdint.h>

#include "characters/conditions.h"

struct Entity;

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

bool canAffordMovementCost(
    const Entity& mover,
    int targetX,
    int targetY);

void spendMovementCost(
    Entity& mover,
    int targetX,
    int targetY);

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
