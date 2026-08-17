#include <Arduino.h>
#include <unity.h>

#include "../../src/characters/characters.h"

// The project has no host test environment yet. Build the condition
// implementation directly into this embedded Unity suite and provide the
// character-rule dependencies it needs for query checks.
int getAbilityModifier(const Character& character, AbilityScore ability)
{
    int score = 10;

    switch (ability)
    {
        case ABILITY_STRENGTH:
            score = character.abilities.strength;
            break;

        case ABILITY_DEXTERITY:
            score = character.abilities.dexterity;
            break;

        case ABILITY_CONSTITUTION:
            score = character.abilities.constitution;
            break;

        case ABILITY_INTELLIGENCE:
            score = character.abilities.intelligence;
            break;

        case ABILITY_WISDOM:
            score = character.abilities.wisdom;
            break;

        case ABILITY_CHARISMA:
            score = character.abilities.charisma;
            break;
    }

    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

bool isConscious(const Character& character)
{
    return character.state == STATE_ALIVE;
}

int rollDie(int sides)
{
    return sides > 0 ? 2 : 0;
}

#include "../../src/characters/conditions.cpp"

void test_add_get_and_refresh_condition()
{
    Character character = {};

    TEST_ASSERT_FALSE(addCondition(character, CONDITION_NONE, 0, 1));
    TEST_ASSERT_FALSE(addCondition(character, CONDITION_MAX, 0, 1));
    TEST_ASSERT_FALSE(addCondition(character, CONDITION_BLESSED, 0, -1));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLESSED, 1, 2));
    TEST_ASSERT_TRUE(hasCondition(character, CONDITION_BLESSED));
    TEST_ASSERT_EQUAL_UINT8(1, character.conditions.count);

    const Condition* first =
        getCondition(character, CONDITION_BLESSED);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_INT(1, first->value);
    TEST_ASSERT_EQUAL_INT(2, first->roundsRemaining);

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLESSED, 3, 4));
    TEST_ASSERT_EQUAL_UINT8(1, character.conditions.count);

    const Condition* refreshed =
        getCondition(character, CONDITION_BLESSED);
    TEST_ASSERT_NOT_NULL(refreshed);
    TEST_ASSERT_EQUAL_INT(3, refreshed->value);
    TEST_ASSERT_EQUAL_INT(4, refreshed->roundsRemaining);

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLESSED, 2, 1));
    TEST_ASSERT_EQUAL_INT(2, refreshed->value);
    TEST_ASSERT_EQUAL_INT(4, refreshed->roundsRemaining);
}

void test_tick_removes_timed_conditions_only()
{
    Character character = {};

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLINDED, 0, 2));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_MAGE_ARMOR, 4, 0));

    tickConditions(character);
    const Condition* blinded =
        getCondition(character, CONDITION_BLINDED);
    TEST_ASSERT_NOT_NULL(blinded);
    TEST_ASSERT_EQUAL_INT(1, blinded->roundsRemaining);

    tickConditions(character);
    TEST_ASSERT_FALSE(hasCondition(character, CONDITION_BLINDED));
    TEST_ASSERT_TRUE(hasCondition(character, CONDITION_MAGE_ARMOR));
    TEST_ASSERT_EQUAL_UINT8(1, character.conditions.count);

    clearConditions(character);
    TEST_ASSERT_EQUAL_UINT8(0, character.conditions.count);
}

void test_condition_queries()
{
    Character character = {};
    character.state = STATE_ALIVE;
    character.abilities.dexterity = 16;

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_FLAT_FOOTED, 0, 3));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_MAGE_ARMOR, 4, 3));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLINDED, 0, 3));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_BLESSED, 2, 3));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_POISONED, 1, 3));

    TEST_ASSERT_EQUAL_INT(-1, getConditionArmorClassModifier(character));
    TEST_ASSERT_EQUAL_INT(-1, getConditionAttackModifier(character));
    TEST_ASSERT_TRUE(canCharacterAct(character));

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_STUNNED, 0, 1));
    TEST_ASSERT_FALSE(canCharacterAct(character));
    TEST_ASSERT_TRUE(removeCondition(character, CONDITION_STUNNED));

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_PARALYZED, 0, 1));
    TEST_ASSERT_FALSE(canCharacterAct(character));
    TEST_ASSERT_TRUE(removeCondition(character, CONDITION_PARALYZED));

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_SLEEPING, 0, 1));
    TEST_ASSERT_FALSE(canCharacterAct(character));
}

void test_sleep_immunity_and_damage_waking_are_generic()
{
    Character living = {};
    living.state = STATE_ALIVE;
    living.creatureType = CREATURE_GOBLIN;

    Character skeleton = living;
    skeleton.creatureType = CREATURE_SKELETON;

    Character zombie = living;
    zombie.creatureType = CREATURE_ZOMBIE;

    TEST_ASSERT_TRUE(
        canReceiveCondition(living, CONDITION_SLEEPING));
    TEST_ASSERT_FALSE(
        canReceiveCondition(skeleton, CONDITION_SLEEPING));
    TEST_ASSERT_FALSE(
        canReceiveCondition(zombie, CONDITION_SLEEPING));
    TEST_ASSERT_TRUE(
        canReceiveCondition(zombie, CONDITION_POISONED));

    TEST_ASSERT_TRUE(addCondition(
        living, CONDITION_SLEEPING, 0, 3));
    TEST_ASSERT_TRUE(addCondition(
        living, CONDITION_BLESSED, 1, 3));
    updateConditionsAfterDamage(living, 0);
    TEST_ASSERT_TRUE(hasCondition(living, CONDITION_SLEEPING));
    TEST_ASSERT_TRUE(hasCondition(living, CONDITION_BLESSED));

    updateConditionsAfterDamage(living, 2);
    TEST_ASSERT_FALSE(hasCondition(living, CONDITION_SLEEPING));
    TEST_ASSERT_TRUE(hasCondition(living, CONDITION_BLESSED));
}

void test_sleep_duration_prevents_each_intended_turn()
{
    Character character = {};
    character.state = STATE_ALIVE;

    TEST_ASSERT_TRUE(addCondition(
        character, CONDITION_SLEEPING, 0, 3));

    ConditionTurnResult first = processConditionsAtTurnStart(character);
    TEST_ASSERT_TRUE(first.actionPrevented);
    TEST_ASSERT_FALSE(canCharacterAct(character));
    TEST_ASSERT_EQUAL_INT(
        2,
        getCondition(character, CONDITION_SLEEPING)->roundsRemaining);

    ConditionTurnResult second = processConditionsAtTurnStart(character);
    TEST_ASSERT_TRUE(second.actionPrevented);
    TEST_ASSERT_EQUAL_INT(
        1,
        getCondition(character, CONDITION_SLEEPING)->roundsRemaining);

    ConditionTurnResult third = processConditionsAtTurnStart(character);
    TEST_ASSERT_TRUE(third.actionPrevented);
    TEST_ASSERT_FALSE(hasCondition(character, CONDITION_SLEEPING));
    TEST_ASSERT_TRUE(canCharacterAct(character));

    ConditionTurnResult fourth = processConditionsAtTurnStart(character);
    TEST_ASSERT_FALSE(fourth.actionPrevented);
}

void test_condition_capacity_failure_is_safe()
{
    Character character = {};

    for (int rawType = CONDITION_NONE + 1;
         rawType < CONDITION_MAX &&
         character.conditions.count < MAX_CONDITIONS_PER_CHARACTER;
         rawType++)
    {
        ConditionType type = static_cast<ConditionType>(rawType);
        if (type != CONDITION_SLEEPING)
            TEST_ASSERT_TRUE(addCondition(character, type, 0, 2));
    }

    TEST_ASSERT_EQUAL_UINT8(
        MAX_CONDITIONS_PER_CHARACTER, character.conditions.count);
    TEST_ASSERT_FALSE(addCondition(
        character, CONDITION_SLEEPING, 0, 3));
    TEST_ASSERT_EQUAL_UINT8(
        MAX_CONDITIONS_PER_CHARACTER, character.conditions.count);
    TEST_ASSERT_FALSE(hasCondition(character, CONDITION_SLEEPING));
}

void test_poison_ticks_at_turn_start_and_expires()
{
    Character character = {};
    character.state = STATE_ALIVE;
    character.health.currentHP = 20;

    TEST_ASSERT_TRUE(addCondition(character, CONDITION_POISONED, 0, 1));
    TEST_ASSERT_TRUE(addCondition(character, CONDITION_POISONED, 0, 3));
    TEST_ASSERT_EQUAL_UINT8(1, character.conditions.count);

    ConditionTurnResult first = processConditionsAtTurnStart(character);
    TEST_ASSERT_EQUAL_INT(2, first.damage);
    TEST_ASSERT_EQUAL(CONDITION_POISONED, first.damageCondition);
    TEST_ASSERT_FALSE(first.poisonExpired);
    TEST_ASSERT_EQUAL_INT(18, character.health.currentHP);

    const Condition* poisoned =
        getCondition(character, CONDITION_POISONED);
    TEST_ASSERT_NOT_NULL(poisoned);
    TEST_ASSERT_EQUAL_INT(2, poisoned->roundsRemaining);

    ConditionTurnResult second = processConditionsAtTurnStart(character);
    TEST_ASSERT_EQUAL_INT(2, second.damage);
    TEST_ASSERT_FALSE(second.poisonExpired);
    TEST_ASSERT_EQUAL_INT(16, character.health.currentHP);

    ConditionTurnResult third = processConditionsAtTurnStart(character);
    TEST_ASSERT_EQUAL_INT(2, third.damage);
    TEST_ASSERT_TRUE(third.poisonExpired);
    TEST_ASSERT_EQUAL_INT(14, character.health.currentHP);
    TEST_ASSERT_FALSE(hasCondition(character, CONDITION_POISONED));
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_add_get_and_refresh_condition);
    RUN_TEST(test_tick_removes_timed_conditions_only);
    RUN_TEST(test_condition_queries);
    RUN_TEST(test_sleep_immunity_and_damage_waking_are_generic);
    RUN_TEST(test_sleep_duration_prevents_each_intended_turn);
    RUN_TEST(test_condition_capacity_failure_is_safe);
    RUN_TEST(test_poison_ticks_at_turn_start_and_expires);
    UNITY_END();
}

void loop()
{
}
