#include "abilityresolver.h"

#include "activemap.h"
#include "combat.h"
#include "mapeffects.h"
#include "characters/characters.h"
#include "data/dice.h"
#include "data/entities.h"

namespace
{
enum SupportedEffectKind : uint8_t
{
    SUPPORTED_EFFECT_NONE,
    SUPPORTED_EFFECT_DAMAGE,
    SUPPORTED_EFFECT_HEALING,
    SUPPORTED_EFFECT_CONDITION
};

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

bool hasSupportedMapEffect(const Ability& ability)
{
    if (ability.target != TARGET_AREA ||
        ability.delivery != DELIVERY_AREA ||
        ability.duration != DURATION_ROUNDS ||
        ability.rangeTiles == 0 ||
        ability.areaRadiusTiles == 0 ||
        ability.mapEffectType == MAP_EFFECT_NONE ||
        ability.mapEffectDurationRounds == 0 ||
        ability.effectCount != 1)
    {
        return false;
    }

    const AbilityEffectData& effect = ability.effects[0];

    return effect.effect != EFFECT_NONE &&
           effect.effect != EFFECT_DAMAGE &&
           effect.effect != EFFECT_HEAL &&
           effect.damageType == DAMAGE_NONE &&
           effect.baseValue >= 0 &&
           effect.valuePerLevel >= 0 &&
           effect.duration == 0 &&
           isValidConditionType(effect.conditionType);
}

bool isValidSaveType(SaveType saveType)
{
    return saveType >= SAVE_NONE && saveType <= SAVE_WILL;
}

SupportedEffectKind getSupportedEffectKind(const Ability& ability)
{
    if (ability.effectCount == 0 ||
        ability.effectCount > MAX_ABILITY_EFFECTS)
    {
        return SUPPORTED_EFFECT_NONE;
    }

    SupportedEffectKind kind = SUPPORTED_EFFECT_NONE;
    bool hasPositiveValue = false;

    for (uint8_t i = 0; i < ability.effectCount; i++)
    {
        const AbilityEffectData& effect = ability.effects[i];
        SupportedEffectKind effectKind = SUPPORTED_EFFECT_NONE;

        if (effect.baseValue < 0 || effect.valuePerLevel < 0)
            return SUPPORTED_EFFECT_NONE;

        if (effect.conditionType != CONDITION_NONE)
        {
            if (!isValidConditionType(effect.conditionType) ||
                effect.effect == EFFECT_NONE ||
                effect.effect == EFFECT_DAMAGE ||
                effect.effect == EFFECT_HEAL ||
                effect.damageType != DAMAGE_NONE ||
                effect.duration <= 0)
            {
                return SUPPORTED_EFFECT_NONE;
            }

            effectKind = SUPPORTED_EFFECT_CONDITION;
        }
        else if (effect.effect == EFFECT_DAMAGE && effect.duration == 0)
        {
            effectKind = SUPPORTED_EFFECT_DAMAGE;
        }
        else if (effect.effect == EFFECT_HEAL)
        {
            if (effect.duration != 0)
                return SUPPORTED_EFFECT_NONE;

            effectKind = SUPPORTED_EFFECT_HEALING;
        }
        else
        {
            return SUPPORTED_EFFECT_NONE;
        }

        if (kind == SUPPORTED_EFFECT_NONE)
            kind = effectKind;
        else if (kind != effectKind)
            return SUPPORTED_EFFECT_NONE;

        if (effect.baseValue > 0 || effect.valuePerLevel > 0)
            hasPositiveValue = true;
    }

    // Keep the first condition pass transactional by accepting one condition
    // effect at a time. Damage and healing may still aggregate multiple rows.
    if (kind == SUPPORTED_EFFECT_CONDITION)
    {
        return ability.effectCount == 1
            ? kind
            : SUPPORTED_EFFECT_NONE;
    }

    return hasPositiveValue ? kind : SUPPORTED_EFFECT_NONE;
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

const AbilityEffectData* getConditionEffect(const Ability& ability)
{
    if (getSupportedEffectKind(ability) != SUPPORTED_EFFECT_CONDITION)
        return nullptr;

    return &ability.effects[0];
}

bool isEntityAbilitySupported(const Ability& ability)
{
    if (!hasSupportedDelivery(ability))
        return false;

    SupportedEffectKind effectKind = getSupportedEffectKind(ability);

    if (effectKind == SUPPORTED_EFFECT_CONDITION)
        return ability.duration == DURATION_ROUNDS;

    return effectKind != SUPPORTED_EFFECT_NONE &&
           ability.duration == DURATION_INSTANT;
}

bool isValidCaster(const Entity& caster)
{
    return caster.active && isCombatEntityType(caster.type) &&
           caster.character.state == STATE_ALIVE &&
           canCharacterAct(caster.character);
}
}

bool isAbilitySupported(AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr ||
        ability->action != ACTION_STANDARD ||
        !isValidSaveType(ability->saveType))
    {
        return false;
    }

    return isEntityAbilitySupported(*ability) ||
           hasSupportedMapEffect(*ability);
}

bool isGroundTargetAbility(AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    return ability != nullptr &&
           ability->target == TARGET_AREA &&
           ability->delivery == DELIVERY_AREA &&
           ability->mapEffectType != MAP_EFFECT_NONE;
}

int getAbilitySaveDC(const Entity& caster, const Ability& ability)
{
    int castingModifier = 0;

    switch (ability.type)
    {
        case ABILITY_ARCANE:
            castingModifier = getAbilityModifier(
                caster.character.abilities.intelligence);
            break;

        case ABILITY_DIVINE:
            castingModifier = getAbilityModifier(
                caster.character.abilities.wisdom);
            break;

        case ABILITY_MONSTER:
            castingModifier = getAbilityModifier(
                caster.character.abilities.charisma);
            break;

        case ABILITY_MARTIAL:
            break;
    }

    return 10 + ability.level + castingModifier;
}

int getAbilitySaveBonus(const Character& target, SaveType saveType)
{
    switch (saveType)
    {
        case SAVE_FORTITUDE:
            return getFortitudeSave(target);

        case SAVE_REFLEX:
            return getReflexSave(target);

        case SAVE_WILL:
            return getWillSave(target);

        case SAVE_NONE:
            return 0;
    }

    return 0;
}

AbilitySavingThrow resolveAbilitySavingThrow(
    const Entity& caster,
    const Entity& target,
    const Ability& ability)
{
    return resolveSavingThrow(
        target.character,
        ability.saveType,
        getAbilitySaveDC(caster, ability));
}

AbilitySavingThrow resolveSavingThrow(
    const Character& target,
    SaveType saveType,
    int dc)
{
    AbilitySavingThrow savingThrow;

    if (saveType == SAVE_NONE)
        return savingThrow;

    savingThrow.roll = rollDie(20);
    savingThrow.bonus = getAbilitySaveBonus(target, saveType);
    savingThrow.total = savingThrow.roll + savingThrow.bonus;
    savingThrow.dc = dc;
    savingThrow.result = savingThrow.total >= dc
        ? SAVE_RESULT_SUCCESS
        : SAVE_RESULT_FAILURE;

    return savingThrow;
}

AbilityResult validateAbility(
    const Entity& caster,
    const Entity* target,
    AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isAbilitySupported(abilityID) ||
        !isEntityAbilitySupported(*ability))
        return ABILITY_RESULT_UNSUPPORTED;

    if (!isValidCaster(caster))
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

    const AbilityEffectData* conditionEffect =
        getConditionEffect(*ability);

    if (conditionEffect != nullptr &&
        !canReceiveCondition(
            resolvedTarget->character,
            conditionEffect->conditionType))
    {
        return ABILITY_RESULT_TARGET_IMMUNE;
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
    SupportedEffectKind effectKind = getSupportedEffectKind(*ability);
    resolution.savingThrow = resolveAbilitySavingThrow(
        caster, *resolvedTarget, *ability);

    // A successful save is still a successfully resolved cast. It applies no
    // effect in this pass, but spends MP and the standard action below.
    if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
        effectKind == SUPPORTED_EFFECT_DAMAGE)
    {
        int damage = 0;

        for (uint8_t i = 0; i < ability->effectCount; i++)
            damage += getEffectAmount(ability->effects[i], caster);

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
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_HEALING)
    {
        int healing = 0;

        for (uint8_t i = 0; i < ability->effectCount; i++)
            healing += getEffectAmount(ability->effects[i], caster);

        resolution.healing = healCharacter(
            resolvedTarget->character, healing);
    }
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_CONDITION)
    {
        const AbilityEffectData& effect = ability->effects[0];

        if (!addCondition(
                resolvedTarget->character,
                effect.conditionType,
                getEffectAmount(effect, caster),
                effect.duration))
        {
            resolution.result = ABILITY_RESULT_CONDITION_LIMIT;
            return resolution;
        }

        resolution.conditionApplied = effect.conditionType;
        resolution.conditionDuration = effect.duration;
    }

    // Resource and action costs occur only after all validation and effect
    // application have succeeded.
    caster.character.magic.currentMP -= ability->mpCost;
    caster.turn.standardActionUsed = true;

    return resolution;
}

AbilityResult validateAbilityAt(
    const Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isAbilitySupported(abilityID) ||
        !hasSupportedMapEffect(*ability))
    {
        return ABILITY_RESULT_UNSUPPORTED;
    }

    if (!isValidCaster(caster))
        return ABILITY_RESULT_INVALID_CASTER;

    if (caster.turn.standardActionUsed)
        return ABILITY_RESULT_NO_STANDARD_ACTION;

    if (!isInsideActiveMap(targetX, targetY))
        return ABILITY_RESULT_INVALID_TARGET;

    if (getEntityGridDistanceToTile(caster, targetX, targetY) >
        ability->rangeTiles)
    {
        return ABILITY_RESULT_OUT_OF_RANGE;
    }

    if (!hasLineOfSightFromFootprintAt(
            caster, caster.x, caster.y, targetX, targetY))
    {
        return ABILITY_RESULT_NO_LINE_OF_SIGHT;
    }

    if (!hasMapEffectCapacity())
        return ABILITY_RESULT_MAP_EFFECT_LIMIT;

    if (caster.character.magic.currentMP < ability->mpCost)
        return ABILITY_RESULT_NOT_ENOUGH_MP;

    return ABILITY_RESULT_SUCCESS;
}

AbilityResolution resolveAbilityAt(
    Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID)
{
    AbilityResolution resolution;
    resolution.result = validateAbilityAt(
        caster, targetX, targetY, abilityID);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return resolution;

    const Ability* ability = getAbility(abilityID);
    const AbilityEffectData& effect = ability->effects[0];
    MapEffect mapEffect;
    mapEffect.active = true;
    mapEffect.type = ability->mapEffectType;
    mapEffect.sourceAbility = abilityID;
    mapEffect.x = static_cast<int8_t>(targetX);
    mapEffect.y = static_cast<int8_t>(targetY);
    mapEffect.radius = ability->areaRadiusTiles;
    mapEffect.roundsRemaining = ability->mapEffectDurationRounds;
    mapEffect.saveType = ability->saveType;
    mapEffect.saveDC = getAbilitySaveDC(caster, *ability);
    mapEffect.conditionType = effect.conditionType;
    mapEffect.conditionValue = getEffectAmount(effect, caster);
    mapEffect.conditionDuration = static_cast<uint8_t>(effect.duration);

    MapEffect* createdEffect = addMapEffect(mapEffect);

    if (createdEffect == nullptr)
    {
        resolution.result = ABILITY_RESULT_MAP_EFFECT_LIMIT;
        return resolution;
    }

    resolution.mapEffectCreated = true;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities != nullptr)
    {
        for (uint8_t i = 0; i < entityCount; i++)
        {
            MapEffectTriggerResult trigger = applyMapEffectToEntity(
                *createdEffect, entities[i]);
            resolution.targetsAffected += trigger.conditionsApplied;
            resolution.targetsResisted += trigger.savesSucceeded;
        }
    }

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

        case ABILITY_RESULT_TARGET_IMMUNE:
            return "That target is immune.";

        case ABILITY_RESULT_OUT_OF_RANGE:
            return "Out of range.";

        case ABILITY_RESULT_NO_LINE_OF_SIGHT:
            return "No line of sight.";

        case ABILITY_RESULT_NOT_ENOUGH_MP:
            return "Not enough MP.";

        case ABILITY_RESULT_NO_STANDARD_ACTION:
            return "Action already used.";

        case ABILITY_RESULT_CONDITION_LIMIT:
            return "No more conditions can be applied.";

        case ABILITY_RESULT_MAP_EFFECT_LIMIT:
            return "No room for another map effect.";

        case ABILITY_RESULT_SUCCESS:
            return "Ability resolved.";
    }

    return "Ability failed.";
}
