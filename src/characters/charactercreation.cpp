//
// Created by james on 7/13/2026.
//

#include "graphics/charcreationscreen.h"
#include "data/dice.h"
#include "data/progression.h"

#if defined(PFME_NATIVE_CONTROLLER_CONTEXT)
#include <cstdlib>
#endif

struct ClassStartingEquipment
{
    ItemID meleeWeapon;
    ItemID rangedWeapon;

    ItemID armor;
    ItemID shield;
};

static const ClassStartingEquipment startingEquipment[] =
{
    // Fighter
    {
        ITEM_LONGSWORD,
        ITEM_SHORTBOW,
        ITEM_CHAINMAIL,
        ITEM_HEAVY_WOODEN_SHIELD,

    },

    // Rogue
    {
        ITEM_DAGGER,
        ITEM_LIGHT_CROSSBOW,
        ITEM_LEATHER_ARMOR,
        ITEM_NONE
    },

    // Wizard
    {
        ITEM_QUARTERSTAFF,
        ITEM_SHORTBOW,
        ITEM_NONE,
        ITEM_NONE
    },

    // Cleric
    {
        ITEM_MACE,
        ITEM_LIGHT_CROSSBOW,
        ITEM_CHAINMAIL,
        ITEM_HEAVY_WOODEN_SHIELD
    }
};

static void sortDescending(int values[], int count)
{
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (values[j] > values[i])
            {
                int temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }
}

static void rollAbilities(int scores[])
{
    for (int i = 0; i < 6; i++)
    {
        scores[i] = rollAbilityScore();
    }

    sortDescending(scores, 6);
}

static void giveStartingEquipment(Character &character,
                                  CharacterClass characterClass)
{
    // Clear equipment
    for (int i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        character.equipment.equipped[i] = makeItemInstance(ITEM_NONE);
    }

    clearInventory(character.inventory);
    character.inventory.gold = STARTING_GOLD;

    ClassStartingEquipment gear = startingEquipment[characterClass];

    if (characterClass == CLASS_FIGHTER)
    {
        gear.meleeWeapon = getFighterStartingMeleeWeapon(
            character.trainedWeaponGroup);
        gear.rangedWeapon = getFighterStartingRangedWeapon();
    }

    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(gear.meleeWeapon);
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(gear.rangedWeapon);
    character.equipment.equipped[SLOT_ARMOR] =
        makeItemInstance(gear.armor);
    character.equipment.equipped[SLOT_SHIELD] =
        makeItemInstance(gear.shield);

    // TODO: Add starting potions once consumables are implemented.
}

namespace
{
struct TestCharacterPackage
{
    uint8_t level;
    uint8_t enhancement;
    ItemID healingPotion;
    uint8_t healingQuantity;
    uint8_t manaQuantity;
};

const TestCharacterPackage testCharacterPackages[] =
{
    { 5,  1, ITEM_POTION_CURE_LIGHT_WOUNDS,    4, 3 },
    { 10, 2, ITEM_POTION_CURE_MODERATE_WOUNDS, 4, 5 },
    { 15, 3, ITEM_POTION_CURE_SERIOUS_WOUNDS,  4, 6 },
    { 20, 5, ITEM_POTION_CURE_CRITICAL_WOUNDS, 4, 8 }
};

bool asciiEqualIgnoreCase(char left, char right)
{
    if (left >= 'A' && left <= 'Z')
        left = static_cast<char>(left - 'A' + 'a');
    if (right >= 'A' && right <= 'Z')
        right = static_cast<char>(right - 'A' + 'a');
    return left == right;
}

bool asciiStringEqualIgnoreCase(const char* left, const char* right)
{
    if (left == nullptr || right == nullptr)
        return false;
    while (*left != '\0' && *right != '\0')
    {
        if (!asciiEqualIgnoreCase(*left, *right))
            return false;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

bool hasTitlePrefix(const char* name, const char* title)
{
    if (name == nullptr || title == nullptr)
        return false;

    uint8_t index = 0;
    while (title[index] != '\0')
    {
        if (name[index] == '\0' ||
            !asciiEqualIgnoreCase(name[index], title[index]))
            return false;
        index++;
    }

    return name[index] == ' ' && name[index + 1] != '\0' &&
           name[index + 1] != ' ';
}

const TestCharacterPackage* getTestCharacterPackage(uint8_t level)
{
    for (const TestCharacterPackage& package : testCharacterPackages)
    {
        if (package.level == level)
            return &package;
    }
    return nullptr;
}

ItemID getTestCharacterArmor(CharacterClass characterClass, uint8_t level)
{
    if (characterClass == CLASS_WIZARD)
        return ITEM_NONE;
    if (characterClass == CLASS_ROGUE)
        return level == 5 ? ITEM_STUDDED_LEATHER : ITEM_CHAIN_SHIRT;
    if (level == 5)
        return ITEM_BREASTPLATE;
    if (level == 10)
        return ITEM_BANDED_MAIL;
    if (level == 15)
        return ITEM_HALF_PLATE;
    return ITEM_FULL_PLATE;
}

ItemInstance makeEnhancedEquipment(ItemID itemID, uint8_t enhancement)
{
    ItemInstance item = makeItemInstance(itemID);
    if (itemID != ITEM_NONE)
        item.enhancementBonus = static_cast<int8_t>(enhancement);
    return item;
}

void grantTestWizardSpellSelections(Character& character)
{
    if (character.characterClass != CLASS_WIZARD)
        return;

    const AbilityID choices[] =
    {
        ABILITY_SCORCHING_RAY,
        ABILITY_FIREBALL,
        ABILITY_HASTE,
        ABILITY_LIGHTNING_BOLT,
        ABILITY_WALL_OF_FIRE,
        ABILITY_CHAIN_LIGHTNING,
        ABILITY_MIND_BLANK
    };
    const uint8_t maximumSpellLevel =
        character.level < 9 ? static_cast<uint8_t>((character.level + 1) / 2)
                            : 5;

    for (AbilityID choice : choices)
    {
        const Ability* ability = getAbility(choice);
        if (ability != nullptr && ability->level <= maximumSpellLevel)
            learnAbility(character, choice);
    }
}
}

uint8_t getTestCharacterStartingLevel(const char* name, bool enabled)
{
    if (!enabled)
        return 1;
    if (hasTitlePrefix(name, "Sir"))
        return 5;
    if (hasTitlePrefix(name, "Earl"))
        return 10;
    if (hasTitlePrefix(name, "Duke"))
        return 15;
    if (hasTitlePrefix(name, "King"))
        return 20;
    return 1;
}

bool giveTestCharacterStartingEquipment(Character& character,
                                        uint8_t startingLevel)
{
    const TestCharacterPackage* package =
        getTestCharacterPackage(startingLevel);
    if (package == nullptr)
        return false;

    const EquipmentData originalEquipment = character.equipment;
    const InventoryData originalInventory = character.inventory;
    const ItemID armor = getTestCharacterArmor(
        character.characterClass, startingLevel);

    ItemInstance& melee =
        character.equipment.equipped[SLOT_MELEE_WEAPON];
    ItemInstance& ranged =
        character.equipment.equipped[SLOT_RANGED_WEAPON];
    if (melee.itemID != ITEM_NONE)
        melee = makeMagicWeapon(melee.itemID, package->enhancement);
    if (ranged.itemID != ITEM_NONE)
        ranged = makeMagicWeapon(ranged.itemID, package->enhancement);
    character.equipment.equipped[SLOT_ARMOR] =
        makeEnhancedEquipment(armor, package->enhancement);

    ItemInstance& shield = character.equipment.equipped[SLOT_SHIELD];
    if (character.characterClass == CLASS_FIGHTER ||
        character.characterClass == CLASS_CLERIC)
    {
        shield = makeEnhancedEquipment(
            ITEM_HEAVY_STEEL_SHIELD, package->enhancement);
    }
    else
        shield = makeItemInstance(ITEM_NONE);

    bool added = addItem(
        character, package->healingPotion, package->healingQuantity);
    if (added && (character.characterClass == CLASS_WIZARD ||
                  character.characterClass == CLASS_CLERIC))
    {
        added = addItem(character, ITEM_MANA_POTION, package->manaQuantity);
    }
    if (added && character.characterClass == CLASS_ROGUE)
        added = addItem(character, ITEM_MASTERWORK_THIEVES_TOOLS);

    if (!added)
    {
        character.equipment = originalEquipment;
        character.inventory = originalInventory;
        return false;
    }
    return true;
}

uint8_t applyTestCharacterTitle(Character& character, bool enabled)
{
    const uint8_t targetLevel = getTestCharacterStartingLevel(
        getCharacterName(character), enabled);
    if (targetLevel == 1 || character.level != 1 || character.xp != 0)
        return 1;

    const Character original = character;
    const uint32_t targetExperience = getExperienceForLevel(targetLevel);
    if (awardExperience(character, targetExperience) != targetLevel - 1 ||
        character.level != targetLevel ||
        !giveTestCharacterStartingEquipment(character, targetLevel))
    {
        character = original;
        return 1;
    }

    grantTestWizardSpellSelections(character);
    character.health.currentHP = character.health.maxHP;
    character.magic.currentMP = character.magic.maxMP;
    restoreClassAbilityUses(character);
    return targetLevel;
}

bool testCharactersEnabled()
{
#if defined(PFME_NATIVE_CONTROLLER_CONTEXT)
    const char* value = std::getenv("PFME_ENABLE_TEST_CHARACTERS");
    if (value == nullptr)
        return false;
    return (value[0] == '1' && value[1] == '\0') ||
           asciiStringEqualIgnoreCase(value, "true");
#elif defined(PFME_ENABLE_TEST_CHARACTERS_DEFAULT)
    return PFME_ENABLE_TEST_CHARACTERS_DEFAULT != 0;
#else
    return false;
#endif
}

void createCharacter(Character &character,
                     CharacterClass characterClass,
                     WeaponGroup fighterWeaponGroup)
{
    character = {};
    setCharacterName(character, DEFAULT_CHARACTER_NAME);
    int scores[6];

    rollAbilities(scores);

    character.characterClass = characterClass;
    character.creatureType = CREATURE_PLAYER;
    setFighterTrainedWeaponGroup(character, fighterWeaponGroup);
    clearConditions(character);

    assignAbilityScoresForClass(character, characterClass, scores);

    giveStartingEquipment(character, characterClass);

    character.level = 1;
    character.xp = 0;
    character.speed = 6;
    character.health.maxHP = getMaxHP(character);
    character.health.currentHP = character.health.maxHP;
    initializeCharacterMagic(character);

    if (character.characterClass == CLASS_FIGHTER)
        learnAbility(character, ABILITY_POWER_ATTACK);
    else if (character.characterClass == CLASS_CLERIC)
    {
        learnAbility(character, ABILITY_CHANNEL_ENERGY);
        learnAbility(character, ABILITY_TURN_UNDEAD);
    }

    restoreClassAbilityUses(character);

    character.team = TEAM_PLAYER;
    character.state = STATE_ALIVE;
}

