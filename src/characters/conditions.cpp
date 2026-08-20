//
// Created by james on 7/25/2026.
//

#include "conditions.h"

#include "characters.h"
#include "data/dice.h"

namespace
{
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
    if (!isValidConditionType(type) || rounds < 0)
        return false;

    int duration = rounds;
    Condition* existing = getCondition(character, type);

    if (existing != nullptr)
    {
        existing->value = value;

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
    condition.value = value;
    condition.roundsRemaining = duration;

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
    int modifier = 0;

    const Condition* blessed =
        getCondition(character, CONDITION_BLESSED);
    if (blessed != nullptr)
        modifier += blessed->value;

    const Condition* poisoned =
        getCondition(character, CONDITION_POISONED);
    if (poisoned != nullptr)
        modifier -= poisoned->value;

    if (hasCondition(character, CONDITION_BLINDED))
        modifier -= 2;

    return modifier;
}

int getConditionArmorClassModifier(const Character& character)
{
    int modifier = 0;

    const Condition* mageArmor =
        getCondition(character, CONDITION_MAGE_ARMOR);
    if (mageArmor != nullptr)
        modifier += mageArmor->value;

    if (hasCondition(character, CONDITION_BLINDED))
        modifier -= 2;

    if (hasCondition(character, CONDITION_FLAT_FOOTED))
    {
        int dexterityModifier =
            getAbilityModifier(character, ABILITY_DEXTERITY);

        if (dexterityModifier > 0)
            modifier -= dexterityModifier;
    }

    return modifier;
}

