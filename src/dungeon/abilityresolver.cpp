#include "abilityresolver.h"

#include <algorithm>

#include "map/activemap.h"
#include "combat.h"
#include "map/mapeffects.h"
#include "characters/characters.h"
#include "data/dice.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "graphics/display.h"

namespace
{
enum SupportedEffectKind : uint8_t
{
    SUPPORTED_EFFECT_NONE,
    SUPPORTED_EFFECT_DAMAGE,
    SUPPORTED_EFFECT_HEALING,
    SUPPORTED_EFFECT_CONDITION,
    SUPPORTED_EFFECT_ENERGY_RESISTANCE,
    SUPPORTED_EFFECT_ENERGY_PROTECTION
};

bool isSelectableResistanceType(DamageType type)
{
    return type == DAMAGE_FIRE || type == DAMAGE_COLD ||
           type == DAMAGE_ELECTRIC || type == DAMAGE_ACID;
}

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

static const AbilityEffectData* getCreatureTypeEnergyEffect(
    const Ability& ability)
{
    for (uint8_t i = 0; i < ability.effectCount; i++)
        if (ability.effects[i].creatureTypeEnergy)
            return &ability.effects[i];
    return nullptr;
}

bool hasSupportedDelivery(const Ability& ability)
{
    if (ability.target == TARGET_ENEMY)
    {
        return (ability.delivery == DELIVERY_TARGET ||
                ability.delivery == DELIVERY_RANGED_TOUCH ||
                ability.delivery == DELIVERY_TOUCH) &&
               ability.rangeTiles > 0;
    }

    if (ability.target == TARGET_ALLY)
        return ability.delivery == DELIVERY_TOUCH ||
               ability.delivery == DELIVERY_TARGET ||
               ability.delivery == DELIVERY_AREA;

    if (ability.target == TARGET_SELF)
    {
        return ability.delivery == DELIVERY_AUTOMATIC ||
               ability.delivery == DELIVERY_TOUCH ||
               ability.delivery == DELIVERY_TARGET;
    }

    return false;
}

int getEffectAmount(const AbilityEffectData& effect, const Entity& caster);

ConditionType getGenericBuffCondition(const AbilityEffectData& effect)
{
    if (effect.conditionType != CONDITION_NONE)
        return effect.conditionType;
    switch (effect.effect)
    {
        case EFFECT_BUFF_AC: return CONDITION_BUFF_AC;
        case EFFECT_BUFF_ATTACK: return CONDITION_BUFF_ATTACK;
        case EFFECT_BUFF_SAVE: return CONDITION_BUFF_SAVE;
        default: return CONDITION_NONE;
    }
}

bool isModifierEffect(AbilityEffect effect)
{
    return effect == EFFECT_BUFF_AC || effect == EFFECT_BUFF_ATTACK ||
           effect == EFFECT_BUFF_DAMAGE || effect == EFFECT_BUFF_SPEED ||
           effect == EFFECT_BUFF_STR || effect == EFFECT_BUFF_DEX ||
           effect == EFFECT_BUFF_CON || effect == EFFECT_BUFF_INT ||
           effect == EFFECT_BUFF_WIS || effect == EFFECT_BUFF_CHA ||
           effect == EFFECT_BUFF_SAVE || effect == EFFECT_BUFF_BONUS_ATTACK ||
           effect == EFFECT_DEBUFF_ATTACK || effect == EFFECT_DEBUFF_DAMAGE ||
           effect == EFFECT_DEBUFF_SPEED || effect == EFFECT_DEBUFF_AC ||
           effect == EFFECT_DEBUFF_STR || effect == EFFECT_DEBUFF_DEX ||
           effect == EFFECT_DEBUFF_SAVE;
}

bool isTimedDamageEffect(AbilityEffect effect)
{
    return effect == EFFECT_DAMAGE_OVER_TIME;
}

bool hasTargetedTimedDamageProfile(const Ability& ability)
{
    if (ability.effectCount < 2 || ability.target != TARGET_ENEMY ||
        ability.duration != DURATION_COMBAT)
        return false;

    bool immediate = false;
    bool timed = false;
    for (uint8_t i = 0; i < ability.effectCount; i++)
    {
        const AbilityEffectData& effect = ability.effects[i];
        immediate |= effect.effect == EFFECT_DAMAGE && effect.duration == 0;
        timed |= isTimedDamageEffect(effect.effect) && effect.duration > 0;
    }
    return immediate && timed;
}

void addModifierEffect(ConditionModifiers& modifiers,
                       const AbilityEffectData& effect,
                       const Entity& caster)
{
    const int amount = getEffectAmount(effect, caster);
    switch (effect.effect)
    {
        case EFFECT_BUFF_AC: modifiers.acBonus += amount; break;
        case EFFECT_BUFF_ATTACK: modifiers.attackBonus += amount; break;
        case EFFECT_BUFF_DAMAGE: modifiers.damageBonus += amount; break;
        case EFFECT_BUFF_SPEED: modifiers.speedBonus += amount; break;
        case EFFECT_BUFF_STR: modifiers.strBonus += amount; break;
        case EFFECT_BUFF_DEX: modifiers.dexBonus += amount; break;
        case EFFECT_BUFF_CON: modifiers.conBonus += amount; break;
        case EFFECT_BUFF_INT: modifiers.intBonus += amount; break;
        case EFFECT_BUFF_WIS: modifiers.wisBonus += amount; break;
        case EFFECT_BUFF_CHA: modifiers.chaBonus += amount; break;
        case EFFECT_BUFF_SAVE: modifiers.saveBonus += amount; break;
        case EFFECT_BUFF_BONUS_ATTACK: modifiers.bonusAttacks += amount; break;
        case EFFECT_DEBUFF_ATTACK: modifiers.attackBonus -= amount; break;
        case EFFECT_DEBUFF_DAMAGE: modifiers.damageBonus -= amount; break;
        case EFFECT_DEBUFF_SPEED: modifiers.speedBonus -= amount; break;
        case EFFECT_DEBUFF_AC: modifiers.acBonus -= amount; break;
        case EFFECT_DEBUFF_STR: modifiers.strBonus -= amount; break;
        case EFFECT_DEBUFF_DEX: modifiers.dexBonus -= amount; break;
        case EFFECT_DEBUFF_SAVE: modifiers.saveBonus -= amount; break;
        default: break;
    }
}

ConditionType getModifierCondition(const Ability& ability)
{
    for (uint8_t i = 0; i < ability.effectCount; i++)
    {
        if (isModifierEffect(ability.effects[i].effect))
        {
            const ConditionType type = getGenericBuffCondition(ability.effects[i]);
            if (type != CONDITION_NONE)
                return type;
        }
    }
    return CONDITION_NONE;
}

bool hasSupportedMapEffect(const Ability& ability)
{
    if (ability.target != TARGET_AREA ||
        (ability.delivery != DELIVERY_AREA &&
         ability.delivery != DELIVERY_LINE) ||
        (ability.duration != DURATION_ROUNDS &&
         ability.duration != DURATION_COMBAT) ||
        ability.rangeTiles == 0 ||
        (ability.delivery == DELIVERY_AREA && ability.areaRadiusTiles == 0) ||
        ability.mapEffectType == MAP_EFFECT_NONE ||
        (ability.mapEffectDurationRounds == 0 &&
         ability.duration != DURATION_COMBAT) ||
        ability.effectCount != 1)
    {
        return false;
    }

    const AbilityEffectData& effect = ability.effects[0];

    if (effect.effect == EFFECT_DAMAGE)
        return effect.damageType != DAMAGE_NONE &&
               effect.baseValue >= 0 && effect.valuePerLevel >= 0 &&
               (effect.diceCount == 0 || effect.diceSides > 0);

    return effect.effect != EFFECT_NONE && effect.effect != EFFECT_HEAL &&
           effect.damageType == DAMAGE_NONE && effect.baseValue >= 0 &&
           effect.valuePerLevel >= 0 && effect.duration == 0 &&
           isValidConditionType(effect.conditionType);
}

bool hasSupportedColorSprayProfile(const Ability& ability)
{
    return ability.id == ABILITY_COLOR_SPRAY &&
           ability.target == TARGET_AREA &&
           ability.delivery == DELIVERY_CONE &&
           ability.duration == DURATION_INSTANT &&
           ability.rangeTiles > 0 &&
           ability.saveType == SAVE_WILL &&
           ability.effectCount == 2 &&
           ability.effects[0].effect == EFFECT_STUN &&
           ability.effects[0].conditionType == CONDITION_STUNNED &&
           ability.effects[0].duration > 0 &&
           ability.effects[1].effect == EFFECT_BLIND &&
           ability.effects[1].conditionType == CONDITION_BLINDED &&
           ability.effects[1].duration > 0;
}

bool hasInstantAreaDamageProfile(const Ability& ability)
{
    if (ability.target != TARGET_AREA ||
        ability.duration != DURATION_INSTANT ||
        (ability.delivery != DELIVERY_CONE &&
         ability.delivery != DELIVERY_LINE &&
         ability.delivery != DELIVERY_AREA) ||
        ability.rangeTiles == 0 ||
        ability.effectCount == 0 || ability.effectCount > 2 ||
        ability.effects[0].effect != EFFECT_DAMAGE ||
        ability.effects[0].duration != 0 ||
        ability.effects[0].damageType == DAMAGE_NONE)
    {
        return false;
    }

    if (ability.effectCount == 1)
        return true;

    const AbilityEffectData& secondary = ability.effects[1];
    return secondary.effect != EFFECT_NONE &&
           secondary.effect != EFFECT_DAMAGE &&
           secondary.effect != EFFECT_HEAL &&
           secondary.damageType == DAMAGE_NONE &&
           secondary.duration > 0 &&
           isValidConditionType(secondary.conditionType);
}

const AbilityEffectData* getAreaSecondaryCondition(
    const Ability& ability)
{
    return ability.effectCount == 2 &&
           hasInstantAreaDamageProfile(ability)
        ? &ability.effects[1]
        : nullptr;
}

void applyAreaSecondaryCondition(
    const Ability& ability,
    Entity& caster,
    Entity& target,
    const AbilitySavingThrow& save,
    AbilityResolution& resolution)
{
    const AbilityEffectData* secondary = getAreaSecondaryCondition(ability);
    if (secondary == nullptr || save.result == SAVE_RESULT_SUCCESS ||
        target.character.state != STATE_ALIVE ||
        !canReceiveCondition(target.character, secondary->conditionType))
    {
        return;
    }

    if (addCondition(target.character, secondary->conditionType,
                     getEffectAmount(*secondary, caster), secondary->duration))
    {
        resolution.conditionApplied = secondary->conditionType;
        resolution.conditionDuration = secondary->duration;
    }
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

        if (effect.effect == EFFECT_DAMAGE_RESISTANCE)
        {
            if (effect.damageType != DAMAGE_NONE || effect.duration <= 0)
                return SUPPORTED_EFFECT_NONE;
            effectKind = SUPPORTED_EFFECT_ENERGY_RESISTANCE;
        }
        else if (effect.effect == EFFECT_ENERGY_PROTECTION)
        {
            if (effect.damageType != DAMAGE_NONE || effect.duration <= 0)
                return SUPPORTED_EFFECT_NONE;
            effectKind = SUPPORTED_EFFECT_ENERGY_PROTECTION;
        }
        else if (isModifierEffect(effect.effect) &&
            getGenericBuffCondition(effect) != CONDITION_NONE)
        {
            if (!isValidConditionType(getGenericBuffCondition(effect)) ||
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

int getEffectDamage(const AbilityEffectData& effect, const Entity& caster)
{
    return getEffectAmount(effect, caster) +
        (effect.diceCount > 0 && effect.diceSides > 0
            ? rollDice(effect.diceCount, effect.diceSides) : 0);
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

    if (hasTargetedTimedDamageProfile(ability))
        return true;

    SupportedEffectKind effectKind = getSupportedEffectKind(ability);

    if (effectKind == SUPPORTED_EFFECT_CONDITION ||
        effectKind == SUPPORTED_EFFECT_ENERGY_RESISTANCE ||
        effectKind == SUPPORTED_EFFECT_ENERGY_PROTECTION)
        return ability.duration == DURATION_ROUNDS || ability.duration == DURATION_COMBAT;

    return effectKind != SUPPORTED_EFFECT_NONE &&
           ability.duration == DURATION_INSTANT;
}

bool isValidCaster(const Entity& caster)
{
    return caster.active && isCombatEntityType(caster.type) &&
           caster.character.state == STATE_ALIVE &&
           canCharacterAct(caster.character);
}

bool isValidDirection(Direction direction)
{
    int rawDirection = static_cast<int>(direction);
    return rawDirection >= static_cast<int>(DIR_NORTH) &&
           rawDirection <= static_cast<int>(DIR_NORTHWEST);
}


struct ColorSprayDurations
{
    uint8_t stunned;
    uint8_t blinded;
};

ColorSprayDurations getColorSprayDurations(
    const Entity& target,
    const Ability& ability)
{
    uint8_t hitDice = getEffectiveHitDice(target);
    uint8_t strongestStun = static_cast<uint8_t>(
        ability.effects[0].duration);
    uint8_t strongestBlind = static_cast<uint8_t>(
        ability.effects[1].duration);

    if (hitDice <= 2)
        return { strongestStun, strongestBlind };

    if (hitDice <= 4)
    {
        uint8_t shorterBlind = strongestBlind > 1
            ? strongestBlind / 2
            : 1;
        return { 1, shorterBlind };
    }

    return { 1, 0 };
}

bool isConeOffset(
    int relativeX,
    int relativeY,
    const DirectionOffset& direction,
    uint8_t length)
{
    int forward = relativeX * direction.dx +
                  relativeY * direction.dy;

    if (forward <= 0)
        return false;

    int depth = (direction.dx == 0 || direction.dy == 0)
        ? forward
        : std::max(abs(relativeX), abs(relativeY));
    int lateral = abs(
        relativeX * direction.dy -
        relativeY * direction.dx);

    return depth >= 1 && depth <= length && lateral < depth;
}

bool entityIsInDirectionalArea(
    const Entity& caster,
    const Entity& target,
    const Ability& ability,
    Direction direction)
{
    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(target);
         offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(target);
             offsetX++)
        {
            if (isTileInDirectionalAbilityArea(
                    caster,
                    ability.id,
                    direction,
                    target.x + offsetX,
                    target.y + offsetY))
            {
                return true;
            }
        }
    }

    return false;
}

uint8_t collectDirectionalAreaTiles(
    const Entity& caster, const Ability& ability, Direction direction,
    AreaFlashTile tiles[], uint8_t capacity)
{
    uint8_t count = 0;
    for (int y = 0; y < getActiveMapHeight(); y++)
        for (int x = 0; x < getActiveMapWidth(); x++)
            if (count < capacity && isTileInDirectionalAbilityArea(
                    caster, ability.id, direction, x, y))
                tiles[count++] = { static_cast<int8_t>(x), static_cast<int8_t>(y) };
    return count;
}

uint8_t collectRadiusAreaTiles(
    const Entity& caster, const Ability& ability, int centerX, int centerY,
    AreaFlashTile tiles[], uint8_t capacity)
{
    uint8_t count = 0;
    for (int y = centerY - ability.areaRadiusTiles; y <= centerY + ability.areaRadiusTiles; y++)
        for (int x = centerX - ability.areaRadiusTiles; x <= centerX + ability.areaRadiusTiles; x++)
            if (count < capacity && isInsideActiveMap(x, y) &&
                hasLineOfSightFromFootprintAt(caster, caster.x, caster.y, x, y))
                tiles[count++] = { static_cast<int8_t>(x), static_cast<int8_t>(y) };
    return count;
}
}

EnergyInteraction getEnergyInteraction(
    DamageType energyType,
    CreatureType creatureType)
{
    const bool undead = isUndeadCreatureType(creatureType);
    if (energyType == DAMAGE_POSITIVE)
        return undead ? EnergyInteraction::DAMAGE : EnergyInteraction::HEAL;
    if (energyType == DAMAGE_NEGATIVE)
        return undead ? EnergyInteraction::HEAL : EnergyInteraction::DAMAGE;
    return EnergyInteraction::NONE;
}

bool isAbilityEffectHostileToTarget(
    const Ability& ability,
    const Character& target)
{
    const AbilityEffectData* energy = getCreatureTypeEnergyEffect(ability);
    if (energy != nullptr)
        return getEnergyInteraction(
                   energy->damageType, target.creatureType) ==
               EnergyInteraction::DAMAGE;
    return ability.target == TARGET_ENEMY;
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

    if (abilityID == ABILITY_TURN_UNDEAD)
        return true;

    // Physical damage reduction remains deferred. Resist Energy and
    // Protection from Energy each have explicit Arcane/Divine rows so scroll
    // compatibility remains data-driven.
    if (getSupportedEffectKind(*ability) ==
            SUPPORTED_EFFECT_ENERGY_RESISTANCE &&
        abilityID != ABILITY_RESIST_ENERGY &&
        abilityID != ABILITY_RESIST_ENERGY_ARCANE)
    {
        return false;
    }

    if (getSupportedEffectKind(*ability) ==
            SUPPORTED_EFFECT_ENERGY_PROTECTION &&
        abilityID != ABILITY_PROTECTION_FROM_ENERGY &&
        abilityID != ABILITY_PROTECTION_FROM_ENERGY_ARCANE)
    {
        return false;
    }

    return isEntityAbilitySupported(*ability) ||
           hasSupportedMapEffect(*ability) ||
           hasSupportedColorSprayProfile(*ability) ||
             hasInstantAreaDamageProfile(*ability);
}

bool isGroundTargetAbility(AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    return ability != nullptr &&
           ability->target == TARGET_AREA &&
           ability->delivery == DELIVERY_AREA &&
            (ability->mapEffectType != MAP_EFFECT_NONE ||
             hasInstantAreaDamageProfile(*ability));
}

bool isDirectionalAbility(AbilityID abilityID)
{
    const Ability* ability = getAbility(abilityID);

    return ability != nullptr && isAbilitySupported(abilityID) &&
            ability->target == TARGET_AREA &&
            (ability->delivery == DELIVERY_CONE || ability->delivery == DELIVERY_LINE);
}

bool isTileInDirectionalAbilityArea(
    const Entity& caster,
    AbilityID abilityID,
    Direction direction,
    int tileX,
    int tileY)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr ||
        (ability->delivery != DELIVERY_CONE && ability->delivery != DELIVERY_LINE) ||
        !isValidDirection(direction) ||
        ability->rangeTiles == 0 || !isInsideActiveMap(tileX, tileY) ||
        entityOccupiesTile(caster, tileX, tileY))
    {
        return false;
    }

    TileType tile = getActiveMapTile(tileX, tileY);
    if (tile == TILE_WALL || tile == TILE_TREE ||
        !hasLineOfSightFromFootprintAt(
            caster, caster.x, caster.y, tileX, tileY))
    {
        return false;
    }

    const DirectionOffset& offset = directionOffsets[direction];

    for (uint8_t originY = 0;
         originY < getEntityTileHeight(caster);
         originY++)
    {
        for (uint8_t originX = 0;
             originX < getEntityTileWidth(caster);
             originX++)
        {
            const int relativeX = tileX - (caster.x + originX);
            const int relativeY = tileY - (caster.y + originY);
            if ((ability->delivery == DELIVERY_CONE && isConeOffset(
                    relativeX, relativeY, offset, ability->rangeTiles)) ||
                (ability->delivery == DELIVERY_LINE &&
                 relativeX * offset.dy == relativeY * offset.dx &&
                 relativeX * offset.dx + relativeY * offset.dy > 0 &&
                 relativeX * offset.dx + relativeY * offset.dy <= ability->rangeTiles))
            {
                return true;
            }
        }
    }

    return false;
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
            return getFortitudeSave(target) + getConditionSaveModifier(target);

        case SAVE_REFLEX:
            return getReflexSave(target) + getConditionSaveModifier(target);

        case SAVE_WILL:
            return getWillSave(target) + getConditionSaveModifier(target);

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

bool canPayAbilityCost(
    const Character& caster,
    const Ability& ability,
    AbilityCastSource source)
{
    return source == AbilityCastSource::SCROLL ||
           caster.magic.currentMP >= ability.mpCost;
}

void payAbilityCost(
    Character& caster,
    const Ability& ability,
    AbilityCastSource source)
{
    if (source == AbilityCastSource::NORMAL)
        caster.magic.currentMP -= ability.mpCost;
}

AbilityResult validateAbility(
    const Entity& caster,
    const Entity* target,
    AbilityID abilityID,
    AbilityCastSource source,
    DamageType selectedDamageType)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isAbilitySupported(abilityID) ||
        !isEntityAbilitySupported(*ability))
        return ABILITY_RESULT_UNSUPPORTED;

    const SupportedEffectKind effectKind = getSupportedEffectKind(*ability);
    if ((effectKind == SUPPORTED_EFFECT_ENERGY_RESISTANCE ||
         effectKind == SUPPORTED_EFFECT_ENERGY_PROTECTION) &&
        !isSelectableResistanceType(selectedDamageType))
    {
        return ABILITY_RESULT_INVALID_TARGET;
    }

    if (!isValidCaster(caster))
    {
        return ABILITY_RESULT_INVALID_CASTER;
    }

    if (combat.active && caster.turn.standardActionUsed)
        return ABILITY_RESULT_NO_STANDARD_ACTION;

    const Entity* resolvedTarget = getResolvedTarget(
        caster, target, *ability);

    const bool canRecoverUnconsciousTarget =
        resolvedTarget != nullptr &&
        resolvedTarget->character.state == STATE_UNCONSCIOUS &&
        getSupportedEffectKind(*ability) == SUPPORTED_EFFECT_HEALING;

    if (resolvedTarget == nullptr || !resolvedTarget->active ||
        !isCombatEntityType(resolvedTarget->type) ||
        (resolvedTarget->character.state != STATE_ALIVE &&
         !canRecoverUnconsciousTarget))
    {
        return ABILITY_RESULT_INVALID_TARGET;
    }

    const bool hostileToTarget =
        isAbilityEffectHostileToTarget(*ability, resolvedTarget->character);
    if ((hostileToTarget &&
         (resolvedTarget == &caster ||
          !areOpposingTeams(caster, *resolvedTarget))) ||
        (!hostileToTarget &&
         resolvedTarget->character.team != caster.character.team))
    {
        return ABILITY_RESULT_INVALID_TARGET;
    }

    if (hostileToTarget)
    {
        if (!canSee(caster))
            return ABILITY_RESULT_NO_LINE_OF_SIGHT;

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

    if (ability->delivery == DELIVERY_TOUCH &&
        resolvedTarget != &caster)
    {
        if (getEntityGridDistance(caster, *resolvedTarget) > 1)
            return ABILITY_RESULT_OUT_OF_RANGE;
        if (!hasLineOfSightBetweenFootprintsAt(
                caster, caster.x, caster.y, *resolvedTarget))
            return ABILITY_RESULT_NO_LINE_OF_SIGHT;
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

    if (!canPayAbilityCost(caster.character, *ability, source))
        return ABILITY_RESULT_NOT_ENOUGH_MP;

    return ABILITY_RESULT_SUCCESS;
}

AbilityResolution resolveAbility(
    Entity& caster,
    Entity* target,
    AbilityID abilityID,
    AbilityCastSource source,
    DamageType selectedDamageType)
{
    AbilityResolution resolution;
    resolution.result = validateAbility(
        caster, target, abilityID, source, selectedDamageType);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return resolution;

    const Ability* ability = getAbility(abilityID);
    Entity* resolvedTarget = ability->target == TARGET_SELF
        ? &caster
        : target;
    SupportedEffectKind effectKind = getSupportedEffectKind(*ability);
    const AbilityEffectData* creatureEnergy =
        getCreatureTypeEnergyEffect(*ability);
    const EnergyInteraction energyInteraction = creatureEnergy != nullptr
        ? getEnergyInteraction(creatureEnergy->damageType,
                               resolvedTarget->character.creatureType)
        : EnergyInteraction::NONE;
    const bool hostileToTarget =
        isAbilityEffectHostileToTarget(*ability, resolvedTarget->character);

    const bool requiresRangedTouchAttack =
        ability->delivery == DELIVERY_RANGED_TOUCH;
    const bool requiresMeleeTouchAttack =
        ability->delivery == DELIVERY_TOUCH &&
        hostileToTarget;

    if (requiresRangedTouchAttack || requiresMeleeTouchAttack)
    {
        resolution.attackRoll.required = true;
        resolution.attackRoll.roll = rollDie(20);
        resolution.attackRoll.bonus = requiresRangedTouchAttack
            ? getRangedTouchAttackBonus(caster.character)
            : getMeleeTouchAttackBonus(caster.character);
        resolution.attackRoll.total = resolution.attackRoll.roll +
                                      resolution.attackRoll.bonus;
        resolution.attackRoll.targetAC =
            getTouchArmorClass(resolvedTarget->character);
        resolution.attackRoll.hit = resolution.attackRoll.roll == 20 ||
            (resolution.attackRoll.roll != 1 &&
             resolution.attackRoll.total >= resolution.attackRoll.targetAC);

        if (!resolution.attackRoll.hit)
        {
            // Confirmation has committed the spell even though the attack
            // missed. Pay its resource/action cost, but return before saves,
            // damage, conditions, or recurring effects can be applied.
            payAbilityCost(caster.character, *ability, source);
            if (combat.active)
                caster.turn.standardActionUsed = true;
            return resolution;
        }
    }

    if (hostileToTarget)
        resolution.savingThrow = resolveAbilitySavingThrow(
            caster, *resolvedTarget, *ability);

    if (energyInteraction != EnergyInteraction::NONE)
    {
        int amount = 0;
        for (uint8_t i = 0; i < ability->effectCount; i++)
            amount += getEffectDamage(ability->effects[i], caster);

        if (energyInteraction == EnergyInteraction::DAMAGE)
        {
            if (resolution.savingThrow.result == SAVE_RESULT_SUCCESS)
            {
                if (ability->saveEffect == SAVE_EFFECT_NEGATES)
                    amount = 0;
                else if (ability->saveEffect == SAVE_EFFECT_HALF)
                    amount /= 2;
            }

            if (amount > 0)
            {
                CombatDamageResult damageResult = applyCombatDamage(
                    *resolvedTarget, amount, creatureEnergy->damageType);
                if (!damageResult.applied)
                {
                    resolution.result = ABILITY_RESULT_INVALID_TARGET;
                    return resolution;
                }
                resolution.damage = amount;
                resolution.targetDefeated = damageResult.defeated;
                resolution.levelReached = damageResult.levelReached;
                playAbilityImpactFlash(
                    IMPACT_DAMAGE, creatureEnergy->damageType,
                    resolvedTarget->x, resolvedTarget->y);
            }
        }
        else
        {
            resolution.healing = healCharacter(
                resolvedTarget->character, amount);
            if (resolution.healing > 0)
                playAbilityImpactFlash(
                    IMPACT_HEAL, creatureEnergy->damageType,
                    resolvedTarget->x, resolvedTarget->y);
        }

        payAbilityCost(caster.character, *ability, source);
        if (combat.active)
            caster.turn.standardActionUsed = true;
        return resolution;
    }

    if (hasTargetedTimedDamageProfile(*ability))
    {
        for (uint8_t i = 0; i < ability->effectCount; i++)
        {
            const AbilityEffectData& effect = ability->effects[i];
            if (effect.effect == EFFECT_DAMAGE)
            {
                const int damage = getEffectDamage(effect, caster);
                CombatDamageResult damageResult = applyCombatDamage(
                    *resolvedTarget, damage, effect.damageType);
                if (!damageResult.applied)
                {
                    resolution.result = ABILITY_RESULT_INVALID_TARGET;
                    return resolution;
                }
                resolution.damage += damage;
                resolution.targetDefeated = damageResult.defeated;
                resolution.levelReached = damageResult.levelReached;
                playAbilityImpactFlash(IMPACT_DAMAGE, effect.damageType,
                                       resolvedTarget->x, resolvedTarget->y);
            }
            else if (isTimedDamageEffect(effect.effect))
            {
                TimedDamageEffect timed;
                timed.damageType = static_cast<uint8_t>(effect.damageType);
                timed.diceCount = effect.diceCount;
                timed.diceSides = effect.diceSides;
                timed.roundsRemaining = static_cast<uint8_t>(effect.duration);
                timed.sourceAbility = static_cast<uint16_t>(ability->id);
                if (!addTimedDamageEffect(resolvedTarget->character, timed))
                {
                    resolution.result = ABILITY_RESULT_CONDITION_LIMIT;
                    return resolution;
                }
            }
        }
        payAbilityCost(caster.character, *ability, source);
        if (combat.active)
            caster.turn.standardActionUsed = true;
        return resolution;
    }

    // A successful save is still a successfully resolved cast. It applies no
    // effect in this pass, but spends MP and the standard action below.
    if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
        effectKind == SUPPORTED_EFFECT_DAMAGE)
    {
        int damage = 0;

        for (uint8_t i = 0; i < ability->effectCount; i++)
            damage += getEffectDamage(ability->effects[i], caster);

        CombatDamageResult damageResult =
            applyCombatDamage(*resolvedTarget, damage,
                              ability->effects[0].damageType);

        if (!damageResult.applied)
        {
            resolution.result = ABILITY_RESULT_INVALID_TARGET;
            return resolution;
        }

        resolution.damage = damage;
        resolution.targetDefeated = damageResult.defeated;
        resolution.levelReached = damageResult.levelReached;
        playAbilityImpactFlash(IMPACT_DAMAGE, ability->effects[0].damageType,
                               resolvedTarget->x, resolvedTarget->y);
    }
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_HEALING)
    {
        int healing = 0;

        for (uint8_t i = 0; i < ability->effectCount; i++)
            healing += getEffectAmount(ability->effects[i], caster);

        resolution.healing = healCharacter(
            resolvedTarget->character, healing);
        if (resolution.healing > 0)
            playAbilityImpactFlash(IMPACT_HEAL, DAMAGE_NONE,
                                   resolvedTarget->x, resolvedTarget->y);
    }
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_CONDITION)
    {
        const ConditionType conditionType = getModifierCondition(*ability);
        ConditionModifiers modifiers;
        int duration = 0;

        for (uint8_t i = 0; i < ability->effectCount; i++)
        {
            const AbilityEffectData& modifierEffect = ability->effects[i];
            addModifierEffect(modifiers, modifierEffect, caster);
            if (modifierEffect.duration > duration)
                duration = modifierEffect.duration;
        }

        if (!addCondition(
                resolvedTarget->character,
                conditionType,
                modifiers,
                duration))
        {
            resolution.result = ABILITY_RESULT_CONDITION_LIMIT;
            return resolution;
        }

        resolution.conditionApplied = conditionType;
        resolution.conditionDuration = duration;
        playAbilityImpactFlash(IMPACT_BUFF, DAMAGE_NONE,
                               resolvedTarget->x, resolvedTarget->y);
    }
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_ENERGY_RESISTANCE)
    {
        const AbilityEffectData& effect = ability->effects[0];
        const int amount = getEnergyResistanceAmountForCasterLevel(
            caster.character.level);
        if (!addEnergyResistance(
                resolvedTarget->character,
                static_cast<uint8_t>(selectedDamageType),
                amount,
                effect.duration))
        {
            resolution.result = ABILITY_RESULT_CONDITION_LIMIT;
            return resolution;
        }

        resolution.resistanceType = selectedDamageType;
        resolution.resistanceAmount = amount;
        resolution.conditionDuration = effect.duration;
        playAbilityImpactFlash(IMPACT_BUFF, DAMAGE_NONE,
                               resolvedTarget->x, resolvedTarget->y);
    }
    else if (resolution.savingThrow.result != SAVE_RESULT_SUCCESS &&
             effectKind == SUPPORTED_EFFECT_ENERGY_PROTECTION)
    {
        const AbilityEffectData& effect = ability->effects[0];
        const int amount = getEnergyProtectionAmountForCasterLevel(
            caster.character.level);
        if (!addEnergyProtection(
                resolvedTarget->character,
                static_cast<uint8_t>(selectedDamageType),
                amount,
                effect.duration))
        {
            resolution.result = ABILITY_RESULT_CONDITION_LIMIT;
            return resolution;
        }

        resolution.protectionType = selectedDamageType;
        resolution.protectionAmount = amount;
        resolution.conditionDuration = effect.duration;
        playAbilityImpactFlash(IMPACT_BUFF, selectedDamageType,
                               resolvedTarget->x, resolvedTarget->y);
    }

    // Resource and action costs occur only after all validation and effect
    // application have succeeded.
    payAbilityCost(caster.character, *ability, source);
    if (combat.active)
        caster.turn.standardActionUsed = true;

    return resolution;
}

int getEnergyResistanceAmountForCasterLevel(int casterLevel)
{
    if (casterLevel >= 11)
        return 30;
    if (casterLevel >= 7)
        return 20;
    return 10;
}

int getEnergyProtectionAmountForCasterLevel(int casterLevel)
{
    if (casterLevel < 1)
        casterLevel = 1;
    const int amount = casterLevel * 12;
    return amount > 120 ? 120 : amount;
}

AbilityResult validateAbilityAt(
    const Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID,
    AbilityCastSource source)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isAbilitySupported(abilityID) ||
        (!hasSupportedMapEffect(*ability) && !hasInstantAreaDamageProfile(*ability)))
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

    if (hasSupportedMapEffect(*ability) && !hasMapEffectCapacity())
        return ABILITY_RESULT_MAP_EFFECT_LIMIT;

    if (!canPayAbilityCost(caster.character, *ability, source))
        return ABILITY_RESULT_NOT_ENOUGH_MP;

    return ABILITY_RESULT_SUCCESS;
}

AbilityResolution resolveAbilityAt(
    Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID,
    AbilityCastSource source)
{
    AbilityResolution resolution;
    resolution.result = validateAbilityAt(
        caster, targetX, targetY, abilityID, source);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return resolution;

    const Ability* ability = getAbility(abilityID);
    if (hasInstantAreaDamageProfile(*ability))
    {
        AreaFlashTile flashTiles[ROOM_SIZE * ROOM_SIZE];
        const uint8_t flashTileCount = collectRadiusAreaTiles(
            caster, *ability, targetX, targetY, flashTiles,
            sizeof(flashTiles) / sizeof(flashTiles[0]));
        playAreaDamageFlash(ability->effects[0].damageType,
                            flashTiles, flashTileCount);
        uint8_t entityCount = 0;
        Entity* entities = getActiveMapEntities(entityCount);
        const int fullDamage = getEffectAmount(ability->effects[0], caster);
        for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
        {
            Entity& target = entities[i];
            if (!target.active || !isCombatEntityType(target.type) ||
                target.character.state != STATE_ALIVE ||
                getEntityGridDistanceToTile(target, targetX, targetY) > ability->areaRadiusTiles ||
                !hasLineOfSightFromFootprintAt(caster, caster.x, caster.y, target.x, target.y))
                continue;
            AbilitySavingThrow save = resolveAbilitySavingThrow(caster, target, *ability);
            resolution.savingThrow = save;
            int damage = fullDamage;
            if (save.result == SAVE_RESULT_SUCCESS)
            {
                resolution.targetsResisted++;
                if (ability->saveEffect == SAVE_EFFECT_NEGATES) continue;
                if (ability->saveEffect == SAVE_EFFECT_HALF) damage /= 2;
            }
            CombatDamageResult result = applyCombatDamage(
                target, damage, ability->effects[0].damageType);
            if (result.applied)
            {
                resolution.damage += damage;
                resolution.targetsAffected++;
            }
            applyAreaSecondaryCondition(*ability, caster, target, save, resolution);
        }
        payAbilityCost(caster.character, *ability, source);
        if (combat.active)
            caster.turn.standardActionUsed = true;
        return resolution;
    }
    const AbilityEffectData& effect = ability->effects[0];
    MapEffect mapEffect;
    mapEffect.active = true;
    mapEffect.type = ability->mapEffectType;
    mapEffect.sourceAbility = abilityID;
    mapEffect.x = static_cast<int8_t>(targetX);
    mapEffect.y = static_cast<int8_t>(targetY);
    mapEffect.radius = ability->areaRadiusTiles;
    mapEffect.roundsRemaining = ability->mapEffectDurationRounds;
    mapEffect.expiresWithCombat = ability->duration == DURATION_COMBAT;
    mapEffect.saveType = ability->saveType;
    mapEffect.saveDC = getAbilitySaveDC(caster, *ability);
    mapEffect.conditionType = effect.conditionType;
    mapEffect.conditionValue = getEffectAmount(effect, caster);
    mapEffect.conditionDuration = static_cast<uint8_t>(effect.duration);
    mapEffect.damageType = effect.effect == EFFECT_DAMAGE
        ? effect.damageType : DAMAGE_NONE;
    mapEffect.damageDiceCount = effect.diceCount;
    mapEffect.damageDiceSides = effect.diceSides;
    mapEffect.flatDamage = static_cast<int16_t>(getEffectAmount(effect, caster));
    mapEffect.damageSaveEffect = ability->saveEffect;

    AreaFlashTile effectTiles[MAX_MAP_EFFECT_TILES];
    mapEffect.tileCount = collectRadiusAreaTiles(
        caster, *ability, targetX, targetY, effectTiles,
        MAX_MAP_EFFECT_TILES);
    for (uint8_t i = 0; i < mapEffect.tileCount; i++)
    {
        mapEffect.tiles[i].x = effectTiles[i].x;
        mapEffect.tiles[i].y = effectTiles[i].y;
    }

    MapEffect* createdEffect = addMapEffect(mapEffect);

    if (createdEffect == nullptr)
    {
        resolution.result = ABILITY_RESULT_MAP_EFFECT_LIMIT;
        return resolution;
    }

    playAbilityImpactFlash(
        effect.effect == EFFECT_DAMAGE ? IMPACT_DAMAGE : IMPACT_CONDITION,
        mapEffect.damageType, targetX, targetY);

    resolution.mapEffectCreated = true;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities != nullptr && effect.effect != EFFECT_DAMAGE)
    {
        for (uint8_t i = 0; i < entityCount; i++)
        {
            MapEffectTriggerResult trigger = applyMapEffectToEntity(
                *createdEffect, entities[i]);
            resolution.targetsAffected += trigger.conditionsApplied;
            resolution.targetsResisted += trigger.savesSucceeded;
        }
    }

    payAbilityCost(caster.character, *ability, source);
    if (combat.active)
        caster.turn.standardActionUsed = true;
    return resolution;
}

AbilityResult validateDirectionalAbility(
    const Entity& caster,
    AbilityID abilityID,
    AbilityCastSource source)
{
    const Ability* ability = getAbility(abilityID);

    if (ability == nullptr)
        return ABILITY_RESULT_INVALID_ABILITY;

    if (!isDirectionalAbility(abilityID) ||
         (!hasSupportedColorSprayProfile(*ability) &&
          !hasInstantAreaDamageProfile(*ability) &&
          !hasSupportedMapEffect(*ability)))
    {
        return ABILITY_RESULT_UNSUPPORTED;
    }

    if (!isValidCaster(caster))
        return ABILITY_RESULT_INVALID_CASTER;

    if (!isInsideActiveMap(caster.x, caster.y))
        return ABILITY_RESULT_INVALID_TARGET;

    if (caster.turn.standardActionUsed)
        return ABILITY_RESULT_NO_STANDARD_ACTION;

    if (!canPayAbilityCost(caster.character, *ability, source))
        return ABILITY_RESULT_NOT_ENOUGH_MP;

    return ABILITY_RESULT_SUCCESS;
}

AbilityResolution resolveAbilityInDirection(
    Entity& caster,
    Direction direction,
    AbilityID abilityID,
    AbilityCastSource source)
{
    AbilityResolution resolution;

    if (!isValidDirection(direction))
    {
        resolution.result = ABILITY_RESULT_INVALID_TARGET;
        return resolution;
    }

    resolution.result = validateDirectionalAbility(caster, abilityID, source);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return resolution;

    const Ability* ability = getAbility(abilityID);
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (hasInstantAreaDamageProfile(*ability))
    {
        AreaFlashTile flashTiles[ROOM_SIZE * ROOM_SIZE];
        const uint8_t flashTileCount = collectDirectionalAreaTiles(
            caster, *ability, direction, flashTiles,
            sizeof(flashTiles) / sizeof(flashTiles[0]));
        playAreaDamageFlash(ability->effects[0].damageType,
                            flashTiles, flashTileCount);
        const int fullDamage = getEffectAmount(ability->effects[0], caster);
        for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
        {
            Entity& target = entities[i];
            if (!target.active || !isCombatEntityType(target.type) ||
                target.character.state != STATE_ALIVE ||
                !entityIsInDirectionalArea(caster, target, *ability, direction))
                continue;

            AbilitySavingThrow save = resolveAbilitySavingThrow(caster, target, *ability);
            resolution.savingThrow = save;
            int damage = fullDamage;
            if (save.result == SAVE_RESULT_SUCCESS)
            {
                resolution.targetsResisted++;
                if (ability->saveEffect == SAVE_EFFECT_NEGATES)
                    continue;
                if (ability->saveEffect == SAVE_EFFECT_HALF)
                    damage /= 2;
            }
            CombatDamageResult result = applyCombatDamage(
                target, damage, ability->effects[0].damageType);
            if (result.applied)
            {
                resolution.damage += damage;
                resolution.targetsAffected++;
            }
            applyAreaSecondaryCondition(*ability, caster, target, save, resolution);
        }
        payAbilityCost(caster.character, *ability, source);
        if (combat.active)
            caster.turn.standardActionUsed = true;
        return resolution;
    }

    if (hasSupportedMapEffect(*ability))
    {
        AreaFlashTile effectTiles[MAX_MAP_EFFECT_TILES];
        const uint8_t tileCount = collectDirectionalAreaTiles(
            caster, *ability, direction, effectTiles, MAX_MAP_EFFECT_TILES);
        if (tileCount == 0)
        {
            resolution.result = ABILITY_RESULT_INVALID_TARGET;
            return resolution;
        }

        const AbilityEffectData& effect = ability->effects[0];
        MapEffect mapEffect;
        mapEffect.active = true;
        mapEffect.type = ability->mapEffectType;
        mapEffect.sourceAbility = abilityID;
        mapEffect.x = effectTiles[0].x;
        mapEffect.y = effectTiles[0].y;
        mapEffect.roundsRemaining = ability->mapEffectDurationRounds;
        mapEffect.expiresWithCombat = ability->duration == DURATION_COMBAT;
        mapEffect.saveType = ability->saveType;
        mapEffect.saveDC = getAbilitySaveDC(caster, *ability);
        mapEffect.damageType = effect.damageType;
        mapEffect.damageDiceCount = effect.diceCount;
        mapEffect.damageDiceSides = effect.diceSides;
        mapEffect.flatDamage = static_cast<int16_t>(getEffectAmount(effect, caster));
        mapEffect.damageSaveEffect = ability->saveEffect;
        mapEffect.tileCount = tileCount;
        for (uint8_t i = 0; i < tileCount; i++)
        {
            mapEffect.tiles[i].x = effectTiles[i].x;
            mapEffect.tiles[i].y = effectTiles[i].y;
        }

        if (addMapEffect(mapEffect) == nullptr)
        {
            resolution.result = ABILITY_RESULT_MAP_EFFECT_LIMIT;
            return resolution;
        }
        playAreaDamageFlash(effect.damageType, effectTiles, tileCount);
        resolution.mapEffectCreated = true;
        payAbilityCost(caster.character, *ability, source);
        if (combat.active) caster.turn.standardActionUsed = true;
        return resolution;
    }

    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
    {
        Entity& target = entities[i];

        if (&target == &caster || !target.active ||
            !isCombatEntityType(target.type) ||
            target.character.state != STATE_ALIVE ||
            !areOpposingTeams(caster, target) ||
            !entityIsInDirectionalArea(
                caster, target, *ability, direction))
        {
            continue;
        }

        // Color Spray is visual. Innately sightless and currently blinded
        // targets never roll because there is no visual stimulus to resist.
        if (!canSee(target))
        {
            resolution.targetsImmune++;
            continue;
        }

        AbilitySavingThrow savingThrow = resolveAbilitySavingThrow(
            caster, target, *ability);
        resolution.savingThrow = savingThrow;

        if (savingThrow.result == SAVE_RESULT_SUCCESS)
        {
            resolution.targetsResisted++;
            continue;
        }

        ColorSprayDurations durations = getColorSprayDurations(
            target, *ability);
        bool conditionApplied = false;

        if (durations.stunned > 0 && addCondition(
                target.character,
                CONDITION_STUNNED,
                0,
                durations.stunned))
        {
            conditionApplied = true;
            resolution.conditionApplied = CONDITION_STUNNED;
            resolution.conditionDuration = durations.stunned;
        }

        if (durations.blinded > 0 && addCondition(
                target.character,
                CONDITION_BLINDED,
                0,
                durations.blinded))
        {
            conditionApplied = true;

            if (durations.blinded > resolution.conditionDuration)
                resolution.conditionDuration = durations.blinded;
        }

        if (conditionApplied)
            resolution.targetsAffected++;
    }

    payAbilityCost(caster.character, *ability, source);
    if (combat.active)
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
