#include <Arduino.h>
#include <unity.h>

#include "../../src/characters/characters.h"
#include "../../src/characters/conditions.h"
#include "../../src/data/progression.h"
#include "../../src/graphics/sprites.h"

const uint16_t fighter16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t rogue16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t wizard16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t cleric16x16[SPRITE_W * SPRITE_H] = {};

int rollDice(int, int) { return 1; }
int getBaseAttackBonus(CharacterClass, uint8_t) { return 0; }
int getBaseHitPoints(CharacterClass, uint8_t) { return 1; }
int getBaseSave(CharacterClass, SaveType, uint8_t) { return 0; }
uint32_t getExperienceForLevel(uint8_t level) { return level * 1000UL; }
void updateConditionsAfterDamage(Character&, int) {}
int getConditionAttackModifier(const Character&) { return 0; }
int getConditionArmorClassModifier(const Character&) { return 0; }

#include "../../src/characters/items.cpp"
#include "../../src/characters/characters.cpp"
#include "../../src/characters/magic.cpp"

namespace
{
Character makeTestCharacter(int maxHP, int currentHP, int maxMP, int currentMP)
{
    Character character = {};
    character.level = 1;
    character.health.maxHP = maxHP;
    character.health.currentHP = currentHP;
    character.magic.maxMP = maxMP;
    character.magic.currentMP = currentMP;
    clearInventory(character.inventory);
    return character;
}
}

void test_ration_restores_one_quarter_of_hp_and_mp_once()
{
    Character character = makeTestCharacter(20, 8, 12, 4);
    TEST_ASSERT_TRUE(addItem(character, ITEM_RATIONS, 3));

    RationRecoveryResult result;
    TEST_ASSERT_EQUAL(RATION_USE_SUCCESS, useRation(character, result));
    TEST_ASSERT_EQUAL(5, result.hpRestored);
    TEST_ASSERT_EQUAL(3, result.mpRestored);
    TEST_ASSERT_EQUAL(13, character.health.currentHP);
    TEST_ASSERT_EQUAL(7, character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT16(2, getItemQuantity(character, ITEM_RATIONS));
}

void test_ration_uses_minimum_one_and_clamps_each_resource()
{
    Character character = makeTestCharacter(3, 2, 3, 2);
    TEST_ASSERT_TRUE(addItem(character, ITEM_RATIONS));

    RationRecoveryResult result;
    TEST_ASSERT_EQUAL(RATION_USE_SUCCESS, useRation(character, result));
    TEST_ASSERT_EQUAL(1, result.hpRestored);
    TEST_ASSERT_EQUAL(1, result.mpRestored);
    TEST_ASSERT_EQUAL(3, character.health.currentHP);
    TEST_ASSERT_EQUAL(3, character.magic.currentMP);
}

void test_ration_recovers_each_resource_independently()
{
    Character hpOnly = makeTestCharacter(20, 19, 10, 10);
    TEST_ASSERT_TRUE(addItem(hpOnly, ITEM_RATIONS));
    RationRecoveryResult result;
    TEST_ASSERT_EQUAL(RATION_USE_SUCCESS, useRation(hpOnly, result));
    TEST_ASSERT_EQUAL(1, result.hpRestored);
    TEST_ASSERT_EQUAL(0, result.mpRestored);

    Character mpOnly = makeTestCharacter(20, 20, 10, 9);
    TEST_ASSERT_TRUE(addItem(mpOnly, ITEM_RATIONS));
    TEST_ASSERT_EQUAL(RATION_USE_SUCCESS, useRation(mpOnly, result));
    TEST_ASSERT_EQUAL(0, result.hpRestored);
    TEST_ASSERT_EQUAL(1, result.mpRestored);
}

void test_ration_fails_without_consuming_when_resources_are_full()
{
    Character caster = makeTestCharacter(20, 20, 10, 10);
    TEST_ASSERT_TRUE(addItem(caster, ITEM_RATIONS));
    RationRecoveryResult result;
    TEST_ASSERT_EQUAL(RATION_USE_RESOURCES_FULL, useRation(caster, result));
    TEST_ASSERT_EQUAL_UINT16(1, getItemQuantity(caster, ITEM_RATIONS));

    Character nonCaster = makeTestCharacter(20, 20, 0, 0);
    TEST_ASSERT_TRUE(addItem(nonCaster, ITEM_RATIONS));
    TEST_ASSERT_EQUAL(RATION_USE_RESOURCES_FULL,
                      useRation(nonCaster, result));
    TEST_ASSERT_EQUAL_UINT16(1, getItemQuantity(nonCaster, ITEM_RATIONS));
}

void test_ration_does_not_change_conditions_or_class_resources()
{
    Character character = makeTestCharacter(20, 15, 0, 0);
    character.conditions.count = 1;
    character.conditions.conditions[0].type = CONDITION_POISONED;
    character.classAbilities.channelEnergyCurrent = 1;
    character.classAbilities.channelEnergyMax = 4;
    TEST_ASSERT_TRUE(addItem(character, ITEM_RATIONS));

    RationRecoveryResult result;
    TEST_ASSERT_EQUAL(RATION_USE_SUCCESS, useRation(character, result));
    TEST_ASSERT_EQUAL_UINT8(1, character.conditions.count);
    TEST_ASSERT_EQUAL(CONDITION_POISONED,
                      character.conditions.conditions[0].type);
    TEST_ASSERT_EQUAL_UINT8(1, character.classAbilities.channelEnergyCurrent);
    TEST_ASSERT_EQUAL_UINT8(4, character.classAbilities.channelEnergyMax);
    TEST_ASSERT_EQUAL(0, result.mpRestored);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_ration_restores_one_quarter_of_hp_and_mp_once);
    RUN_TEST(test_ration_uses_minimum_one_and_clamps_each_resource);
    RUN_TEST(test_ration_recovers_each_resource_independently);
    RUN_TEST(test_ration_fails_without_consuming_when_resources_are_full);
    RUN_TEST(test_ration_does_not_change_conditions_or_class_resources);
    UNITY_END();
}

void loop() {}
