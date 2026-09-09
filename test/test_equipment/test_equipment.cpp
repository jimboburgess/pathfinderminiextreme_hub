#include <Arduino.h>
#include <unity.h>

#include "../../src/characters/characters.h"
#include "../../src/characters/conditions.h"
#include "../../src/data/progression.h"
#include "../../src/dungeon/combatpolicy.h"
#include "../../src/dungeon/loot.h"
#include "../../src/data/entities.h"
#include "../../src/graphics/sprites.h"

const uint16_t fighter16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t rogue16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t wizard16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t cleric16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t chestopenwithout[SPRITE_W * SPRITE_H] = {};

int rollDice(int, int) { return 1; }
static int controlledBaseAttackBonus = 0;
static int controlledAttackModifier = 0;
static int controlledArmorClassModifier = 0;
ConditionModifiers getActiveConditionModifiers(const Character&) {
    ConditionModifiers modifiers;
    modifiers.attackBonus = controlledAttackModifier;
    modifiers.acBonus = controlledArmorClassModifier;
    return modifiers;
}
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
const Monster* getMonster(MonsterID) { return nullptr; }
void markEntityFootprintDirty(const Entity&) {}
void removeEntity(Entity& entity) { entity.active = false; }

#include "../../src/characters/items.cpp"
#include "../../src/characters/characters.cpp"
#include "../../src/dungeon/loot.cpp"

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
                              uint8_t properties)
{
    ItemInstance item = makeItemInstance(id);
    item.enhancementBonus = bonus;
    item.weaponProperties = properties;
    return item;
}

void test_exact_instance_swap_preserves_every_field()
{
    Character character = makeTestCharacter();
    ItemInstance incoming = enhanced(
        ITEM_DAGGER, 1, WEAPON_PROPERTY_FLAMING);
    ItemInstance outgoing = enhanced(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_FROST);
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
        ITEM_GREATSWORD, 1, WEAPON_PROPERTY_SHOCK);
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
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_FLAMING);
    character.equipment.equipped[SLOT_MELEE_WEAPON] = equipped;

    for (uint8_t i = 0; i < MAX_INVENTORY; i++)
    {
        ItemInstance filler = makeItemInstance(ITEM_DAGGER);
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
        enhanced(ITEM_DAGGER, 1, WEAPON_PROPERTY_NONE);
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        enhanced(ITEM_SHORTBOW, 1, WEAPON_PROPERTY_NONE);
    character.equipment.equipped[SLOT_ARMOR] =
        enhanced(ITEM_CHAINMAIL, 1, WEAPON_PROPERTY_NONE);
    character.equipment.equipped[SLOT_SHIELD] =
        enhanced(ITEM_HEAVY_STEEL_SHIELD, 1, WEAPON_PROPERTY_NONE);
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
        enhanced(ITEM_SHORTBOW, 4, WEAPON_PROPERTY_NONE);
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
        enhanced(ITEM_LONGSWORD, 5, WEAPON_PROPERTY_NONE);
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
        enhanced(ITEM_CHAINMAIL, 2, WEAPON_PROPERTY_NONE);
    character.equipment.equipped[SLOT_SHIELD] =
        enhanced(ITEM_HEAVY_STEEL_SHIELD, 2,
                  WEAPON_PROPERTY_NONE);

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

void test_locked_fighter_weapon_progression()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    const Weapon* blade = getWeapon(ITEM_LONGSWORD);
    const Weapon* untrained = getWeapon(ITEM_BATTLEAXE);
    TEST_ASSERT_NOT_NULL(blade);
    TEST_ASSERT_NOT_NULL(untrained);
    TEST_ASSERT_EQUAL(WEAPON_GROUP_BLADES, blade->group);
    TEST_ASSERT_EQUAL(WEAPON_GROUP_AXES, untrained->group);

    fighter.level = 2;
    TEST_ASSERT_EQUAL(1, getFighterWeaponAttackBonus(fighter, *blade));
    TEST_ASSERT_EQUAL(0, getFighterWeaponAttackBonus(fighter, *untrained));

    fighter.level = 4;
    TEST_ASSERT_EQUAL(2, getFighterWeaponDamageBonus(fighter, *blade));

    fighter.level = 5;
    TEST_ASSERT_EQUAL(2, getFighterWeaponAttackBonus(fighter, *blade));
    TEST_ASSERT_EQUAL(3, getFighterWeaponDamageBonus(fighter, *blade));

    fighter.level = 8;
    TEST_ASSERT_EQUAL(3, getFighterWeaponAttackBonus(fighter, *blade));

    fighter.level = 12;
    TEST_ASSERT_EQUAL(2, getFighterWeaponTrainingBonus(fighter));
    TEST_ASSERT_EQUAL(6, getFighterWeaponDamageBonus(fighter, *blade));

    fighter.level = 17;
    TEST_ASSERT_EQUAL(6, getFighterWeaponAttackBonus(fighter, *blade));
    TEST_ASSERT_EQUAL(8, getFighterWeaponDamageBonus(fighter, *blade));

    Character rogue = fighter;
    rogue.characterClass = CLASS_ROGUE;
    TEST_ASSERT_EQUAL(0, getFighterWeaponAttackBonus(rogue, *blade));
    TEST_ASSERT_EQUAL(0, getFighterWeaponDamageBonus(rogue, *blade));
}

void test_fighter_toughness_and_critical_progression()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    const Weapon* longsword = getWeapon(ITEM_LONGSWORD);
    const Weapon* scythe = getWeapon(ITEM_SCYTHE);
    TEST_ASSERT_NOT_NULL(longsword);
    TEST_ASSERT_NOT_NULL(scythe);

    fighter.level = 3;
    TEST_ASSERT_EQUAL(3, getFighterBonusMaxHP(fighter));
    TEST_ASSERT_EQUAL(4, getMaxHP(fighter));

    fighter.level = 10;
    TEST_ASSERT_EQUAL_UINT8(17,
        getWeaponCriticalThreatMinimum(fighter, *longsword));
    TEST_ASSERT_EQUAL_UINT8(19,
        getWeaponCriticalThreatMinimum(fighter, *scythe));
    TEST_ASSERT_FALSE(fighterAutomaticallyConfirmsCritical(
        fighter, *longsword));

    fighter.level = 14;
    TEST_ASSERT_EQUAL(28, getFighterBonusMaxHP(fighter));

    fighter.level = 20;
    TEST_ASSERT_EQUAL(40, getFighterBonusMaxHP(fighter));
    TEST_ASSERT_TRUE(fighterAutomaticallyConfirmsCritical(
        fighter, *longsword));

    const Weapon* battleaxe = getWeapon(ITEM_BATTLEAXE);
    TEST_ASSERT_NOT_NULL(battleaxe);
    TEST_ASSERT_EQUAL_UINT8(battleaxe->criticalThreat,
        getWeaponCriticalThreatMinimum(fighter, *battleaxe));
    TEST_ASSERT_FALSE(fighterAutomaticallyConfirmsCritical(
        fighter, *battleaxe));
}

void test_fighter_bonuses_flow_through_shared_attack_helpers()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    fighter.level = 17;
    fighter.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_LONGSWORD);
    fighter.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(ITEM_SHORTBOW);
    controlledBaseAttackBonus = 17;

    TEST_ASSERT_EQUAL(23, getMeleeAttackBonus(fighter));
    TEST_ASSERT_EQUAL(17, getRangedAttackBonus(fighter));

    controlledBaseAttackBonus = 0;
}

void test_power_attack_scaling_remains_compatible_with_iteratives()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    fighter.level = 16;
    const Weapon* longsword = getWeapon(ITEM_LONGSWORD);
    TEST_ASSERT_NOT_NULL(longsword);
    controlledBaseAttackBonus = 16;

    TEST_ASSERT_EQUAL(-5, getPowerAttackPenalty(fighter));
    TEST_ASSERT_EQUAL(10, getPowerAttackDamageBonus(fighter, *longsword));
    TEST_ASSERT_EQUAL_UINT8(4, getIterativeAttackCount(16));
    TEST_ASSERT_EQUAL(16, getSequenceAttackBAB(16, 0, 0));
    TEST_ASSERT_EQUAL(11, getSequenceAttackBAB(16, 1, 0));
    TEST_ASSERT_EQUAL(6, getSequenceAttackBAB(16, 2, 0));
    TEST_ASSERT_EQUAL(1, getSequenceAttackBAB(16, 3, 0));

    controlledBaseAttackBonus = 0;
}

void test_fighter_selected_groups_control_progression_bonuses()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.level = 20;
    const Weapon* blade = getWeapon(ITEM_LONGSWORD);
    const Weapon* axe = getWeapon(ITEM_BATTLEAXE);
    const Weapon* bludgeon = getWeapon(ITEM_WARHAMMER);
    const Weapon* bow = getWeapon(ITEM_SHORTBOW);
    TEST_ASSERT_NOT_NULL(blade);
    TEST_ASSERT_NOT_NULL(axe);
    TEST_ASSERT_NOT_NULL(bludgeon);
    TEST_ASSERT_NOT_NULL(bow);

    const WeaponGroup groups[] =
    {
        WEAPON_GROUP_BLADES,
        WEAPON_GROUP_AXES,
        WEAPON_GROUP_BLUDGEONS
    };
    const Weapon* trainedWeapons[] = { blade, axe, bludgeon };

    for (uint8_t selected = 0; selected < 3; selected++)
    {
        setFighterTrainedWeaponGroup(fighter, groups[selected]);
        TEST_ASSERT_EQUAL(groups[selected],
            getFighterTrainedWeaponGroup(fighter));

        for (uint8_t weaponIndex = 0; weaponIndex < 3; weaponIndex++)
        {
            const int expectedAttack = selected == weaponIndex ? 6 : 0;
            const int expectedDamage = selected == weaponIndex ? 8 : 0;
            TEST_ASSERT_EQUAL(expectedAttack, getFighterWeaponAttackBonus(
                fighter, *trainedWeapons[weaponIndex]));
            TEST_ASSERT_EQUAL(expectedDamage, getFighterWeaponDamageBonus(
                fighter, *trainedWeapons[weaponIndex]));
        }

        TEST_ASSERT_EQUAL(0, getFighterWeaponAttackBonus(fighter, *bow));
        TEST_ASSERT_EQUAL(0, getFighterWeaponDamageBonus(fighter, *bow));
        TEST_ASSERT_EQUAL_UINT8(
            21 - (21 - trainedWeapons[selected]->criticalThreat) * 2,
            getWeaponCriticalThreatMinimum(
                fighter, *trainedWeapons[selected]));
        TEST_ASSERT_TRUE(fighterAutomaticallyConfirmsCritical(
            fighter, *trainedWeapons[selected]));
        TEST_ASSERT_FALSE(fighterAutomaticallyConfirmsCritical(fighter, *bow));
    }

    Character rogue = fighter;
    rogue.characterClass = CLASS_ROGUE;
    setFighterTrainedWeaponGroup(rogue, WEAPON_GROUP_AXES);
    TEST_ASSERT_EQUAL(WEAPON_GROUP_NONE, rogue.trainedWeaponGroup);
}

void test_character_creation_primary_bonus_is_assigned_once()
{
    const int rolls[6] = {18, 16, 14, 12, 10, 8};
    Character character = makeTestCharacter();

    assignAbilityScoresForClass(character, CLASS_FIGHTER, rolls);
    TEST_ASSERT_EQUAL_UINT8(20, character.abilities.strength);
    TEST_ASSERT_EQUAL_UINT8(16, character.abilities.constitution);
    TEST_ASSERT_EQUAL_UINT8(14, character.abilities.dexterity);
    TEST_ASSERT_EQUAL_UINT8(12, character.abilities.wisdom);
    TEST_ASSERT_EQUAL_UINT8(10, character.abilities.intelligence);
    TEST_ASSERT_EQUAL_UINT8(8, character.abilities.charisma);

    // Reassigning models a reroll: every score is overwritten before +2.
    assignAbilityScoresForClass(character, CLASS_FIGHTER, rolls);
    TEST_ASSERT_EQUAL_UINT8(20, character.abilities.strength);

    assignAbilityScoresForClass(character, CLASS_ROGUE, rolls);
    TEST_ASSERT_EQUAL_UINT8(20, character.abilities.dexterity);
    TEST_ASSERT_EQUAL_UINT8(18, character.abilities.intelligence);
    TEST_ASSERT_EQUAL_UINT8(16, character.abilities.wisdom);

    assignAbilityScoresForClass(character, CLASS_WIZARD, rolls);
    TEST_ASSERT_EQUAL_UINT8(20, character.abilities.intelligence);
    TEST_ASSERT_EQUAL_UINT8(18, character.abilities.dexterity);
    TEST_ASSERT_EQUAL_UINT8(16, character.abilities.wisdom);
    TEST_ASSERT_EQUAL_UINT8(14, character.abilities.constitution);

    assignAbilityScoresForClass(character, CLASS_CLERIC, rolls);
    TEST_ASSERT_EQUAL_UINT8(20, character.abilities.wisdom);
    TEST_ASSERT_EQUAL_UINT8(18, character.abilities.constitution);
    TEST_ASSERT_EQUAL_UINT8(16, character.abilities.strength);
}

void test_fighter_starting_weapon_mapping()
{
    TEST_ASSERT_EQUAL(ITEM_LONGSWORD,
        getFighterStartingMeleeWeapon(WEAPON_GROUP_BLADES));
    TEST_ASSERT_EQUAL(ITEM_BATTLEAXE,
        getFighterStartingMeleeWeapon(WEAPON_GROUP_AXES));
    TEST_ASSERT_EQUAL(ITEM_WARHAMMER,
        getFighterStartingMeleeWeapon(WEAPON_GROUP_BLUDGEONS));
    TEST_ASSERT_EQUAL(ITEM_SHORTBOW, getFighterStartingRangedWeapon());
}

void test_defeated_and_legacy_turned_monsters_are_lootable()
{
    Character character = makeTestCharacter();

    character.state = STATE_ALIVE;
    TEST_ASSERT_FALSE(isLootable(character));

    character.state = STATE_DEAD;
    TEST_ASSERT_TRUE(isLootable(character));

    character.state = STATE_TURNED;
    TEST_ASSERT_TRUE(isLootable(character));
}

void test_masterwork_and_numeric_enhancement_bonuses()
{
    const ItemInstance masterwork = makeMasterworkWeapon(ITEM_LONGSWORD);
    TEST_ASSERT_TRUE(hasWeaponProperty(
        masterwork, WEAPON_PROPERTY_MASTERWORK));
    TEST_ASSERT_EQUAL(1, getItemWeaponAttackBonus(masterwork));
    TEST_ASSERT_EQUAL(0, getItemWeaponDamageBonus(masterwork));

    ItemInstance magicMarkedMasterwork = makeMagicWeapon(
        ITEM_LONGSWORD, 1, WEAPON_PROPERTY_NONE);
    magicMarkedMasterwork.weaponProperties |= WEAPON_PROPERTY_MASTERWORK;
    TEST_ASSERT_EQUAL(1, getItemWeaponAttackBonus(magicMarkedMasterwork));
    TEST_ASSERT_EQUAL(1, getItemWeaponDamageBonus(magicMarkedMasterwork));
    TEST_ASSERT_FALSE(isValidItemInstance(magicMarkedMasterwork));

    const uint8_t bonuses[] = {1, 3, 5};
    for (uint8_t bonus : bonuses)
    {
        const ItemInstance magic = makeMagicWeapon(
            ITEM_LONGSWORD, bonus, WEAPON_PROPERTY_NONE);
        TEST_ASSERT_EQUAL(bonus, getItemWeaponAttackBonus(magic));
        TEST_ASSERT_EQUAL(bonus, getItemWeaponDamageBonus(magic));
        TEST_ASSERT_EQUAL_UINT8(bonus,
            getEffectiveEnhancementBonus(magic));
        TEST_ASSERT_FALSE(hasWeaponProperty(
            magic, WEAPON_PROPERTY_MASTERWORK));
    }
}

void test_magic_properties_effective_bonus_and_elemental_dice()
{
    const ItemInstance flaming = makeMagicWeapon(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_FLAMING);
    const ItemInstance keen = makeMagicWeapon(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_KEEN);
    const ItemInstance plusTwoFlaming = makeMagicWeapon(
        ITEM_LONGSWORD, 3, WEAPON_PROPERTY_FLAMING);
    const ItemInstance flamingKeen = makeMagicWeapon(
        ITEM_LONGSWORD, 4,
        WEAPON_PROPERTY_FLAMING | WEAPON_PROPERTY_KEEN);

    TEST_ASSERT_EQUAL_INT8(1, flaming.enhancementBonus);
    TEST_ASSERT_EQUAL_UINT8(2, getEffectiveEnhancementBonus(flaming));
    TEST_ASSERT_EQUAL_UINT8(2, getEffectiveEnhancementBonus(keen));
    TEST_ASSERT_EQUAL_INT8(2, plusTwoFlaming.enhancementBonus);
    TEST_ASSERT_EQUAL_UINT8(3,
        getEffectiveEnhancementBonus(plusTwoFlaming));
    TEST_ASSERT_EQUAL_INT8(2, flamingKeen.enhancementBonus);
    TEST_ASSERT_EQUAL_UINT8(4, getEffectiveEnhancementBonus(flamingKeen));

    TEST_ASSERT_EQUAL_UINT8(1,
        getWeaponElementalDamageDice(flaming, DAMAGE_FIRE));
    TEST_ASSERT_EQUAL_UINT8(0,
        getWeaponElementalDamageDice(flaming, DAMAGE_COLD));
    const ItemInstance frost = makeMagicWeapon(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_FROST);
    const ItemInstance shock = makeMagicWeapon(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_SHOCK);
    TEST_ASSERT_EQUAL_UINT8(1,
        getWeaponElementalDamageDice(frost, DAMAGE_COLD));
    TEST_ASSERT_EQUAL_UINT8(1,
        getWeaponElementalDamageDice(shock, DAMAGE_ELECTRIC));
}

void test_keen_doubles_natural_range_once_with_improved_critical()
{
    Character character = makeTestCharacter();
    const ItemInstance keenAxe = makeMagicWeapon(
        ITEM_BATTLEAXE, 2, WEAPON_PROPERTY_KEEN);
    const ItemInstance keenLongsword = makeMagicWeapon(
        ITEM_LONGSWORD, 2, WEAPON_PROPERTY_KEEN);
    const ItemInstance keenFalchion = makeMagicWeapon(
        ITEM_FALCHION, 2, WEAPON_PROPERTY_KEEN);
    TEST_ASSERT_EQUAL_UINT8(19, getWeaponCriticalThreatMinimum(
        character, *getWeapon(ITEM_BATTLEAXE), &keenAxe));
    TEST_ASSERT_EQUAL_UINT8(17, getWeaponCriticalThreatMinimum(
        character, *getWeapon(ITEM_LONGSWORD), &keenLongsword));
    TEST_ASSERT_EQUAL_UINT8(15, getWeaponCriticalThreatMinimum(
        character, *getWeapon(ITEM_FALCHION), &keenFalchion));

    character.characterClass = CLASS_FIGHTER;
    character.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    character.level = 10;
    TEST_ASSERT_EQUAL_UINT8(17, getWeaponCriticalThreatMinimum(
        character, *getWeapon(ITEM_LONGSWORD), &keenLongsword));

    const ItemInstance attemptedKeenHammer = makeMagicWeapon(
        ITEM_WARHAMMER, 2, WEAPON_PROPERTY_KEEN);
    TEST_ASSERT_FALSE(hasWeaponProperty(
        attemptedKeenHammer, WEAPON_PROPERTY_KEEN));
    TEST_ASSERT_EQUAL_INT8(2, attemptedKeenHammer.enhancementBonus);
    const ItemInstance attemptedKeenBow = makeMagicWeapon(
        ITEM_SHORTBOW, 2, WEAPON_PROPERTY_KEEN);
    TEST_ASSERT_FALSE(hasWeaponProperty(
        attemptedKeenBow, WEAPON_PROPERTY_KEEN));
}

void test_enhancement_stacks_with_fighter_training()
{
    Character fighter = makeTestCharacter();
    fighter.characterClass = CLASS_FIGHTER;
    fighter.trainedWeaponGroup = WEAPON_GROUP_BLADES;
    fighter.level = 5;
    fighter.equipment.equipped[SLOT_MELEE_WEAPON] = makeMagicWeapon(
        ITEM_LONGSWORD, 1, WEAPON_PROPERTY_NONE);
    controlledBaseAttackBonus = 5;

    TEST_ASSERT_EQUAL(8, getMeleeAttackBonus(fighter));
    TEST_ASSERT_EQUAL(3, getFighterWeaponDamageBonus(
        fighter, *getWeapon(ITEM_LONGSWORD)));
    TEST_ASSERT_EQUAL(1, getItemWeaponDamageBonus(
        fighter.equipment.equipped[SLOT_MELEE_WEAPON]));
    TEST_ASSERT_EQUAL_UINT8(2, getIterativeAttackCount(6));
    TEST_ASSERT_EQUAL_UINT8(2, getIterativeAttackCount(7));
    controlledBaseAttackBonus = 0;
}

void test_generated_item_names_show_complete_quality()
{
    char name[48];
    const ItemInstance item = makeMagicWeapon(
        ITEM_LONGSWORD, 4,
        WEAPON_PROPERTY_FLAMING | WEAPON_PROPERTY_KEEN);
    formatItemInstanceName(item, name, sizeof(name));
    TEST_ASSERT_EQUAL_STRING("+2 Flaming Keen Longsword", name);
    const ItemInstance masterwork = makeMasterworkWeapon(ITEM_LONGSWORD);
    formatItemInstanceName(masterwork, name, sizeof(name));
    TEST_ASSERT_EQUAL_STRING("Masterwork Longsword", name);
}

static LootQuality highestEligibleQuality(uint8_t level, LootSource source)
{
    const LootQualityWeights& weights = getLootQualityWeights(level, source);
    for (int quality = LOOT_QUALITY_COUNT - 1; quality >= 0; quality--)
    {
        if (weights.weights[quality] != 0)
            return static_cast<LootQuality>(quality);
    }
    return LOOT_QUALITY_MUNDANE;
}

void test_loot_level_gating_and_unlocks()
{
    for (uint8_t source = 0; source < LOOT_SOURCE_COUNT; source++)
    {
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MUNDANE,
            highestEligibleQuality(1, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MUNDANE,
            highestEligibleQuality(2, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MASTERWORK,
            highestEligibleQuality(3, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MASTERWORK,
            highestEligibleQuality(4, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_1,
            highestEligibleQuality(5, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_2,
            highestEligibleQuality(7, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_2,
            highestEligibleQuality(8, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_3,
            highestEligibleQuality(12, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_4,
            highestEligibleQuality(16, static_cast<LootSource>(source)));
        TEST_ASSERT_EQUAL(LOOT_QUALITY_MAGIC_5,
            highestEligibleQuality(20, static_cast<LootSource>(source)));
    }
}

void test_loot_quality_rows_are_exact_percentages()
{
    const uint8_t representativeLevels[] = {1, 3, 4, 5, 7, 8, 12, 16, 20};
    for (uint8_t level : representativeLevels)
    {
        for (uint8_t source = 0; source < LOOT_SOURCE_COUNT; source++)
        {
            const LootQualityWeights& weights = getLootQualityWeights(
                level, static_cast<LootSource>(source));
            uint16_t total = 0;
            for (uint8_t quality = 0; quality < LOOT_QUALITY_COUNT; quality++)
                total += weights.weights[quality];
            TEST_ASSERT_EQUAL_UINT16(100, total);
        }
    }

    TEST_ASSERT_EQUAL(LOOT_QUALITY_MUNDANE,
        selectLootQuality(3, LOOT_SOURCE_NORMAL_MONSTER, 94));
    TEST_ASSERT_EQUAL(LOOT_QUALITY_MASTERWORK,
        selectLootQuality(3, LOOT_SOURCE_NORMAL_MONSTER, 95));
    TEST_ASSERT_TRUE(getEquipmentDropChance(LOOT_SOURCE_NORMAL_MONSTER) <
        getEquipmentDropChance(LOOT_SOURCE_CHEST));
    TEST_ASSERT_TRUE(getEquipmentDropChance(LOOT_SOURCE_CHEST) <
        getEquipmentDropChance(LOOT_SOURCE_BOSS));
    TEST_ASSERT_TRUE(getEquipmentDropChance(LOOT_SOURCE_BOSS) <
        getEquipmentDropChance(LOOT_SOURCE_FINAL_TREASURE));
}

void test_loot_builder_never_exceeds_effective_budget()
{
    for (uint8_t budget = 1; budget <= 5; budget++)
    {
        const LootQuality quality = static_cast<LootQuality>(
            LOOT_QUALITY_MAGIC_1 + budget - 1);
        const ItemInstance weapon = createLootEquipment(
            ITEM_LONGSWORD, quality,
            WEAPON_PROPERTY_FLAMING | WEAPON_PROPERTY_FROST |
            WEAPON_PROPERTY_SHOCK | WEAPON_PROPERTY_KEEN);
        TEST_ASSERT_EQUAL_UINT8(budget,
            getEffectiveEnhancementBonus(weapon));
        TEST_ASSERT_TRUE(weapon.enhancementBonus >= 1);
        TEST_ASSERT_TRUE(weapon.enhancementBonus <= budget);
        TEST_ASSERT_TRUE(isValidItemInstance(weapon));
    }

    const ItemInstance masterwork = createLootEquipment(
        ITEM_LONGSWORD, LOOT_QUALITY_MASTERWORK);
    TEST_ASSERT_EQUAL(1, getItemWeaponAttackBonus(masterwork));
    TEST_ASSERT_EQUAL(0, getItemWeaponDamageBonus(masterwork));
    TEST_ASSERT_EQUAL_UINT8(0, getEffectiveEnhancementBonus(masterwork));

    const ItemInstance armor = createLootEquipment(
        ITEM_CHAINMAIL, LOOT_QUALITY_MAGIC_3);
    TEST_ASSERT_EQUAL_INT8(3, armor.enhancementBonus);
    TEST_ASSERT_EQUAL_UINT8(0, armor.weaponProperties);
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
    RUN_TEST(test_locked_fighter_weapon_progression);
    RUN_TEST(test_fighter_toughness_and_critical_progression);
    RUN_TEST(test_fighter_bonuses_flow_through_shared_attack_helpers);
    RUN_TEST(test_power_attack_scaling_remains_compatible_with_iteratives);
    RUN_TEST(test_fighter_selected_groups_control_progression_bonuses);
    RUN_TEST(test_character_creation_primary_bonus_is_assigned_once);
    RUN_TEST(test_fighter_starting_weapon_mapping);
    RUN_TEST(test_defeated_and_legacy_turned_monsters_are_lootable);
    RUN_TEST(test_masterwork_and_numeric_enhancement_bonuses);
    RUN_TEST(test_magic_properties_effective_bonus_and_elemental_dice);
    RUN_TEST(test_keen_doubles_natural_range_once_with_improved_critical);
    RUN_TEST(test_enhancement_stacks_with_fighter_training);
    RUN_TEST(test_generated_item_names_show_complete_quality);
    RUN_TEST(test_loot_level_gating_and_unlocks);
    RUN_TEST(test_loot_quality_rows_are_exact_percentages);
    RUN_TEST(test_loot_builder_never_exceeds_effective_budget);
    UNITY_END();
}

void loop() {}
