#ifndef PATHFINDERMINIEXTREME_025_WORLDITEMS_H
#define PATHFINDERMINIEXTREME_025_WORLDITEMS_H

#include <stdint.h>

#include "data/entities.h"

constexpr int SQUARE_TARGET_ARMOR_CLASS = 5;

enum class WorldItemResult : uint8_t
{
    SUCCESS,
    INVALID_ACTOR,
    INVALID_ITEM,
    INVALID_TARGET,
    OUT_OF_RANGE,
    BLOCKED,
    NOT_ACTING,
    ACTION_SPENT,
    INVENTORY_FULL,
    NO_DROP_SPACE,
    WORLD_FULL,
    UNAVAILABLE
};

struct ThrownPotionResolution
{
    WorldItemResult result = WorldItemResult::INVALID_TARGET;
    Entity* target = nullptr;
    int attackRoll = 0;
    int attackTotal = 0;
    int healing = 0;
    bool hit = false;
    bool consumed = false;
};

bool isDroppedWorldItem(const Entity& entity);
const InventorySlot* getDroppedWorldItem(const Entity& entity);

bool squareTargetRangedAttackHits(int naturalRoll, int attackBonus);
bool isHealingPotionThrowTargetTile(
    const Entity& thrower, int targetX, int targetY);
ThrownPotionResolution resolveThrownHealingPotion(
    Entity& thrower,
    int targetX,
    int targetY,
    int attackRoll,
    int healingRoll);

WorldItemResult dropWorldItem(
    Entity& actor,
    const ItemInstance& item,
    Entity** createdDrop = nullptr);

WorldItemResult pickupWorldItem(Entity& actor, Entity& drop);

#endif
