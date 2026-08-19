#ifndef PATHFINDERMINIEXTREME_025_COMBATPOLICY_H
#define PATHFINDERMINIEXTREME_025_COMBATPOLICY_H

#include <stdint.h>

#include "characters/conditions.h"
#include "data/entities.h"

inline bool isHostileMonsterForCombat(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_MONSTER &&
           entity.character.team == TEAM_MONSTER;
}

inline bool isLivingHostileForCombat(const Entity& entity)
{
    return isHostileMonsterForCombat(entity) &&
           entity.character.state == STATE_ALIVE;
}

inline bool shouldMonsterJoinCombatRoster(const Entity& entity)
{
    return isLivingHostileForCombat(entity);
}

inline bool shouldMonsterRunCombatAI(const Entity& entity)
{
    return isLivingHostileForCombat(entity) && entity.awareOfPlayer;
}

inline uint8_t buildCombatRoster(
    Entity entities[],
    uint8_t entityCount,
    Entity* player,
    Entity* roster[],
    uint8_t rosterCapacity)
{
    if (entities == nullptr || roster == nullptr || rosterCapacity == 0)
        return 0;

    uint8_t rosterCount = 0;

    if (player != nullptr && player->active &&
        player->type == ENTITY_PLAYER &&
        player->character.team == TEAM_PLAYER &&
        player->character.state == STATE_ALIVE)
    {
        roster[rosterCount++] = player;
    }

    for (uint8_t i = 0; i < entityCount && rosterCount < rosterCapacity; i++)
    {
        if (&entities[i] == player ||
            !shouldMonsterJoinCombatRoster(entities[i]))
        {
            continue;
        }

        roster[rosterCount++] = &entities[i];
    }

    return rosterCount;
}

inline uint8_t countLivingHostilesInCombatRoster(
    Entity* const roster[],
    uint8_t rosterCount)
{
    uint8_t enemyCount = 0;

    for (uint8_t i = 0; i < rosterCount; i++)
    {
        if (roster[i] != nullptr && isLivingHostileForCombat(*roster[i]))
            enemyCount++;
    }

    return enemyCount;
}

inline bool shouldContinueCombatAfterOpeningAttack(
    bool wasAmbush,
    bool targetDefeated,
    bool anotherMonsterDetectedPlayer)
{
    return !wasAmbush || !targetDefeated || anotherMonsterDetectedPlayer;
}

inline bool qualifiesForSneakAttack(
    const Entity& attacker,
    const Entity& target)
{
    return attacker.active && target.active &&
           attacker.character.state == STATE_ALIVE &&
           target.character.state == STATE_ALIVE &&
           attacker.character.characterClass == CLASS_ROGUE &&
           attacker.character.team != TEAM_NEUTRAL &&
           target.character.team != TEAM_NEUTRAL &&
           attacker.character.team != target.character.team &&
           hasCondition(target.character, CONDITION_FLAT_FOOTED);
}

#endif
