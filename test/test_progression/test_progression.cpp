#include <Arduino.h>
#include <unity.h>

#include "../../src/characters/characters.h"
#include "../../src/data/progression.h"

// The project currently has only embedded Unity suites. Build the progression
// implementation directly and provide its small character-rule dependencies,
// matching the production calculations in characters.cpp.
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

static Character makeCharacter(
    CharacterClass characterClass,
    uint8_t level = 1,
    uint32_t xp = 0)
{
    Character character = {};
    character.characterClass = characterClass;
    character.level = level;
    character.xp = xp;
    character.state = STATE_ALIVE;
    character.team = TEAM_PLAYER;
    character.abilities = {10, 10, 10, 10, 10, 10};
    character.health.maxHP = getMaxHP(character);
    character.health.currentHP = character.health.maxHP;
    return character;
}

static uint8_t getPrimaryAbilityScore(const Character& character)
{
    switch (character.characterClass)
    {
        case CLASS_FIGHTER:
            return character.abilities.strength;
        case CLASS_ROGUE:
            return character.abilities.dexterity;
        case CLASS_WIZARD:
            return character.abilities.intelligence;
        case CLASS_CLERIC:
            return character.abilities.wisdom;
    }

    return 0;
}

void test_wizard_mp_progression_and_level_one_magic()
{
    const int expectedMaxMP[MAX_CHARACTER_LEVEL] =
    {
          6,   8,  11,  14,  18,
         22,  27,  32,  38,  44,
         51,  58,  66,  74,  83,
         92, 102, 112, 123, 134
    };

    for (uint8_t level = 1; level <= MAX_CHARACTER_LEVEL; level++)
    {
        Character wizard = makeCharacter(CLASS_WIZARD, level);
        TEST_ASSERT_EQUAL_INT(
            expectedMaxMP[level - 1], getMaxMPForCharacter(wizard));
    }

    Character wizard = makeCharacter(CLASS_WIZARD);
    initializeCharacterMagic(wizard);

    TEST_ASSERT_EQUAL_INT(6, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(6, wizard.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.magic.arcaneCaster);
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_GREASE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_COLOR_SPRAY));
    TEST_ASSERT_FALSE(knowsAbility(wizard, ABILITY_RAY_OF_FROST));
    TEST_ASSERT_FALSE(knowsAbility(wizard, ABILITY_ACID_ARROW));

    Character intelligentWizard = makeCharacter(CLASS_WIZARD);
    intelligentWizard.abilities.intelligence = 18;
    initializeCharacterMagic(intelligentWizard);

    TEST_ASSERT_EQUAL_INT(10, intelligentWizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(10, intelligentWizard.magic.currentMP);

    Character intelligentFighter = makeCharacter(CLASS_FIGHTER);
    intelligentFighter.abilities.intelligence = 18;
    TEST_ASSERT_EQUAL_INT(0, getMaxMPForCharacter(intelligentFighter));
}

void test_wizard_level_up_preserves_mp_and_learns_idempotently()
{
    Character wizard = makeCharacter(CLASS_WIZARD, 1, 1999);
    initializeCharacterMagic(wizard);
    wizard.magic.currentMP = 2;

    TEST_ASSERT_EQUAL_UINT8(1, awardExperience(wizard, 1));
    TEST_ASSERT_EQUAL_UINT8(2, wizard.level);
    TEST_ASSERT_EQUAL_INT(8, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(2, wizard.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);

    TEST_ASSERT_EQUAL_UINT8(1, awardExperience(wizard, 3000));
    TEST_ASSERT_EQUAL_UINT8(3, wizard.level);
    TEST_ASSERT_EQUAL_INT(11, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(2, wizard.magic.currentMP);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_ACID_ARROW));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SCORCHING_RAY));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_WEB));
    TEST_ASSERT_EQUAL_UINT8(7, wizard.magic.knownAbilityCount);

    TEST_ASSERT_EQUAL_UINT8(4, awardExperience(wizard, 30000));
    TEST_ASSERT_EQUAL_UINT8(7, wizard.level);
    TEST_ASSERT_EQUAL_INT(27, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(2, wizard.magic.currentMP);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_FIREBALL));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_LIGHTNING_BOLT));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_HASTE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_ICE_STORM));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_GREATER_INVISIBILITY));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_STONESKIN));
    TEST_ASSERT_EQUAL_UINT8(13, wizard.magic.knownAbilityCount);

    refreshCharacterMagicProgression(wizard);
    refreshCharacterMagicProgression(wizard);
    TEST_ASSERT_EQUAL_UINT8(13, wizard.magic.knownAbilityCount);
}

void test_wizard_int_milestone_updates_max_mp_without_refilling()
{
    Character wizard = makeCharacter(CLASS_WIZARD, 3, 8999);
    wizard.abilities.intelligence = 17;
    initializeCharacterMagic(wizard);

    TEST_ASSERT_EQUAL_INT(14, wizard.magic.maxMP);
    wizard.magic.currentMP = 3;

    TEST_ASSERT_EQUAL_UINT8(1, awardExperience(wizard, 1));
    TEST_ASSERT_EQUAL_UINT8(4, wizard.level);
    TEST_ASSERT_EQUAL_UINT8(18, wizard.abilities.intelligence);
    TEST_ASSERT_EQUAL_INT(18, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(3, wizard.magic.currentMP);

    refreshCharacterMagicProgression(wizard);
    TEST_ASSERT_EQUAL_INT(18, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(3, wizard.magic.currentMP);
}

void test_cleric_mp_progression_and_initialization()
{
    const int expectedBaseMP[MAX_CHARACTER_LEVEL] =
    {
          6,   8,  11,  14,  18,
         22,  27,  32,  38,  44,
         51,  58,  66,  74,  83,
         92, 102, 112, 123, 134
    };

    for (uint8_t level = 1; level <= MAX_CHARACTER_LEVEL; level++)
    {
        Character cleric = makeCharacter(CLASS_CLERIC, level);
        TEST_ASSERT_EQUAL_INT(
            expectedBaseMP[level - 1], getMaxMPForCharacter(cleric));
    }

    Character wiseCleric = makeCharacter(CLASS_CLERIC);
    wiseCleric.abilities.wisdom = 18;
    initializeCharacterMagic(wiseCleric);

    TEST_ASSERT_TRUE(wiseCleric.magic.divineCaster);
    TEST_ASSERT_FALSE(wiseCleric.magic.arcaneCaster);
    TEST_ASSERT_EQUAL_INT(10, wiseCleric.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(10, wiseCleric.magic.currentMP);
}

void test_cleric_level_up_and_load_preserve_spent_mp()
{
    Character cleric = makeCharacter(CLASS_CLERIC, 3, 8999);
    cleric.abilities.wisdom = 17;
    initializeCharacterMagic(cleric);

    TEST_ASSERT_EQUAL_INT(14, cleric.magic.maxMP);
    cleric.magic.currentMP = 3;

    TEST_ASSERT_EQUAL_UINT8(1, awardExperience(cleric, 1));
    TEST_ASSERT_EQUAL_UINT8(4, cleric.level);
    TEST_ASSERT_EQUAL_UINT8(18, cleric.abilities.wisdom);
    TEST_ASSERT_EQUAL_INT(18, cleric.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(3, cleric.magic.currentMP);

    Character loadedCleric = makeCharacter(CLASS_CLERIC, 3);
    loadedCleric.abilities.wisdom = 18;
    restoreCharacterMagic(loadedCleric, 3);
    TEST_ASSERT_TRUE(loadedCleric.magic.divineCaster);
    TEST_ASSERT_EQUAL_INT(15, loadedCleric.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(3, loadedCleric.magic.currentMP);

    restoreCharacterMagic(loadedCleric, 999);
    TEST_ASSERT_EQUAL_INT(15, loadedCleric.magic.currentMP);
}

void test_loaded_wizard_mp_is_preserved_and_clamped()
{
    Character wizard = makeCharacter(CLASS_WIZARD, 3);
    wizard.abilities.intelligence = 18;

    restoreCharacterMagic(wizard, 3);
    TEST_ASSERT_EQUAL_INT(15, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(3, wizard.magic.currentMP);
    TEST_ASSERT_EQUAL_INT(
        3, clampCurrentMPForCharacter(wizard, wizard.magic.currentMP));
    TEST_ASSERT_EQUAL_UINT8(7, wizard.magic.knownAbilityCount);

    restoreCharacterMagic(wizard, 999);
    TEST_ASSERT_EQUAL_INT(15, wizard.magic.currentMP);

    restoreCharacterMagic(wizard, -25);
    TEST_ASSERT_EQUAL_INT(0, wizard.magic.currentMP);
}

void test_loaded_known_abilities_merge_with_progression_without_duplicates()
{
    Character savedWizard = makeCharacter(CLASS_WIZARD, 1);
    initializeCharacterMagic(savedWizard);
    TEST_ASSERT_TRUE(learnAbility(savedWizard, ABILITY_ACID_ARROW));

    AbilityID savedAbilities[MAX_KNOWN_ABILITIES] = {};
    for (uint8_t i = 0; i < savedWizard.magic.knownAbilityCount; i++)
        savedAbilities[i] = savedWizard.magic.knownAbilities[i];

    Character loadedWizard = makeCharacter(CLASS_WIZARD, 1);
    restoreCharacterMagic(
        loadedWizard,
        3,
        savedAbilities,
        savedWizard.magic.knownAbilityCount);

    TEST_ASSERT_EQUAL_INT(3, loadedWizard.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(5, loadedWizard.magic.knownAbilityCount);
    TEST_ASSERT_TRUE(knowsAbility(loadedWizard, ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(knowsAbility(loadedWizard, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(loadedWizard, ABILITY_GREASE));
    TEST_ASSERT_TRUE(knowsAbility(loadedWizard, ABILITY_COLOR_SPRAY));
    TEST_ASSERT_TRUE(knowsAbility(loadedWizard, ABILITY_ACID_ARROW));

    refreshCharacterMagicProgression(loadedWizard);
    TEST_ASSERT_EQUAL_UINT8(5, loadedWizard.magic.knownAbilityCount);
}

void test_medium_xp_threshold_boundaries()
{
    TEST_ASSERT_EQUAL_UINT8(1, getLevelForExperience(0));
    TEST_ASSERT_EQUAL_UINT8(1, getLevelForExperience(1999));
    TEST_ASSERT_EQUAL_UINT8(2, getLevelForExperience(2000));
    TEST_ASSERT_EQUAL_UINT8(2, getLevelForExperience(4999));
    TEST_ASSERT_EQUAL_UINT8(3, getLevelForExperience(5000));
    TEST_ASSERT_EQUAL_UINT8(19, getLevelForExperience(3599999));
    TEST_ASSERT_EQUAL_UINT8(20, getLevelForExperience(3600000));
    TEST_ASSERT_EQUAL_UINT8(20, getLevelForExperience(UINT32_MAX));
}

void test_one_award_can_advance_multiple_levels_without_healing()
{
    Character fighter = makeCharacter(CLASS_FIGHTER, 1, 1500);
    fighter.abilities.strength = 16;
    fighter.abilities.constitution = 14;
    fighter.health.maxHP = getMaxHP(fighter);
    fighter.health.currentHP = 7;
    fighter.inventory.gold = 321;
    fighter.magic.knownAbilities[0] = ABILITY_POWER_ATTACK;
    fighter.magic.knownAbilityCount = 1;
    fighter.equipment.equipped[SLOT_MELEE_WEAPON].itemID = ITEM_LONGSWORD;

    TEST_ASSERT_EQUAL_UINT8(2, awardExperience(fighter, 4000));
    TEST_ASSERT_EQUAL_UINT32(5500, fighter.xp);
    TEST_ASSERT_EQUAL_UINT8(3, fighter.level);
    TEST_ASSERT_EQUAL_UINT8(16, fighter.abilities.strength);
    TEST_ASSERT_EQUAL_INT(23, fighter.health.maxHP);
    TEST_ASSERT_EQUAL_INT(7, fighter.health.currentHP);

    TEST_ASSERT_EQUAL_UINT32(321, fighter.inventory.gold);
    TEST_ASSERT_EQUAL_UINT8(1, fighter.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL(ABILITY_POWER_ATTACK,
                      fighter.magic.knownAbilities[0]);
    TEST_ASSERT_EQUAL(ITEM_LONGSWORD,
                      fighter.equipment.equipped[SLOT_MELEE_WEAPON].itemID);
    TEST_ASSERT_EQUAL(STATE_ALIVE, fighter.state);
}

void test_primary_ability_milestones_apply_once_for_every_class()
{
    const CharacterClass classes[] =
    {
        CLASS_FIGHTER,
        CLASS_ROGUE,
        CLASS_WIZARD,
        CLASS_CLERIC
    };

    for (uint8_t i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
    {
        Character character = makeCharacter(classes[i]);
        character.health.currentHP = 4;

        TEST_ASSERT_EQUAL_UINT8(
            19, awardExperience(character, 3600000));
        TEST_ASSERT_EQUAL_UINT8(20, character.level);
        TEST_ASSERT_EQUAL_UINT8(15, getPrimaryAbilityScore(character));
        TEST_ASSERT_EQUAL_INT(4, character.health.currentHP);
        TEST_ASSERT_EQUAL_INT(
            getBaseHitPoints(classes[i], 20), character.health.maxHP);

        TEST_ASSERT_EQUAL_UINT8(0, awardExperience(character, 500));
        TEST_ASSERT_EQUAL_UINT8(15, getPrimaryAbilityScore(character));
        TEST_ASSERT_EQUAL_UINT32(3600500, character.xp);
    }
}

void test_new_level_immediately_selects_existing_class_progression()
{
    Character fighter = makeCharacter(CLASS_FIGHTER, 1, 1999);

    TEST_ASSERT_EQUAL_UINT8(1, awardExperience(fighter, 1));
    TEST_ASSERT_EQUAL_UINT8(2, fighter.level);
    TEST_ASSERT_EQUAL_INT(2, getBaseAttackBonus(
        fighter.characterClass, fighter.level));
    TEST_ASSERT_EQUAL_INT(3, getFortitudeSave(
        fighter.characterClass, fighter.level));
    TEST_ASSERT_EQUAL_INT(0, getReflexSave(
        fighter.characterClass, fighter.level));
    TEST_ASSERT_EQUAL_INT(0, getWillSave(
        fighter.characterClass, fighter.level));
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_wizard_mp_progression_and_level_one_magic);
    RUN_TEST(test_wizard_level_up_preserves_mp_and_learns_idempotently);
    RUN_TEST(test_wizard_int_milestone_updates_max_mp_without_refilling);
    RUN_TEST(test_cleric_mp_progression_and_initialization);
    RUN_TEST(test_cleric_level_up_and_load_preserve_spent_mp);
    RUN_TEST(test_loaded_wizard_mp_is_preserved_and_clamped);
    RUN_TEST(
        test_loaded_known_abilities_merge_with_progression_without_duplicates);
    RUN_TEST(test_medium_xp_threshold_boundaries);
    RUN_TEST(test_one_award_can_advance_multiple_levels_without_healing);
    RUN_TEST(test_primary_ability_milestones_apply_once_for_every_class);
    RUN_TEST(test_new_level_immediately_selects_existing_class_progression);
    UNITY_END();
}

void loop()
{
}
