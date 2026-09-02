//
// Created by james on 7/13/2026.
//

#include "graphics/charcreationscreen.h"
#include "data/dice.h"

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

void createCharacter(Character &character,
                     CharacterClass characterClass,
                     WeaponGroup fighterWeaponGroup)
{
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

