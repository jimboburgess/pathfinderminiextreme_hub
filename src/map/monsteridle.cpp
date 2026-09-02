#include "map/monsteridle.h"

#include <Arduino.h>

#include "characters/sheet.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/monsterscripts.h"
#include "graphics/display.h"
#include "input/inventorymenu.h"
#include "input/menu.h"
#include "map/activemap.h"

namespace
{
constexpr uint32_t MIN_IDLE_ACTION_MS = 1500;
constexpr uint32_t MAX_IDLE_ACTION_MS = 2500;
constexpr uint8_t MIN_PATROL_STEPS = 2;
constexpr uint8_t MAX_PATROL_STEPS = 5;

const Direction cardinalDirections[] =
{
    DIR_NORTH,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST
};

bool idleSuspendedByCombat = false;
bool idleSuspendedByModal = false;

bool timeReached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

uint32_t randomIdleDelay()
{
    return static_cast<uint32_t>(random(
        MIN_IDLE_ACTION_MS, MAX_IDLE_ACTION_MS + 1));
}

Direction randomCardinalDirection()
{
    const uint8_t count = sizeof(cardinalDirections) /
                          sizeof(cardinalDirections[0]);
    return cardinalDirections[random(count)];
}

void scheduleNextIdleAction(Entity& monster, uint32_t now)
{
    monster.nextIdleActionTime = now + randomIdleDelay();
}

bool isLivingMonster(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_MONSTER &&
           entity.monster != nullptr &&
           entity.character.state == STATE_ALIVE;
}

bool isHostileToPlayer(const Entity& monster, const Entity& player)
{
    return monster.character.team != player.character.team &&
           monster.monster != nullptr &&
           monster.monster->script != SCRIPT_PASSIVE;
}

bool footprintsOverlapAt(
    const Entity& first,
    int firstX,
    int firstY,
    const Entity& second)
{
    const int firstRight = firstX + getEntityTileWidth(first);
    const int firstBottom = firstY + getEntityTileHeight(first);
    const int secondRight = second.x + getEntityTileWidth(second);
    const int secondBottom = second.y + getEntityTileHeight(second);

    return firstX < secondRight && firstRight > second.x &&
           firstY < secondBottom && firstBottom > second.y;
}

bool explorationIsModal()
{
    return menuState.isOpen || isInventoryMenuOpen() ||
           isCharacterSheetVisible() || isInspectingEntities() ||
           isPlayerTargetingAttack() || isPlayerAttackResolving() ||
           isPlayerTargetingAbility() || isAbilityResolving();
}

void rescheduleMovableMonsters(uint32_t now)
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    if (entities == nullptr)
        return;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];
        if (!isLivingMonster(monster))
            continue;

        const MonsterIdleBehavior behavior = monster.monster->idleBehavior;
        if (behavior == IDLE_PATROL || behavior == IDLE_WANDER)
            scheduleNextIdleAction(monster, now);
    }
}

bool attemptIdleStep(
    Entity& monster,
    Entity& player,
    Direction direction)
{
    const DirectionOffset offset = directionOffsets[direction];
    const int targetX = static_cast<int>(monster.x) + offset.dx;
    const int targetY = static_cast<int>(monster.y) + offset.dy;

    // Detect direct contact before the ordinary occupancy check rejects it.
    // Contact reveals both sides and hands control to the existing combat
    // startup without granting an exploration attack.
    if (footprintsOverlapAt(monster, targetX, targetY, player))
    {
        if (isHostileToPlayer(monster, player))
        {
            monster.awareOfPlayer = true;
            monster.revealedToPlayer = true;
            markEntityFootprintDirty(monster);
            Serial.printf("%s contacts player; starting combat\n",
                          getEntityName(&monster));
            startCombat();
        }
        return false;
    }

    // The shared combat movement validator is footprint-aware and already
    // enforces active-map bounds, terrain, doors/exits, and entity occupancy.
    if (!canMonsterMoveTo(&monster, targetX, targetY))
        return false;

    const int oldX = monster.x;
    const int oldY = monster.y;
    markEntityFootprintDirtyAt(monster, oldX, oldY);
    monster.x = static_cast<uint8_t>(targetX);
    monster.y = static_cast<uint8_t>(targetY);
    markEntityFootprintDirty(monster);
    return true;
}

void updatePatrol(Entity& monster, Entity& player)
{
    if (monster.idleStepsRemaining == 0)
    {
        monster.idleDirection = randomCardinalDirection();
        monster.idleStepsRemaining = static_cast<uint8_t>(random(
            MIN_PATROL_STEPS, MAX_PATROL_STEPS + 1));
    }

    if (attemptIdleStep(monster, player, monster.idleDirection))
    {
        if (monster.idleStepsRemaining > 0)
            monster.idleStepsRemaining--;
    }
    else
    {
        // Do not hammer the same blocked square at the next interval.
        monster.idleStepsRemaining = 0;
    }
}

void updateWander(Entity& monster, Entity& player)
{
    monster.idleDirection = randomCardinalDirection();
    attemptIdleStep(monster, player, monster.idleDirection);
}
}

void updateMonsterIdleBehavior()
{
    const uint32_t now = millis();

    if (combat.active)
    {
        idleSuspendedByCombat = true;
        return;
    }

    if (gameState != GAME_DUNGEON && gameState != GAME_FOREST)
        return;

    // Combat may have lasted beyond every stored timer. Stagger survivors on
    // the first exploration frame rather than moving them all immediately.
    if (idleSuspendedByCombat)
    {
        idleSuspendedByCombat = false;
        rescheduleMovableMonsters(now);
        return;
    }

    if (explorationIsModal())
    {
        idleSuspendedByModal = true;
        return;
    }

    // Like combat, a menu or targeting cursor may remain open longer than
    // every timer. Resume with fresh independent delays instead of moving a
    // crowd on the first frame after the modal closes.
    if (idleSuspendedByModal)
    {
        idleSuspendedByModal = false;
        rescheduleMovableMonsters(now);
        return;
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    Entity* player = getActiveMapPlayer();
    if (entities == nullptr || player == nullptr ||
        player->character.state != STATE_ALIVE)
    {
        return;
    }

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];
        if (!isLivingMonster(monster) || monster.awareOfPlayer)
            continue;

        const MonsterIdleBehavior behavior = monster.monster->idleBehavior;
        if (behavior == IDLE_STATIONARY || behavior == IDLE_HIDE)
            continue;

        if (monster.nextIdleActionTime == 0)
        {
            scheduleNextIdleAction(monster, now);
            continue;
        }

        if (!timeReached(now, monster.nextIdleActionTime))
            continue;

        // Schedule before acting so even a blocked step or combat transition
        // cannot retrigger continuously on later frames.
        scheduleNextIdleAction(monster, now);

        if (behavior == IDLE_PATROL)
            updatePatrol(monster, *player);
        else
            updateWander(monster, *player);

        // Direct contact may have started combat. No later entity may finish
        // an idle step once combat owns the simulation.
        if (combat.active)
        {
            idleSuspendedByCombat = true;
            return;
        }
    }
}
