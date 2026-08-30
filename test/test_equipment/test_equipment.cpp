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
static int controlledBaseAttackBonus = 0;
static int controlledAttackModifier = 0;
static int controlledArmorClassModifier = 0;
int getBaseAttackBonus(CharacterClass, uint8_t) {
    return controlledBaseAttackBonus;
}
int getBaseHitPoints(CharacterClass, uint8_t) { return 1; }
int getBaseSave(CharacterClass, SaveType, uint8_t) { return 0; }
uint32_t getExperienceForLevel(uint8_t level) { return level * 1000UL; }
void updateConditionsAfterDamage(Character&, int) {}
int getConditionAttackModifier(const Character&) {
    return controlledAttackModifier;
}
int getConditionArmorClassModifier(const Character&) {
    return controlledArmorClassModifier;
}

#include "../../src/characters/items.cpp"
#include "../../src/characters/characters.cpp"

static Character makeTestCharacter()
{
    Character character = {};
    character.level = 1;
    character.abilities = {10, 10, 10, 10, 10, 10};
    clearInventory(character.inventory);
    for (uint8_t i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
        character.equipment.equipped[i] = makeItemInstance(ITEM_NONE);
    return character;
}

static ItemInstance enhanced(ItemID id, int8_t bonus,
                             WeaponEnhancement property)
{
    ItemInstance item = makeItemInstance(id);
    item.enhancementBonus = bonus;
    item.weaponEnhancement = property;
    return item;
}

void test_exact_instance_swap_preserves_every_field()
{
    Character character = makeTestCharacter();
    ItemInstance incoming = enhanced(
        ITEM_DAGGER, 1, WEAPON_ENHANCEMENT_FLAMING);
    ItemInstance outgoing = enhanced(
        ITEM_LONGSWORD, 2, WEAPON_ENHANCEMENT_FROST);
    character.equipment.equipped[SLOT_MELEE_WEAPON] = outgoing;
    TEST_ASSERT_TRUE(addItem(character, incoming));

    TEST_ASSERT_EQUAL(EQUIP_SUCCESS,
                      equipItemWithResult(character, incoming));
    TEST_ASSERT_TRUE(character.equipment.equipped[SLOT_MELEE_WEAPON] == incoming);
    TEST_ASSERT_TRUE(hasItem(character, outgoing));
    TEST_ASSERT_FALSE(hasItem(character, incoming));

    TEST_ASSERT_TRUE(unequipItem(character, SLOT_MELEE_WEAPON));
    TEST_ASSERT_TRUE(hasItem(character, incoming));
    TEST_ASSERT_EQUAL(ITEM_NONE,
        character.equipment.equipped[SLOT_MELEE_WEAPON].itemID);
}

void test_full_inventory_allows_one_for_one_swap()
{
    Character character = makeTestCharacter();
    ItemInstance incoming = makeItemInstance(ITEM_DAGGER);
    ItemInstance outgoing = makeItemInstance(ITEM_LONGSWORD);
    character.equipment.equipped[SLOT_MELEE_WEAPON] = outgoing;
    TEST_ASSERT_TRUE(addItem(character, incoming));

    for (uint8_t i = 1; i < MAX_INVENTORY; i++)
    {
        ItemInstance filler = makeItemInstance(ITEM_DAGGER);
        filler.enhancementBonus = static_cast<int8_t>(i);
        TEST_ASSERT_TRUE(addItem(character, filler));
    }
    TEST_ASSERT_EQUAL_UINT8(MAX_INVENTORY, character.inventory.itemCount);
    TEST_ASSERT_EQUAL(EQUIP_SUCCESS,
                      equipItemWithResult(character, incoming));
    TEST_ASSERT_EQUAL_UINT8(MAX_INVENTORY, character.inventory.itemCount);
    TEST_ASSERT_TRUE(hasItem(character, outgoing));
}

void test_two_handed_and_shield_conflicts_are_atomic()
{
    Character character = makeTestCharacter();
    ItemInstance greatsword = enhanced(
        ITEM_GREATSWORD, 1, WEAPON_ENHANCEMENT_SHOCK);
    ItemInstance shield = makeItemInstance(ITEM_HEAVY_STEEL_SHIELD);
    character.equipment.equipped[SLOT_SHIELD] = shield;
    TEST_ASSERT_TRUE(addItem(character, greatsword));

    TEST_ASSERT_EQUAL(EQUIP_TWO_HANDED_CONFLICT,
                      equipItemWithResult(character, greatsword));
    TEST_ASSERT_TRUE(hasItem(character, greatsword));
    TEST_ASSERT_TRUE(character.equipment.equipped[SLOT_SHIELD] == shield);

    character.equipment.equipped[SLOT_SHIELD] = makeItemInstance(ITEM_NONE);
    character.equipment.equipped[SLOT_MELEE_WEAPON] = greatsword;
    TEST_ASSERT_TRUE(addItem(character, shield));
    TEST_ASSERT_EQUAL(EQUIP_TWO_HANDED_CONFLICT,
                      equipItemWithResult(character, shield));
    TEST_ASSERT_TRUE(hasItem(character, shield));
    TEST_ASSERT_TRUE(
        character.equipment.equipped[SLOT_MELEE_WEAPON] == greatsword);
}

void test_unequip_fails_safely_when_inventory_is_full()
{
    Character character = makeTestCharacter();
    ItemInstance equipped = enhanced(
        ITEM_LONGSWORD, 2, WEAPON_ENHANCEMENT_FLAMING);
    character.equipment.equipped[SLOT_MELEE_WEAPON] = equipped;

    for (uint8_t i = 0; i < MAX_INVENTORY; i++)
    {
        ItemInstance filler = makeItemInstance(ITEM_DAGGER);
        filler.enhancementBonus = static_cast<int8_t>(i + 1);
        TEST_ASSERT_TRUE(addItem(character, filler));
    }

    TEST_ASSERT_FALSE(unequipItem(character, SLOT_MELEE_WEAPON));
    TEST_ASSERT_TRUE(
        character.equipment.equipped[SLOT_MELEE_WEAPON] == equipped);
    TEST_ASSERT_EQUAL_UINT8(MAX_INVENTORY, character.inventory.itemCount);
}

void test_slot_filtering_and_enhancement_stats()
{
    Character character = makeTestCharacter();
    TEST_ASSERT_TRUE(isItemCompatibleWithEquipmentSlot(
        makeItemInstance(ITEM_DAGGER), SLOT_MELEE_WEAPON));
    TEST_ASSERT_TRUE(isItemCompatibleWithEquipmentSlot(
        makeItemInstance(ITEM_SHORTBOW), SLOT_RANGED_WEAPON));
    TEST_ASSERT_TRUE(isItemCompatibleWithEquipmentSlot(
        makeItemInstance(ITEM_CHAINMAIL), SLOT_ARMOR));
    TEST_ASSERT_TRUE(isItemCompatibleWithEquipmentSlot(
        makeItemInstance(ITEM_HEAVY_STEEL_SHIELD), SLOT_SHIELD));

    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        enhanced(ITEM_DAGGER, 1, WEAPON_ENHANCEMENT_NONE);
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        enhanced(ITEM_SHORTBOW, 1, WEAPON_ENHANCEMENT_NONE);
    character.equipment.equipped[SLOT_ARMOR] =
        enhanced(ITEM_CHAINMAIL, 1, WEAPON_ENHANCEMENT_NONE);
    character.equipment.equipped[SLOT_SHIELD] =
        enhanced(ITEM_HEAVY_STEEL_SHIELD, 1, WEAPON_ENHANCEMENT_NONE);
    TEST_ASSERT_EQUAL(1, getMeleeAttackBonus(character));
    TEST_ASSERT_EQUAL(1, getRangedAttackBonus(character));

    int expectedAC = 10 + getArmor(ITEM_CHAINMAIL)->armorBonus +
                     getShield(ITEM_HEAVY_STEEL_SHIELD)->shieldBonus + 2;
    TEST_ASSERT_EQUAL(expectedAC, getArmorClass(character));
}

void test_ranged_touch_bonus_uses_bab_dex_and_generic_modifier_only()
{
    Character character = makeTestCharacter();
    character.abilities.dexterity = 16;
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        enhanced(ITEM_SHORTBOW, 4, WEAPON_ENHANCEMENT_NONE);
    controlledBaseAttackBonus = 5;
    controlledAttackModifier = 2;

    TEST_ASSERT_EQUAL(10, getRangedTouchAttackBonus(character));
    TEST_ASSERT_EQUAL(14, getRangedAttackBonus(character));

    controlledBaseAttackBonus = 0;
    controlledAttackModifier = 0;
}

void test_melee_touch_bonus_uses_bab_str_and_generic_modifier_only()
{
    Character character = makeTestCharacter();
    character.abilities.strength = 18;
    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        enhanced(ITEM_LONGSWORD, 5, WEAPON_ENHANCEMENT_NONE);
    controlledBaseAttackBonus = 4;
    controlledAttackModifier = -1;

    TEST_ASSERT_EQUAL(7, getMeleeTouchAttackBonus(character));
    TEST_ASSERT_EQUAL(12, getMeleeAttackBonus(character));

    controlledBaseAttackBonus = 0;
    controlledAttackModifier = 0;
}

void test_touch_ac_ignores_armor_shield_and_untyped_ac_buffs()
{
    Character character = makeTestCharacter();
    character.abilities.dexterity = 14;
    controlledArmorClassModifier = 3;
    character.equipment.equipped[SLOT_ARMOR] =
        enhanced(ITEM_CHAINMAIL, 2, WEAPON_ENHANCEMENT_NONE);
    character.equipment.equipped[SLOT_SHIELD] =
        enhanced(ITEM_HEAVY_STEEL_SHIELD, 2,
                 WEAPON_ENHANCEMENT_NONE);

    TEST_ASSERT_EQUAL(12, getTouchArmorClass(character));
    TEST_ASSERT_TRUE(getArmorClass(character) >
                     getTouchArmorClass(character));

    character.equipment.equipped[SLOT_ARMOR] =
        makeItemInstance(ITEM_NONE);
    character.equipment.equipped[SLOT_SHIELD] =
        makeItemInstance(ITEM_NONE);
    TEST_ASSERT_EQUAL(12, getTouchArmorClass(character));
    TEST_ASSERT_EQUAL(14, getTouchArmorClass(character, 2));
    controlledArmorClassModifier = 0;
}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_exact_instance_swap_preserves_every_field);
    RUN_TEST(test_full_inventory_allows_one_for_one_swap);
    RUN_TEST(test_two_handed_and_shield_conflicts_are_atomic);
    RUN_TEST(test_unequip_fails_safely_when_inventory_is_full);
    RUN_TEST(test_slot_filtering_and_enhancement_stats);
    RUN_TEST(test_ranged_touch_bonus_uses_bab_dex_and_generic_modifier_only);
    RUN_TEST(test_melee_touch_bonus_uses_bab_str_and_generic_modifier_only);
    RUN_TEST(test_touch_ac_ignores_armor_shield_and_untyped_ac_buffs);
    UNITY_END();
}

void loop() {}
