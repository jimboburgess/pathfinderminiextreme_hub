#include "abilityresolver.h"

#include "activemap.h"
#include "combat.h"
#include "characters/characters.h"
#include "data/entities.h"

namespace
{
bool isCombatEntityType(EntityType type)
{
    return type == ENTITY_PLAYER ||
           type == ENTITY_MONSTER ||
           type == ENTITY_NPC;
}

bool areOpposingTeams(const Entity& caster, const Entity& target)
{
    return caster.character.team != TEAM_NEUTRAL &&
           target.character.team != TEAM_NEUTRAL &&
           caster.character.team != target.character.team;
}

bool hasSupportedDelivery(const Ability& ability)
{
    if (ability.target == TARGET_ENEMY)
    {
        return ability.delivery == DELIVERY_TARGET &&
               ability.rangeTiles > 0;
    }

    if (ability.target == TARGET_SELF)
    {
        return ability.delivery == DELIVERY_AUTOMATIC ||
               ability.delivery == DELIVERY_TOUCH ||
               ability.delivery == DELIVERY_TARGET;
    }

    return false;
}

bool hasSupportedEffects(const Ability& ability)
{
    if (ability.effectCount == 0 ||
        ability.effectCount > MAX_ABILITY_EFFECTS)
    {
        return false;
    }

    bool hasDamage = false;
    bool hasHealing = false;
    bool hasPositiveValue = false;

    for (uint8_t i = 0; i < ability.effectCount; i++)
    {
        const AbilityEffectData& effect = ability.effects[i];

        if (effect.baseValue < 0 || effect.valuePerLevel < 0 ||
            effect.duration != 0)
            return false;

        if (effect.effect == EFFECT_DAMAGE)
            hasDamage = true;
        else if (effect.effect == EFFECT_HEAL)
            hasHealing = true;
        else
            return false;

        if (effect.baseValue > 0 || effect.valuePerLevel > 0)
            hasPositiveValue = true;
    }

    // Mixed damage/healing abilities need effect-specific recipients, which
    // are intentionally outside this first single-target implementation.
    return hasDamage != hasHealing && hasPositiveValue;
}

const Entity* getResolvedTarget(
    const Entity& caster,
    const Entity* requestedTarget,
    const Ability& ability)
{
    return ability.target == TARGET_SELF
        ? &caster
        : requestedTarget;
}

int getCasterLevel(const Entity& caster)
{
    return caster.character.level > 0
        ? caster.character.level
        : 1;
}

int getEffectAmount(
    const AbilityEffectData& effect,
    const Entity& caster)
{
    return effect.baseValue +
           effect.valuePerLevel * getCasterLevel(caster);
}
}

bool isAbilitySupported(AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    return ability != nullptr &&
           ability->action == ACTION_STANDARD &&
           ability->duration == DURATION_INSTANT &&
           hasSupportedDelivery(*ability) &&
           hasSupportedEffects(*ability);
}

AbilityResult validateAbility(
    const Entity& caster,
    const Entity* target,
    AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isAbilitySupported(abilityID))
        return ABILITY_RESULT_UNSUPPORTED;

    if (!caster.active || !isCombatEntityType(caster.type) ||
        caster.character.state != STATE_ALIVE ||
        !canCharacterAct(caster.character))
    {
        return ABILITY_RESULT_INVALID_CASTER;
    }

    if (caster.turn.standardActionUsed)
        return ABILITY_RESULT_NO_STANDARD_ACTION;

    const Entity* resolvedTarget = getResolvedTarget(
        caster, target, *ability);

    if (resolvedTarget == nullptr || !resolvedTarget->active ||
        !isCombatEntityType(resolvedTarget->type) ||
        resolvedTarget->character.state != STATE_ALIVE)
    {
        return ABILITY_RESULT_INVALID_TARGET;
    }

    if (ability->target == TARGET_ENEMY &&
        (resolvedTarget == &caster ||
         !areOpposingTeams(caster, *resolvedTarget)))
    {
        return ABILITY_RESULT_INVALID_TARGET;
    }

    if (ability->target == TARGET_ENEMY)
    {
        if (getEntityGridDistance(caster, *resolvedTarget) >
            ability->rangeTiles)
        {
            return ABILITY_RESULT_OUT_OF_RANGE;
        }

        if (!hasLineOfSightBetweenFootprintsAt(
                caster, caster.x, caster.y, *resolvedTarget))
        {
            return ABILITY_RESULT_NO_LINE_OF_SIGHT;
        }
    }

    if (caster.character.magic.currentMP < ability->mpCost)
        return ABILITY_RESULT_NOT_ENOUGH_MP;

    return ABILITY_RESULT_SUCCESS;
}

AbilityResolution resolveAbility(
    Entity& caster,
    Entity* target,
    AbilityID abilityID)
{
    AbilityResolution resolution;
    resolution.result = validateAbility(caster, target, abilityID);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return resolution;

    const Ability* ability = getAbility(abilityID);
    Entity* resolvedTarget = ability->target == TARGET_SELF
        ? &caster
        : target;
    int damage = 0;
    int healing = 0;

    // Calculate every supported effect before mutating state. Validation has
    // already rejected unsupported or mixed effect sets.
    for (uint8_t i = 0; i < ability->effectCount; i++)
    {
        int amount = getEffectAmount(ability->effects[i], caster);

        if (ability->effects[i].effect == EFFECT_DAMAGE)
            damage += amount;
        else
            healing += amount;
    }

    if (ability->effects[0].effect == EFFECT_DAMAGE)
    {
        CombatDamageResult damageResult =
            applyCombatDamage(*resolvedTarget, damage);

        if (!damageResult.applied)
        {
            resolution.result = ABILITY_RESULT_INVALID_TARGET;
            return resolution;
        }

        resolution.damage = damage;
        resolution.targetDefeated = damageResult.defeated;
        resolution.levelReached = damageResult.levelReached;
    }
    else
    {
        resolution.healing = healCharacter(
            resolvedTarget->character, healing);
    }

    // Resource and action costs occur only after all validation and effect
    // application have succeeded.
    caster.character.magic.currentMP -= ability->mpCost;
    caster.turn.standardActionUsed = true;

    return resolution;
}

const char* getAbilityResultMessage(AbilityResult result)
{
    switch (result)
    {
        case ABILITY_RESULT_INVALID_ABILITY:
            return "Invalid ability.";

        case ABILITY_RESULT_UNSUPPORTED:
            return "That ability is not supported yet.";

        case ABILITY_RESULT_INVALID_CASTER:
            return "You cannot cast right now.";

        case ABILITY_RESULT_INVALID_TARGET:
            return "Invalid target.";

        case ABILITY_RESULT_OUT_OF_RANGE:
            return "Out of range.";

        case ABILITY_RESULT_NO_LINE_OF_SIGHT:
            return "No line of sight.";

        case ABILITY_RESULT_NOT_ENOUGH_MP:
            return "Not enough MP.";

        case ABILITY_RESULT_NO_STANDARD_ACTION:
            return "Action already used.";

        case ABILITY_RESULT_SUCCESS:
            return "Ability resolved.";
    }

    return "Ability failed.";
}
