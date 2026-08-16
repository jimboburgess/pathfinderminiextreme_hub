#ifndef PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H
#define PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H

#include <stdint.h>

#include "characters/abilities.h"

struct Entity;

enum AbilityResult : uint8_t
{
    ABILITY_RESULT_SUCCESS,
    ABILITY_RESULT_INVALID_ABILITY,
    ABILITY_RESULT_UNSUPPORTED,
    ABILITY_RESULT_INVALID_CASTER,
    ABILITY_RESULT_INVALID_TARGET,
    ABILITY_RESULT_OUT_OF_RANGE,
    ABILITY_RESULT_NO_LINE_OF_SIGHT,
    ABILITY_RESULT_NOT_ENOUGH_MP,
    ABILITY_RESULT_NO_STANDARD_ACTION
};

struct AbilityResolution
{
    AbilityResult result = ABILITY_RESULT_INVALID_ABILITY;
    int damage = 0;
    int healing = 0;
    bool targetDefeated = false;
    uint8_t levelReached = 0;
};

// The first resolver version deliberately accepts only instant, standard-
// action abilities that target one enemy or the caster and contain either
// damage effects or healing effects.
bool isAbilitySupported(AbilityID abilityID);

// Performs every legality check without changing either entity.
AbilityResult validateAbility(
    const Entity& caster,
    const Entity* target,
    AbilityID abilityID);

// Authoritative execution path shared by player and monster spellcasting.
AbilityResolution resolveAbility(
    Entity& caster,
    Entity* target,
    AbilityID abilityID);

const char* getAbilityResultMessage(AbilityResult result);

#endif // PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H
