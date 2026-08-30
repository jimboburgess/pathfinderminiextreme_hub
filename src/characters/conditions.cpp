//
// Created by james on 7/25/2026.
//

#include "conditions.h"

#include "characters.h"
#include "data/dice.h"

namespace
{
ConditionModifiers getLegacyModifiers(ConditionType type, int value)
{
    ConditionModifiers modifiers;
    switch (type)
    {
        case CONDITION_BLESSED:
        case CONDITION_BUFF_ATTACK: modifiers.attackBonus = value; break;
        case CONDITION_BUFF_AC:
        case CONDITION_MAGE_ARMOR: modifiers.acBonus = value; break;
        case CONDITION_BUFF_SAVE: modifiers.saveBonus = value; break;
        case CONDITION_POISONED: modifiers.attackBonus = -value; break;
        case CONDITION_BLINDED: modifiers.attackBonus = -2; modifiers.acBonus = -2; break;
        default: break;
    }
    return modifiers;
}

void accumulateModifiers(ConditionModifiers& result,
                         const ConditionModifiers& value)
{
    result.attackBonus += value.attackBonus; result.damageBonus += value.damageBonus;
    result.acBonus += value.acBonus; result.saveBonus += value.saveBonus;
    result.strBonus += value.strBonus; result.dexBonus += value.dexBonus;
    result.conBonus += value.conBonus; result.intBonus += value.intBonus;
    result.wisBonus += value.wisBonus; result.chaBonus += value.chaBonus;
    result.speedBonus += value.speedBonus; result.bonusAttacks += value.bonusAttacks;
}

void logPoisonApplied(int rounds)
{
    Serial.print("POISONED applied: ");
    Serial.print(rounds);
    Serial.println(" rounds");
}
}

bool isValidConditionType(ConditionType type)
{
    return type > CONDITION_NONE && type < CONDITION_MAX;
}

bool canReceiveCondition(
    const Character& character,
    ConditionType type)
{
    if (!isValidConditionType(type))
        return false;

    if (type == CONDITION_SLEEPING &&
        (character.creatureType == CREATURE_SKELETON ||
         character.creatureType == CREATURE_ZOMBIE))
    {
        return false;
    }

    return true;
}

bool hasCondition(const Character& character, ConditionType type)
{
    return getCondition(character, type) != nullptr;
}

Condition* getCondition(Character& character, ConditionType type)
{
    if (!isValidConditionType(type))
        return nullptr;

    for (uint8_t i = 0; i < character.conditions.count; i++)
    {
        Condition& condition = character.conditions.conditions[i];

        if (condition.type == type)
            return &condition;
    }

    return nullptr;
}

const Condition* getCondition(const Character& character,
                              ConditionType type)
{
    if (!isValidConditionType(type))
        return nullptr;

    for (uint8_t i = 0; i < character.conditions.count; i++)
    {
        const Condition& condition = character.conditions.conditions[i];

        if (condition.type == type)
            return &condition;
    }

    return nullptr;
}

bool addCondition(Character& character,
                  ConditionType type,
                  int value,
                  int rounds)
{
    if (!addCondition(character, type, getLegacyModifiers(type, value), rounds))
        return false;

    Condition* condition = getCondition(character, type);
    if (condition != nullptr)
        condition->value = value;
    return condition != nullptr;
}

bool addCondition(Character& character,
                  ConditionType type,
                  const ConditionModifiers& modifiers,
                  int rounds)
{
    if (!isValidConditionType(type) || rounds < 0)
        return false;

    int duration = rounds;
    Condition* existing = getCondition(character, type);

    if (existing != nullptr)
    {
        existing->value = 0;
        existing->modifiers = modifiers;

        // A zero duration represents a condition that must be explicitly
        // removed. Otherwise, retain the longer of the two durations.
        if (existing->roundsRemaining == 0 || duration == 0)
            existing->roundsRemaining = 0;
        else if (duration > existing->roundsRemaining)
            existing->roundsRemaining = duration;

        if (type == CONDITION_POISONED)
            logPoisonApplied(rounds);

        return true;
    }

    if (character.conditions.count >= MAX_CONDITIONS_PER_CHARACTER)
        return false;

    Condition& condition =
        character.conditions.conditions[character.conditions.count++];
    condition.type = type;
    condition.value = 0;
    condition.roundsRemaining = duration;
    condition.modifiers = modifiers;

    if (type == CONDITION_POISONED)
        logPoisonApplied(rounds);

    return true;
}

bool removeCondition(Character& character, ConditionType type)
{
    if (!isValidConditionType(type))
        return false;

    for (uint8_t i = 0; i < character.conditions.count; i++)
    {
        if (character.conditions.conditions[i].type != type)
            continue;

        for (uint8_t j = i; j + 1 < character.conditions.count; j++)
        {
            character.conditions.conditions[j] =
                character.conditions.conditions[j + 1];
        }

        character.conditions.count--;
        character.conditions.conditions[character.conditions.count] =
            Condition{};

        if (type == CONDITION_POISONED)
            Serial.println("POISON removed");

        return true;
    }

    return false;
}

bool addTimedDamageEffect(Character& character,
                          const TimedDamageEffect& effect)
{
    if (effect.damageType == 0 || effect.diceCount == 0 ||
        effect.diceSides == 0 || effect.roundsRemaining == 0)
        return false;

    for (uint8_t i = 0; i < character.conditions.timedDamageCount; i++)
    {
        TimedDamageEffect& existing = character.conditions.timedDamage[i];
        if (existing.sourceAbility == effect.sourceAbility &&
            existing.damageType == effect.damageType)
        {
            existing = effect;
            return true;
        }
    }

    if (character.conditions.timedDamageCount >= MAX_TIMED_DAMAGE_EFFECTS)
        return false;

    character.conditions.timedDamage[
        character.conditions.timedDamageCount++] = effect;
    return true;
}

bool addEnergyResistance(Character& character,
                         uint8_t damageType,
                         int amount,
                         int rounds)
{
    if (damageType == 0 || amount <= 0 || rounds <= 0)
        return false;

    for (uint8_t i = 0; i < character.conditions.energyResistanceCount; i++)
    {
        EnergyResistance& existing = character.conditions.energyResistances[i];
        if (existing.damageType == damageType)
        {
            if (amount > existing.amount)
                existing.amount = amount;
            if (rounds > existing.roundsRemaining)
                existing.roundsRemaining = rounds;
            return true;
        }
    }

    if (character.conditions.energyResistanceCount >= MAX_ENERGY_RESISTANCES)
        return false;

    EnergyResistance& resistance = character.conditions.energyResistances[
        character.conditions.energyResistanceCount++];
    resistance.damageType = damageType;
    resistance.amount = amount;
    resistance.roundsRemaining = rounds;
    return true;
}

int getEnergyResistance(const Character& character, uint8_t damageType)
{
    int highest = 0;
    for (uint8_t i = 0; i < character.conditions.energyResistanceCount; i++)
    {
        const EnergyResistance& resistance =
            character.conditions.energyResistances[i];
        if (resistance.damageType == damageType && resistance.amount > highest)
            highest = resistance.amount;
    }
    return highest;
}

bool addEnergyProtection(Character& character,
                         uint8_t damageType,
                         int amount,
                         int rounds)
{
    if (damageType == 0 || amount <= 0 || rounds <= 0)
        return false;

    for (uint8_t i = 0; i < character.conditions.energyProtectionCount; i++)
    {
        EnergyProtection& existing = character.conditions.energyProtections[i];
        if (existing.damageType == damageType)
        {
            if (amount > existing.remainingAbsorption)
                existing.remainingAbsorption = amount;
            if (rounds > existing.roundsRemaining)
                existing.roundsRemaining = rounds;
            return true;
        }
    }

    if (character.conditions.energyProtectionCount >= MAX_ENERGY_PROTECTIONS)
        return false;

    EnergyProtection& protection = character.conditions.energyProtections[
        character.conditions.energyProtectionCount++];
    protection.damageType = damageType;
    protection.remainingAbsorption = amount;
    protection.roundsRemaining = rounds;
    return true;
}

int getEnergyProtection(const Character& character, uint8_t damageType)
{
    for (uint8_t i = 0; i < character.conditions.energyProtectionCount; i++)
    {
        const EnergyProtection& protection =
            character.conditions.energyProtections[i];
        if (protection.damageType == damageType)
            return protection.remainingAbsorption;
    }
    return 0;
}

int absorbEnergyProtection(Character& character,
                           uint8_t damageType,
                           int incomingDamage)
{
    if (incomingDamage <= 0 || damageType == 0)
        return 0;

    for (uint8_t i = 0; i < character.conditions.energyProtectionCount; i++)
    {
        EnergyProtection& protection = character.conditions.energyProtections[i];
        if (protection.damageType != damageType)
            continue;

        const int absorbed = incomingDamage < protection.remainingAbsorption
            ? incomingDamage : protection.remainingAbsorption;
        protection.remainingAbsorption -= absorbed;
        if (protection.remainingAbsorption == 0)
        {
            for (uint8_t j = i;
                 j + 1 < character.conditions.energyProtectionCount; j++)
            {
                character.conditions.energyProtections[j] =
                    character.conditions.energyProtections[j + 1];
            }
            character.conditions.energyProtectionCount--;
            character.conditions.energyProtections[
                character.conditions.energyProtectionCount] = {};
        }
        return absorbed;
    }
    return 0;
}

int applyEnergyMitigation(Character& character,
                          uint8_t damageType,
                          int incomingDamage)
{
    if (incomingDamage <= 0)
        return 0;

    incomingDamage -= absorbEnergyProtection(
        character, damageType, incomingDamage);
    incomingDamage -= getEnergyResistance(character, damageType);
    return incomingDamage > 0 ? incomingDamage : 0;
}

void clearConditions(Character& character)
{
    character.conditions = ConditionData{};
}

void updateConditionsAfterDamage(Character& character, int damage)
{
    if (damage > 0)
        removeCondition(character, CONDITION_SLEEPING);
}

void tickConditions(Character& character)
{
    for (uint8_t i = 0; i < character.conditions.count;)
    {
        Condition& condition = character.conditions.conditions[i];

        if (condition.roundsRemaining > 0)
        {
            condition.roundsRemaining--;

            if (condition.type == CONDITION_POISONED)
            {
                Serial.print("POISON tick: ");
                Serial.print(condition.roundsRemaining);
                Serial.println(" rounds remaining");
            }

            if (condition.roundsRemaining == 0)
            {
                removeCondition(character, condition.type);
                continue;
            }
        }

        i++;
    }

    for (uint8_t i = 0; i < character.conditions.energyResistanceCount;)
    {
        EnergyResistance& resistance =
            character.conditions.energyResistances[i];
        if (--resistance.roundsRemaining == 0)
        {
            for (uint8_t j = i; j + 1 < character.conditions.energyResistanceCount; j++)
                character.conditions.energyResistances[j] =
                    character.conditions.energyResistances[j + 1];
            character.conditions.energyResistanceCount--;
            character.conditions.energyResistances[
                character.conditions.energyResistanceCount] = {};
            continue;
        }
        i++;
    }

    for (uint8_t i = 0; i < character.conditions.energyProtectionCount;)
    {
        EnergyProtection& protection =
            character.conditions.energyProtections[i];
        if (--protection.roundsRemaining == 0)
        {
            for (uint8_t j = i;
                 j + 1 < character.conditions.energyProtectionCount; j++)
            {
                character.conditions.energyProtections[j] =
                    character.conditions.energyProtections[j + 1];
            }
            character.conditions.energyProtectionCount--;
            character.conditions.energyProtections[
                character.conditions.energyProtectionCount] = {};
            continue;
        }
        i++;
    }
}

ConditionTurnResult processConditionsAtTurnStart(Character& character)
{
    ConditionTurnResult result;

    if (character.state != STATE_ALIVE)
        return result;

    // Capture action eligibility before durations advance. A one-round
    // disabling condition therefore prevents exactly one turn even though it
    // expires during that turn's start processing.
    result.actionPrevented = !canCharacterAct(character);

    for (uint8_t i = 0; i < character.conditions.timedDamageCount;)
    {
        TimedDamageEffect& effect = character.conditions.timedDamage[i];
        if (result.timedDamageCount < MAX_TIMED_DAMAGE_EFFECTS)
            result.timedDamage[result.timedDamageCount++] = effect;

        effect.roundsRemaining--;
        if (effect.roundsRemaining == 0)
        {
            for (uint8_t j = i; j + 1 < character.conditions.timedDamageCount; j++)
                character.conditions.timedDamage[j] = character.conditions.timedDamage[j + 1];
            character.conditions.timedDamageCount--;
            character.conditions.timedDamage[character.conditions.timedDamageCount] = {};
            continue;
        }
        i++;
    }

    if (hasCondition(character, CONDITION_POISONED))
    {
        result.damage = rollDie(4);
        result.damageCondition = CONDITION_POISONED;
        character.health.currentHP -= result.damage;
        updateConditionsAfterDamage(character, result.damage);
    }

    bool wasPoisoned = hasCondition(character, CONDITION_POISONED);
    tickConditions(character);
    result.poisonExpired = wasPoisoned &&
                           !hasCondition(character, CONDITION_POISONED);

    return result;
}

bool canCharacterAct(const Character& character)
{
    if (!isConscious(character))
        return false;

    return !hasCondition(character, CONDITION_STUNNED) &&
           !hasCondition(character, CONDITION_PARALYZED) &&
           !hasCondition(character, CONDITION_SLEEPING) &&
           !hasCondition(character, CONDITION_FRIGHTENED);
}

int getConditionAttackModifier(const Character& character)
{
    return getActiveConditionModifiers(character).attackBonus;
}

ConditionModifiers getActiveConditionModifiers(const Character& character)
{
    ConditionModifiers modifiers;
    uint8_t count = character.conditions.count;
    if (count > MAX_CONDITIONS_PER_CHARACTER)
        count = MAX_CONDITIONS_PER_CHARACTER;

    for (uint8_t i = 0; i < count; i++)
    {
        accumulateModifiers(modifiers, character.conditions.conditions[i].modifiers);
    }
    // Slow prevents temporary extra attacks while leaving ordinary iterative
    // attacks intact. Its normal attack/AC/save penalties still aggregate.
    if (hasCondition(character, CONDITION_SLOWED))
        modifiers.bonusAttacks = 0;
    if (hasCondition(character, CONDITION_FLAT_FOOTED))
    {
        const int dexterity = character.abilities.dexterity;
        const int dexterityModifier = dexterity >= 10
            ? (dexterity - 10) / 2 : (dexterity - 11) / 2;
        if (dexterityModifier > 0)
            modifiers.acBonus -= dexterityModifier;
    }
    return modifiers;
}

int getConditionArmorClassModifier(const Character& character)
{
    return getActiveConditionModifiers(character).acBonus;
}

int getConditionSaveModifier(const Character& character)
{
    return getActiveConditionModifiers(character).saveBonus;
}

const char* getActionAffectingConditionMessage(const Character& character)
{
    const bool prone = hasCondition(character, CONDITION_PRONE);
    const bool webbed = hasCondition(character, CONDITION_WEBBED);

    if (prone && webbed)
        return "You are prone and stuck in the web.";
    if (prone)
        return "You are prone.";
    if (webbed)
        return "You are stuck in the web.";

    return nullptr;
}

