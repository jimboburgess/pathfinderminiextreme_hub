#include "map/worlditems.h"

#include "characters/combat_rules.h"
#include "characters/items.h"
#include "data/entityspawn.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/actioneconomy.h"
#include "dungeon/combat.h"
#include "graphics/tiles.h"
#include "graphics/actionpose.h"
#include "map/activemap.h"

namespace
{
Entity* alliedPlayerAt(const Entity& thrower, int x, int y)
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    for (uint8_t i = 0; entities != nullptr && i < entityCount; ++i)
    {
        Entity& candidate = entities[i];
        if (&candidate != &thrower && candidate.active &&
            candidate.type == ENTITY_PLAYER &&
            candidate.character.team == thrower.character.team &&
            (candidate.character.state == STATE_ALIVE ||
             candidate.character.state == STATE_UNCONSCIOUS) &&
            entityOccupiesTile(candidate, x, y))
            return &candidate;
    }
    return nullptr;
}

bool canUseStandardItemAction(const Entity& actor)
{
    if (!combat.active) return true;
    return getCurrentCombatant() == &actor && isPlayerTurn() &&
           combat.waitingForPlayer && canCharacterAct(actor.character) &&
           canUseStandardAction(actor.turn);
}

bool canUseMoveItemAction(const Entity& actor)
{
    if (!combat.active) return true;
    return getCurrentCombatant() == &actor && isPlayerTurn() &&
           combat.waitingForPlayer && canCharacterAct(actor.character) &&
           canUseMoveAction(actor.turn);
}

bool validDropTerrain(TileType tile)
{
    return isDungeonFloorTerrain(tile) || tile == TILE_GRASS ||
           tile == TILE_MUD || tile == TILE_BRUSH || tile == TILE_STONE;
}

bool findDropTile(const Entity& actor, int& x, int& y)
{
    static const int8_t offsets[][2] =
    {
        { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
        { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 }
    };
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    for (const auto& offset : offsets)
    {
        const int candidateX = actor.x + offset[0];
        const int candidateY = actor.y + offset[1];
        if (!isInsideActiveMap(candidateX, candidateY) ||
            !validDropTerrain(getActiveMapTile(candidateX, candidateY)) ||
            getEntityAt(entities, entityCount, candidateX, candidateY) != nullptr)
            continue;
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}
}

bool isDroppedWorldItem(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_LOOT &&
           entity.loot.generated && entity.loot.itemCount == 1 &&
           entity.loot.slots[0].quantity > 0 &&
           isValidItemInstance(entity.loot.slots[0].item);
}

const InventorySlot* getDroppedWorldItem(const Entity& entity)
{
    return isDroppedWorldItem(entity) ? &entity.loot.slots[0] : nullptr;
}

bool squareTargetRangedAttackHits(int naturalRoll, int attackBonus)
{
    return resolveAttackRoll(
        naturalRoll, attackBonus, SQUARE_TARGET_ARMOR_CLASS,
        21, false).hit;
}

bool isHealingPotionThrowTargetTile(
    const Entity& thrower, int targetX, int targetY)
{
    return isInsideActiveMap(targetX, targetY) &&
           getEntityGridDistanceToTile(thrower, targetX, targetY) <=
               THROWN_POTION_RANGE_TILES &&
           isAbilityTargetTileInRange(
               thrower, ABILITY_THROW_HEALING_POTION, targetX, targetY) &&
           alliedPlayerAt(thrower, targetX, targetY) != nullptr;
}

ThrownPotionResolution resolveThrownHealingPotion(
    Entity& thrower,
    int targetX,
    int targetY,
    int attackRoll,
    int healingRoll)
{
    ThrownPotionResolution resolution;
    resolution.attackRoll = attackRoll;
    if (!thrower.active || thrower.type != ENTITY_PLAYER ||
        !canCharacterAct(thrower.character))
    {
        resolution.result = WorldItemResult::INVALID_ACTOR;
        return resolution;
    }
    if (!hasItem(thrower.character, ITEM_POTION_CURE_LIGHT_WOUNDS))
    {
        resolution.result = WorldItemResult::INVALID_ITEM;
        return resolution;
    }
    if (!canUseStandardItemAction(thrower))
    {
        resolution.result = combat.active &&
            getCurrentCombatant() == &thrower
                ? WorldItemResult::ACTION_SPENT
                : WorldItemResult::NOT_ACTING;
        return resolution;
    }
    if (!isInsideActiveMap(targetX, targetY))
    {
        resolution.result = WorldItemResult::INVALID_TARGET;
        return resolution;
    }
    if (getEntityGridDistanceToTile(thrower, targetX, targetY) >
        THROWN_POTION_RANGE_TILES)
    {
        resolution.result = WorldItemResult::OUT_OF_RANGE;
        return resolution;
    }
    if (!isAbilityTargetTileInRange(
            thrower, ABILITY_THROW_HEALING_POTION, targetX, targetY))
    {
        resolution.result = WorldItemResult::BLOCKED;
        return resolution;
    }
    resolution.target = alliedPlayerAt(thrower, targetX, targetY);
    if (resolution.target == nullptr)
    {
        resolution.result = WorldItemResult::INVALID_TARGET;
        return resolution;
    }
    if (!removeItem(thrower.character, ITEM_POTION_CURE_LIGHT_WOUNDS))
    {
        resolution.result = WorldItemResult::INVALID_ITEM;
        return resolution;
    }

    resolution.consumed = true;
    triggerEntityActionPose(thrower);
    resolution.attackTotal = attackRoll + getRangedAttackBonus(thrower.character);
    resolution.hit = squareTargetRangedAttackHits(
        attackRoll, getRangedAttackBonus(thrower.character));
    if (resolution.hit)
    {
        resolution.healing = healCharacter(
            resolution.target->character,
            calculateCureLightWoundsPotionHealing(healingRoll));
    }
    if (combat.active)
        thrower.turn.standardActionUsed = true;
    resolution.result = WorldItemResult::SUCCESS;
    return resolution;
}

WorldItemResult dropWorldItem(
    Entity& actor,
    const ItemInstance& item,
    Entity** createdDrop)
{
    if (createdDrop != nullptr) *createdDrop = nullptr;
    if (!actor.active || actor.type != ENTITY_PLAYER)
        return WorldItemResult::INVALID_ACTOR;
    if (!hasItem(actor.character, item))
        return WorldItemResult::INVALID_ITEM;
    if (!canUseMoveItemAction(actor))
        return combat.active && getCurrentCombatant() == &actor
            ? WorldItemResult::ACTION_SPENT
            : WorldItemResult::NOT_ACTING;

    int dropX = 0;
    int dropY = 0;
    if (!findDropTile(actor, dropX, dropY))
        return WorldItemResult::NO_DROP_SPACE;

    Entity* drop = spawnActiveMapEntity(
        ENTITY_LOOT,
        static_cast<uint8_t>(dropX),
        static_cast<uint8_t>(dropY));
    if (drop == nullptr) return WorldItemResult::WORLD_FULL;

    drop->loot.generated = true;
    drop->loot.itemCount = 1;
    drop->loot.slots[0].item = item;
    drop->loot.slots[0].quantity = 1;
    if (!removeItem(actor.character, item, 1))
    {
        *drop = Entity{};
        return WorldItemResult::INVALID_ITEM;
    }
    if (combat.active)
    {
        actor.turn.moveActionUsed = true;
        actor.turn.movementRemaining = 0;
    }
    if (createdDrop != nullptr) *createdDrop = drop;
    return WorldItemResult::SUCCESS;
}

WorldItemResult pickupWorldItem(Entity& actor, Entity& drop)
{
    const InventorySlot* slot = getDroppedWorldItem(drop);
    if (!actor.active || actor.type != ENTITY_PLAYER)
        return WorldItemResult::INVALID_ACTOR;
    if (slot == nullptr) return WorldItemResult::UNAVAILABLE;
    if (getEntityGridDistance(actor, drop) > 1)
        return WorldItemResult::OUT_OF_RANGE;
    if (!canUseMoveItemAction(actor))
        return combat.active && getCurrentCombatant() == &actor
            ? WorldItemResult::ACTION_SPENT
            : WorldItemResult::NOT_ACTING;

    const ItemInstance item = slot->item;
    if (!addItem(actor.character, item, 1))
        return WorldItemResult::INVENTORY_FULL;
    if (combat.active)
    {
        actor.turn.moveActionUsed = true;
        actor.turn.movementRemaining = 0;
    }
    drop = Entity{};
    return WorldItemResult::SUCCESS;
}
