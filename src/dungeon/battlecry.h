#ifndef PATHFINDERMINIEXTREME_025_BATTLE_CRY_H
#define PATHFINDERMINIEXTREME_025_BATTLE_CRY_H

#include "abilityresolver.h"
#include "map/activemap.h"
#include "data/entities.h"

constexpr uint8_t BATTLE_CRY_RADIUS_TILES = 3;
constexpr uint8_t BATTLE_CRY_DURATION_ROUNDS = 2;

inline bool isBattleCryEligibleRecipient(
    const Entity& chieftain,
    const Entity& candidate)
{
    return candidate.active && candidate.type == ENTITY_MONSTER &&
           candidate.character.state == STATE_ALIVE &&
           candidate.character.team == TEAM_MONSTER &&
           candidate.character.creatureType == CREATURE_GOBLIN &&
           getEntityGridDistance(chieftain, candidate) <=
               BATTLE_CRY_RADIUS_TILES;
}

inline bool monsterDefinitionHasBattleCry(const Monster* monster)
{
    if (monster == nullptr)
        return false;

    for (AbilityID ability : monster->specialAbilities)
    {
        if (ability == ABILITY_BATTLE_CRY)
            return true;
    }
    return false;
}

inline bool shouldUseBattleCry(
    const Entity& chieftain,
    bool hasEligibleRecipient)
{
    return chieftain.active && chieftain.type == ENTITY_MONSTER &&
           chieftain.character.state == STATE_ALIVE &&
           chieftain.character.team == TEAM_MONSTER &&
           monsterDefinitionHasBattleCry(chieftain.monster) &&
           !chieftain.turn.oncePerCombatAbilityUsed &&
           !chieftain.turn.standardActionUsed && hasEligibleRecipient;
}

inline bool applyBattleCryCondition(
    const Entity& chieftain,
    Entity& recipient)
{
    return applyAbilityModifierCondition(
        chieftain, recipient.character, ABILITY_BATTLE_CRY);
}

#endif // PATHFINDERMINIEXTREME_025_BATTLE_CRY_H
