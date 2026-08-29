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
#include "../../src/map/dungeontools.cpp"

namespace
{
Character makeTestCharacter()
{
    Character character = {};
    character.level = 1;
    character.abilities = {10, 10, 10, 10, 10, 10};
    clearInventory(character.inventory);
    return character;
}
}

void test_disable_device_tool_modifiers_and_priority()
{
    Character character = makeTestCharacter();
    TEST_ASSERT_EQUAL(DISABLE_TOOL_NONE, getDisableDeviceTool(character));
    TEST_ASSERT_EQUAL(-4, getDisableDeviceToolModifier(character));

    TEST_ASSERT_TRUE(addItem(character, ITEM_THIEVES_TOOLS));
    TEST_ASSERT_EQUAL(DISABLE_TOOL_STANDARD, getDisableDeviceTool(character));
    TEST_ASSERT_EQUAL(0, getDisableDeviceToolModifier(character));

    TEST_ASSERT_TRUE(addItem(character, ITEM_MASTERWORK_THIEVES_TOOLS));
    TEST_ASSERT_EQUAL(DISABLE_TOOL_MASTERWORK, getDisableDeviceTool(character));
    TEST_ASSERT_EQUAL(2, getDisableDeviceToolModifier(character));
}

void test_natural_one_breaks_only_the_selected_tool_set()
{
    Character character = makeTestCharacter();
    TEST_ASSERT_TRUE(isDisableDeviceAutomaticFailure(1));
    TEST_ASSERT_FALSE(isDisableDeviceAutomaticFailure(2));
    TEST_ASSERT_TRUE(addItem(character, ITEM_THIEVES_TOOLS, 2));
    TEST_ASSERT_FALSE(handleDisableDeviceToolBreak(
        character, DISABLE_TOOL_STANDARD, 2));
    TEST_ASSERT_EQUAL_UINT16(2, getItemQuantity(character, ITEM_THIEVES_TOOLS));

    TEST_ASSERT_TRUE(handleDisableDeviceToolBreak(
        character, DISABLE_TOOL_STANDARD, 1));
    TEST_ASSERT_EQUAL_UINT16(1, getItemQuantity(character, ITEM_THIEVES_TOOLS));

    TEST_ASSERT_TRUE(addItem(character, ITEM_MASTERWORK_THIEVES_TOOLS));
    const DisableDeviceToolType tool = getDisableDeviceTool(character);
    TEST_ASSERT_EQUAL(DISABLE_TOOL_MASTERWORK, tool);
    TEST_ASSERT_TRUE(handleDisableDeviceToolBreak(character, tool, 1));
    TEST_ASSERT_EQUAL_UINT16(0,
                             getItemQuantity(character,
                                             ITEM_MASTERWORK_THIEVES_TOOLS));
    TEST_ASSERT_EQUAL_UINT16(1, getItemQuantity(character, ITEM_THIEVES_TOOLS));
}

void test_no_tool_natural_one_removes_nothing_and_masterwork_is_inventory_gear()
{
    Character character = makeTestCharacter();
    TEST_ASSERT_FALSE(handleDisableDeviceToolBreak(
        character, DISABLE_TOOL_NONE, 1));
    TEST_ASSERT_EQUAL_UINT8(0, character.inventory.itemCount);
    TEST_ASSERT_TRUE(addItem(character, ITEM_MASTERWORK_THIEVES_TOOLS));
    TEST_ASSERT_TRUE(hasItem(character, ITEM_MASTERWORK_THIEVES_TOOLS));

    const Item* masterwork = getItem(ITEM_MASTERWORK_THIEVES_TOOLS);
    TEST_ASSERT_NOT_NULL(masterwork);
    TEST_ASSERT_EQUAL(ITEMTYPE_ADVENTURING_GEAR, masterwork->type);
    TEST_ASSERT_FALSE(masterwork->consumable);
    TEST_ASSERT_TRUE(masterwork->value > 0);
}

void test_crowbar_lock_and_force_modifiers_combine_with_tools()
{
    Character character = makeTestCharacter();
    TEST_ASSERT_EQUAL(-4, getLockDisableDeviceModifier(character));
    TEST_ASSERT_EQUAL(0, getForceOpenToolModifier(character));

    TEST_ASSERT_TRUE(addItem(character, ITEM_CROWBAR));
    TEST_ASSERT_EQUAL(-2, getLockDisableDeviceModifier(character));
    TEST_ASSERT_EQUAL(2, getForceOpenToolModifier(character));

    TEST_ASSERT_TRUE(addItem(character, ITEM_THIEVES_TOOLS));
    TEST_ASSERT_EQUAL(2, getLockDisableDeviceModifier(character));

    TEST_ASSERT_TRUE(addItem(character, ITEM_MASTERWORK_THIEVES_TOOLS));
    TEST_ASSERT_EQUAL(4, getLockDisableDeviceModifier(character));
    TEST_ASSERT_TRUE(hasItem(character, ITEM_CROWBAR));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_disable_device_tool_modifiers_and_priority);
    RUN_TEST(test_natural_one_breaks_only_the_selected_tool_set);
    RUN_TEST(test_no_tool_natural_one_removes_nothing_and_masterwork_is_inventory_gear);
    RUN_TEST(test_crowbar_lock_and_force_modifiers_combine_with_tools);
    UNITY_END();
}

void loop() {}
