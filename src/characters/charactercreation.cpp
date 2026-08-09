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

static void assignAbilitiesForClass(Character &character,
                                        CharacterClass characterClass,
                                        int scores[])
{
    switch (characterClass)
    {
        case CLASS_FIGHTER:

            character.abilities.strength     = scores[0];
            character.abilities.constitution = scores[1];
            character.abilities.dexterity    = scores[2];
            character.abilities.wisdom       = scores[3];
            character.abilities.charisma     = scores[4];
            character.abilities.intelligence = scores[5];
            break;

        case CLASS_ROGUE:

            character.abilities.dexterity    = scores[0];
            character.abilities.intelligence = scores[1];
            character.abilities.charisma     = scores[2];
            character.abilities.constitution = scores[3];
            character.abilities.wisdom       = scores[4];
            character.abilities.strength     = scores[5];
            break;

        case CLASS_WIZARD:

            character.abilities.intelligence = scores[0];
            character.abilities.dexterity    = scores[1];
            character.abilities.constitution = scores[2];
            character.abilities.wisdom       = scores[3];
            character.abilities.charisma     = scores[4];
            character.abilities.strength     = scores[5];
            break;

        case CLASS_CLERIC:

            character.abilities.wisdom       = scores[0];
            character.abilities.constitution = scores[1];
            character.abilities.strength     = scores[2];
            character.abilities.charisma     = scores[3];
            character.abilities.dexterity    = scores[4];
            character.abilities.intelligence = scores[5];
            break;
    }
}

static void giveStartingEquipment(Character &character,
                                  CharacterClass characterClass)
{
    // Clear equipment
    for (int i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        character.equipment.equipped[i] = ITEM_NONE;
    }

    clearInventory(character.inventory);

    const ClassStartingEquipment& gear = startingEquipment[characterClass];

    character.equipment.equipped[SLOT_MELEE_WEAPON]  = gear.meleeWeapon;
    character.equipment.equipped[SLOT_RANGED_WEAPON] = gear.rangedWeapon;
    character.equipment.equipped[SLOT_ARMOR]         = gear.armor;
    character.equipment.equipped[SLOT_SHIELD]        = gear.shield;

    // TODO: Add starting potions once consumables are implemented.
}

void createCharacter(Character &character, CharacterClass characterClass)
{
    int scores[6];

    rollAbilities(scores);

    character.characterClass = characterClass;
    character.creatureType = CREATURE_PLAYER;
    clearConditions(character);

    assignAbilitiesForClass(character, characterClass, scores);

    giveStartingEquipment(character, characterClass);

    character.level = 1;
    character.xp = 0;
    character.speed = 6;
    character.health.maxHP = getMaxHP(character);
    character.health.currentHP = character.health.maxHP;
    character.magic.currentMP = 0;
    character.magic.maxMP = 0;
    character.magic.knownAbilityCount = 0;
    character.magic.arcaneCaster = false;
    character.magic.divineCaster = false;

    character.team = TEAM_PLAYER;
    character.state = STATE_ALIVE;
}

