#include "characters/charactercreation.h"
#include "characters/creation_state.h"
#include "characters/combat_rules.h"
#include "data/progression.h"
#include "data/savegame.h"
#include "data/game.h"
#include "graphics/charcreationscreen.h"
#include "graphics/messagelog.h"
#include "runtime.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void setEnvironment(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value == nullptr ? "" : value);
#else
    if (value == nullptr)
        unsetenv(name);
    else
        setenv(name, value, 1);
#endif
}

Character makeCharacter(const char* name, CharacterClass characterClass,
                        WeaponGroup weaponGroup = WEAPON_GROUP_NONE)
{
    Character character{};
    createCharacter(character, characterClass, weaponGroup);
    setCharacterName(character, name);
    return character;
}

void wait(unsigned milliseconds)
{
    while (milliseconds != 0)
    {
        const unsigned step = milliseconds < 10 ? milliseconds : 10;
        pfme::runtime::advance(step);
        milliseconds -= step;
    }
}

void tap(const char* action)
{
    check(pfme::runtime::input(action, true), "input down");
    wait(30);
    check(pfme::runtime::input(action, false), "input up");
    wait(310);
}

uint8_t primaryAbility(const Character& character)
{
    switch (character.characterClass)
    {
        case CLASS_FIGHTER: return character.abilities.strength;
        case CLASS_ROGUE: return character.abilities.dexterity;
        case CLASS_WIZARD: return character.abilities.intelligence;
        case CLASS_CLERIC: return character.abilities.wisdom;
    }
    return 0;
}

uint8_t milestoneIncreases(uint8_t level)
{
    return level / 4;
}

void validateProgression(CharacterClass characterClass, const char* name,
                         uint8_t expectedLevel, WeaponGroup weaponGroup)
{
    Character character = makeCharacter(name, characterClass, weaponGroup);
    const uint8_t originalPrimary = primaryAbility(character);
    check(applyTestCharacterTitle(character, true) == expectedLevel,
          "title level applied");
    check(std::strcmp(getCharacterName(character), name) == 0,
          "original name capitalization preserved");
    check(character.level == expectedLevel &&
          character.xp == getExperienceForLevel(expectedLevel),
          "exact Medium XP threshold");
    check(primaryAbility(character) ==
              originalPrimary + milestoneIncreases(expectedLevel),
          "normal ability milestones crossed");
    check(character.health.maxHP == getMaxHP(character) &&
          character.health.currentHP == character.health.maxHP,
          "HP progression restored");
    check(character.magic.maxMP == getMaxMPForCharacter(character) &&
          character.magic.currentMP == character.magic.maxMP,
          "MP progression restored");
    check(getBaseAttackBonus(characterClass, expectedLevel) ==
              calculateBaseAttackBonus(characterClass, expectedLevel),
          "BAB uses production progression");
    check(getSkillBonus(character, SKILL_PERCEPTION) ==
              getSkillRank(getSkillAptitude(characterClass, SKILL_PERCEPTION),
                           expectedLevel) +
              getAbilityModifier(character, ABILITY_WISDOM),
          "skill progression uses production level");
    check(getFortitudeSave(character) ==
              getBaseSave(characterClass, SAVE_FORTITUDE, expectedLevel) +
              getAbilityModifier(character, ABILITY_CONSTITUTION) &&
          getReflexSave(character) ==
              getBaseSave(characterClass, SAVE_REFLEX, expectedLevel) +
              getAbilityModifier(character, ABILITY_DEXTERITY) &&
          getWillSave(character) ==
              getBaseSave(characterClass, SAVE_WILL, expectedLevel) +
              getAbilityModifier(character, ABILITY_WISDOM),
          "saves use class progression and abilities");
    check(character.inventory.itemCount <= MAX_INVENTORY,
          "equipment package respects capacity");
}
}

int main()
{
    const std::filesystem::path saveDirectory =
        std::filesystem::temp_directory_path() / "pfme-test-characters";
    try
    {
        check(getTestCharacterStartingLevel("Sir Aric", true) == 5,
              "Sir mapping");
        check(getTestCharacterStartingLevel("Earl Mira", true) == 10,
              "Earl mapping");
        check(getTestCharacterStartingLevel("Duke Shade", true) == 15,
              "Duke mapping");
        check(getTestCharacterStartingLevel("King Ezren", true) == 20,
              "King mapping");
        check(getTestCharacterStartingLevel("sir aric", true) == 5 &&
              getTestCharacterStartingLevel("SIR ARIC", true) == 5 &&
              getTestCharacterStartingLevel("eArL Mira", true) == 10,
              "mixed-case mappings");
        for (const char* name : {"Aric", "Sirena", "Kingston", "Dukeman",
                                 "Earlington", "Sir", "Sir ", "Sir  Aric"})
        {
            check(getTestCharacterStartingLevel(name, true) == 1,
                  "ordinary partial or empty prefix rejected");
        }
        check(getTestCharacterStartingLevel("King 1234567890", true) == 20,
              "maximum-length titled name");
        check(getTestCharacterStartingLevel("King Ezren", false) == 1,
              "disabled mapping");

        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", nullptr);
        check(!testCharactersEnabled(), "native setting defaults disabled");
        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", "false");
        check(!testCharactersEnabled(), "false setting disabled");
        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", "TrUe");
        check(testCharactersEnabled(), "true setting enabled case-insensitively");

        validateProgression(CLASS_FIGHTER, "Sir Aric", 5,
                            WEAPON_GROUP_BLADES);
        validateProgression(CLASS_CLERIC, "Earl Mira", 10,
                            WEAPON_GROUP_NONE);
        validateProgression(CLASS_ROGUE, "Duke Shade", 15,
                            WEAPON_GROUP_NONE);
        validateProgression(CLASS_WIZARD, "King Ezren", 20,
                            WEAPON_GROUP_NONE);

        Character fighter = makeCharacter(
            "King Aric", CLASS_FIGHTER, WEAPON_GROUP_BLADES);
        check(applyTestCharacterTitle(fighter, true) == 20,
              "level-20 Fighter created");
        check(hasFighterFeature(fighter, FIGHTER_WEAPON_SPECIALIZATION) &&
              hasFighterFeature(fighter, FIGHTER_WEAPON_MASTERY) &&
              getFighterTrainedWeaponGroup(fighter) == WEAPON_GROUP_BLADES,
              "Fighter specialization and mastery valid");
        check(fighter.equipment.equipped[SLOT_MELEE_WEAPON].enhancementBonus == 5 &&
              fighter.equipment.equipped[SLOT_ARMOR].itemID == ITEM_FULL_PLATE &&
              fighter.equipment.equipped[SLOT_ARMOR].enhancementBonus == 5 &&
              getItemQuantity(fighter.inventory,
                  ITEM_POTION_CURE_CRITICAL_WOUNDS) == 4,
              "level-20 Fighter package");

        Character rogue = makeCharacter("Duke Shade", CLASS_ROGUE);
        check(applyTestCharacterTitle(rogue, true) == 15 &&
              getSneakAttackDice(rogue) == 8 &&
              rogue.equipment.equipped[SLOT_ARMOR].itemID == ITEM_CHAIN_SHIRT &&
              getItemQuantity(rogue.inventory,
                  ITEM_MASTERWORK_THIEVES_TOOLS) == 1,
              "Rogue features and package");

        Character wizard = makeCharacter("King Ezren", CLASS_WIZARD);
        check(applyTestCharacterTitle(wizard, true) == 20 &&
              knowsAbility(wizard, ABILITY_FIREBALL) &&
              knowsAbility(wizard, ABILITY_CHAIN_LIGHTNING) &&
              wizard.magic.currentMP > 0 &&
              getItemQuantity(wizard.inventory, ITEM_MANA_POTION) == 8,
              "Wizard deterministic spells and package");

        Character cleric = makeCharacter("Earl Mira", CLASS_CLERIC);
        check(applyTestCharacterTitle(cleric, true) == 10 &&
              getClericSpellAccessLevel(cleric) == 5 &&
              knowsAbility(cleric, ABILITY_CHANNEL_ENERGY) &&
              getChannelEnergyDice(cleric) == 5 &&
              cleric.classAbilities.channelEnergyCurrent ==
                  cleric.classAbilities.channelEnergyMax,
              "Cleric spells and Channel Energy progression");

        Character normal = makeCharacter("Aric", CLASS_FIGHTER,
                                         WEAPON_GROUP_BLADES);
        check(applyTestCharacterTitle(normal, true) == 1 && normal.level == 1 &&
              normal.xp == 0,
              "normal level-1 creation unchanged");
        Character disabled = makeCharacter("Earl Aric", CLASS_FIGHTER,
                                           WEAPON_GROUP_BLADES);
        check(applyTestCharacterTitle(disabled, false) == 1 &&
              disabled.level == 1,
              "disabled title is ordinary name");

        const uint8_t wizardItems = wizard.inventory.itemCount;
        const uint16_t wizardMana = getItemQuantity(
            wizard.inventory, ITEM_MANA_POTION);
        check(applyTestCharacterTitle(wizard, true) == 1 &&
              wizard.level == 20 && wizard.inventory.itemCount == wizardItems &&
              getItemQuantity(wizard.inventory, ITEM_MANA_POTION) == wizardMana,
              "repeat application does not grant levels or items");

        Character full = makeCharacter("Sir Full", CLASS_FIGHTER,
                                       WEAPON_GROUP_BLADES);
        check(addItem(full, ITEM_TORCH, MAX_INVENTORY),
              "fill inventory for capacity test");
        const EquipmentData fullEquipment = full.equipment;
        check(!giveTestCharacterStartingEquipment(full, 5) &&
              full.inventory.itemCount == MAX_INVENTORY &&
              std::memcmp(&full.equipment, &fullEquipment,
                          sizeof(fullEquipment)) == 0,
              "capacity failure rolls equipment package back");

        CharacterCreationModel firstModel;
        firstModel.preview = makeCharacter("Earl One", CLASS_CLERIC);
        firstModel.showingPreview = true;
        CharacterCreationModel secondModel;
        secondModel.preview = makeCharacter("Earl Two", CLASS_WIZARD);
        secondModel.showingPreview = true;
        Character first{}, second{};
        uint8_t firstLevel = 0, secondLevel = 0;
        check(firstModel.accept(first, true, &firstLevel) &&
              secondModel.accept(second, true, &secondLevel) &&
              firstLevel == 10 && secondLevel == 10 &&
              first.characterClass == CLASS_CLERIC &&
              second.characterClass == CLASS_WIZARD,
              "independent accounts apply same title independently");
        check(!firstModel.accept(first, true, &firstLevel),
              "accepted creation cannot apply twice");

        std::error_code error;
        std::filesystem::remove_all(saveDirectory, error);
        std::filesystem::create_directories(saveDirectory, error);
        setEnvironment("PFME_SAVE_DIR", saveDirectory.string().c_str());
        check(saveGameToSlot(cleric, 0), "save titled character");
        Character loaded{};
        check(loadGameFromSlot(loaded, 0) &&
              std::strcmp(getCharacterName(loaded), "Earl Mira") == 0 &&
              loaded.level == 10 &&
              loaded.xp == getExperienceForLevel(10) &&
              loaded.inventory.itemCount == cleric.inventory.itemCount,
              "save/load preserves titled character");
        const uint8_t loadedItems = loaded.inventory.itemCount;
        check(applyTestCharacterTitle(loaded, true) == 1 &&
              loaded.level == 10 && loaded.inventory.itemCount == loadedItems,
              "load cannot reapply title shortcut");

        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", "true");
        pfme::runtime::initialize(729);
        for (unsigned index = 0; index < 5; index++)
            tap("a");
        check(isCharacterNameEntryActive(),
              "production creation reached name entry");
        for (const char character : std::string("Earl Aric"))
        {
            const std::string action = std::string("key:") + character;
            check(pfme::runtime::input(action, true),
                  "production title text input");
        }
        tap("encoder");
        check(gameState == GAME_TOWN && player.level == 10 &&
              player.xp == getExperienceForLevel(10) &&
              std::strcmp(getCharacterName(player), "Earl Aric") == 0,
              "authoritative production acceptance applies title");
        check(std::strcmp(getGameMessage(),
                          "Earl Aric begins at Level 10!") == 0,
              "creation confirmation message");

        std::filesystem::remove_all(saveDirectory, error);
        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", nullptr);
        std::cout << "PASS title detection, canonical progression, equipment, "
                     "class features, saves, configuration, and repeat guards\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::error_code error;
        std::filesystem::remove_all(saveDirectory, error);
        setEnvironment("PFME_ENABLE_TEST_CHARACTERS", nullptr);
        std::cerr << "FAILED: " << exception.what() << '\n';
        return 1;
    }
}
