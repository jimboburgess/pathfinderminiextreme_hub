#include <Arduino.h>
#include <unity.h>

#include "../../src/characters/characters.h"
#include "../../src/characters/items.h"
#include "../../src/data/progression.h"

// Match the small production dependencies used by progression.cpp, following
// the existing progression test suite's compile-directly pattern.
int getAbilityModifier(int score)
{
    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

int getAbilityModifier(const Character& character, AbilityScore ability)
{
    switch (ability)
    {
        case ABILITY_STRENGTH:
            return getAbilityModifier(character.abilities.strength);
        case ABILITY_DEXTERITY:
            return getAbilityModifier(character.abilities.dexterity);
        case ABILITY_CONSTITUTION:
            return getAbilityModifier(character.abilities.constitution);
        case ABILITY_INTELLIGENCE:
            return getAbilityModifier(character.abilities.intelligence);
        case ABILITY_WISDOM:
            return getAbilityModifier(character.abilities.wisdom);
        case ABILITY_CHARISMA:
            return getAbilityModifier(character.abilities.charisma);
    }

    return 0;
}

int getMaxHP(const Character& character)
{
    return getBaseHitPoints(character.characterClass, character.level) +
           getAbilityModifier(character, ABILITY_CONSTITUTION);
}

#include "../../src/characters/abilities.cpp"
#include "../../src/data/progression.cpp"

static bool forceRemoveFailure = false;

bool hasItem(const Character& character, const ItemInstance& item)
{
    for (uint8_t i = 0; i < character.inventory.itemCount; i++)
    {
        const InventorySlot& slot = character.inventory.slots[i];
        if (slot.item == item && slot.quantity > 0)
            return true;
    }

    return false;
}

bool removeItem(
    Character& character,
    const ItemInstance& item,
    uint8_t quantity)
{
    if (forceRemoveFailure || quantity != 1)
        return false;

    for (uint8_t i = 0; i < character.inventory.itemCount; i++)
    {
        InventorySlot& slot = character.inventory.slots[i];
        if (slot.item != item || slot.quantity == 0)
            continue;

        slot.quantity--;
        if (slot.quantity > 0)
            return true;

        for (uint8_t j = i;
             j + 1 < character.inventory.itemCount;
             j++)
        {
            character.inventory.slots[j] =
                character.inventory.slots[j + 1];
        }

        character.inventory.itemCount--;
        character.inventory.slots[
            character.inventory.itemCount] = InventorySlot{};
        return true;
    }

    return false;
}

#include "../../src/characters/items.cpp"

static Character makeWizard(bool initializeMagic = true)
{
    Character wizard = {};
    wizard.characterClass = CLASS_WIZARD;
    wizard.level = 1;
    wizard.abilities = {10, 10, 10, 10, 10, 10};

    if (initializeMagic)
        initializeCharacterMagic(wizard);
    else
        wizard.magic.arcaneCaster = true;

    return wizard;
}

static void giveScroll(
    Character& character,
    ItemID itemID,
    uint8_t quantity = 1)
{
    character.inventory.itemCount = 1;
    character.inventory.slots[0].item = makeItemInstance(itemID);
    character.inventory.slots[0].quantity = quantity;
}

void test_wizard_starting_spellbook_is_idempotent()
{
    Character wizard = makeWizard();

    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_GREASE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_COLOR_SPRAY));

    refreshCharacterMagicProgression(wizard);
    refreshCharacterMagicProgression(wizard);
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
}

void test_scroll_items_reference_the_expected_abilities()
{
    const Scroll* magicMissile = getScroll(ITEM_SCROLL_MAGIC_MISSILE);
    const Scroll* sleep = getScroll(ITEM_SCROLL_SLEEP);
    const Scroll* grease = getScroll(ITEM_SCROLL_GREASE);

    TEST_ASSERT_NOT_NULL(magicMissile);
    TEST_ASSERT_NOT_NULL(sleep);
    TEST_ASSERT_NOT_NULL(grease);
    TEST_ASSERT_EQUAL(ABILITY_MAGIC_MISSILE, magicMissile->taughtAbility);
    TEST_ASSERT_EQUAL(ABILITY_SLEEP, sleep->taughtAbility);
    TEST_ASSERT_EQUAL(ABILITY_GREASE, grease->taughtAbility);
    TEST_ASSERT_NULL(getScroll(ITEM_POTION_CURE_LIGHT_WOUNDS));
}

void test_successful_scroll_learning_consumes_exactly_one()
{
    Character wizard = makeWizard(false);
    giveScroll(wizard, ITEM_SCROLL_SLEEP, 2);
    ItemInstance selected = wizard.inventory.slots[0].item;

    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_SUCCESS,
        useSpellScroll(wizard, selected));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SLEEP));
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.itemCount);
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.slots[0].quantity);
}

void test_duplicate_learning_preserves_scroll_and_spellbook()
{
    Character wizard = makeWizard();
    giveScroll(wizard, ITEM_SCROLL_SLEEP);
    ItemInstance selected = wizard.inventory.slots[0].item;

    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_ALREADY_KNOWN,
        useSpellScroll(wizard, selected));
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.itemCount);
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.slots[0].quantity);
}

void test_non_wizard_cannot_learn_and_keeps_scroll()
{
    Character fighter = {};
    fighter.characterClass = CLASS_FIGHTER;
    fighter.level = 1;
    giveScroll(fighter, ITEM_SCROLL_GREASE);
    ItemInstance selected = fighter.inventory.slots[0].item;

    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_NOT_ARCANE_CASTER,
        useSpellScroll(fighter, selected));
    TEST_ASSERT_FALSE(knowsAbility(fighter, ABILITY_GREASE));
    TEST_ASSERT_EQUAL_UINT8(1, fighter.inventory.slots[0].quantity);
}

void test_full_spellbook_rejects_learning_without_consumption()
{
    Character wizard = makeWizard(false);

    for (int rawAbility = ABILITY_NONE + 1;
         rawAbility < ABILITY_MAX &&
         wizard.magic.knownAbilityCount < MAX_KNOWN_ABILITIES;
         rawAbility++)
    {
        AbilityID abilityID = static_cast<AbilityID>(rawAbility);
        if (abilityID != ABILITY_GREASE)
        {
            wizard.magic.knownAbilities[
                wizard.magic.knownAbilityCount++] = abilityID;
        }
    }

    TEST_ASSERT_EQUAL_UINT8(
        MAX_KNOWN_ABILITIES, wizard.magic.knownAbilityCount);
    giveScroll(wizard, ITEM_SCROLL_GREASE);
    ItemInstance selected = wizard.inventory.slots[0].item;

    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_SPELLBOOK_FULL,
        useSpellScroll(wizard, selected));
    TEST_ASSERT_FALSE(knowsAbility(wizard, ABILITY_GREASE));
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.slots[0].quantity);
}

void test_invalid_scroll_and_failed_removal_do_not_change_character()
{
    Character wizard = makeWizard(false);
    giveScroll(wizard, ITEM_TORCH);

    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_INVALID_SCROLL,
        useSpellScroll(wizard, wizard.inventory.slots[0].item));
    TEST_ASSERT_EQUAL_UINT8(0, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.slots[0].quantity);

    giveScroll(wizard, ITEM_SCROLL_MAGIC_MISSILE);
    forceRemoveFailure = true;
    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_INVALID_SCROLL,
        useSpellScroll(wizard, wizard.inventory.slots[0].item));
    forceRemoveFailure = false;

    TEST_ASSERT_FALSE(knowsAbility(wizard, ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_EQUAL_UINT8(1, wizard.inventory.slots[0].quantity);
}

void test_scroll_learned_spell_survives_magic_restore()
{
    Character wizard = makeWizard();
    TEST_ASSERT_EQUAL(
        SCROLL_LEARN_SUCCESS,
        learnSpellFromScroll(wizard, ABILITY_ACID_ARROW));

    AbilityID savedAbilities[MAX_KNOWN_ABILITIES] = {};
    for (uint8_t i = 0; i < wizard.magic.knownAbilityCount; i++)
        savedAbilities[i] = wizard.magic.knownAbilities[i];

    Character loaded = makeWizard(false);
    restoreCharacterMagic(
        loaded,
        4,
        savedAbilities,
        wizard.magic.knownAbilityCount);

    TEST_ASSERT_EQUAL_UINT8(5, loaded.magic.knownAbilityCount);
    TEST_ASSERT_TRUE(knowsAbility(loaded, ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(knowsAbility(loaded, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(loaded, ABILITY_GREASE));
    TEST_ASSERT_TRUE(knowsAbility(loaded, ABILITY_COLOR_SPRAY));
    TEST_ASSERT_TRUE(knowsAbility(loaded, ABILITY_ACID_ARROW));

    refreshCharacterMagicProgression(loaded);
    TEST_ASSERT_EQUAL_UINT8(5, loaded.magic.knownAbilityCount);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_wizard_starting_spellbook_is_idempotent);
    RUN_TEST(test_scroll_items_reference_the_expected_abilities);
    RUN_TEST(test_successful_scroll_learning_consumes_exactly_one);
    RUN_TEST(test_duplicate_learning_preserves_scroll_and_spellbook);
    RUN_TEST(test_non_wizard_cannot_learn_and_keeps_scroll);
    RUN_TEST(test_full_spellbook_rejects_learning_without_consumption);
    RUN_TEST(test_invalid_scroll_and_failed_removal_do_not_change_character);
    RUN_TEST(test_scroll_learned_spell_survives_magic_restore);
    UNITY_END();
}

void loop()
{
}
