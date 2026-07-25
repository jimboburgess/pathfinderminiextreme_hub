//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_CHARACTERS_H
#define PATHFINDERMINIEXTREME_025_CHARACTERS_H

#include <Arduino.h>
#include "items.h"
#include "abilities.h"
#include "conditions.h"

//==================================================
// Character Constants
//==================================================

constexpr uint8_t MAX_INVENTORY = 64;

//==================================================
// Ability Scores
//==================================================

struct AbilityScores
{
    uint8_t strength;
    uint8_t dexterity;
    uint8_t constitution;

    uint8_t intelligence;
    uint8_t wisdom;
    uint8_t charisma;
};

//==================================================
// Character Classes
//==================================================

enum CharacterClass
{
    CLASS_FIGHTER,
    CLASS_ROGUE,
    CLASS_WIZARD,
    CLASS_CLERIC
};

//==================================================
// Abilities
//==================================================

enum AbilityScore
{
    ABILITY_STRENGTH,
    ABILITY_DEXTERITY,
    ABILITY_CONSTITUTION,
    ABILITY_INTELLIGENCE,
    ABILITY_WISDOM,
    ABILITY_CHARISMA
};

//==================================================
// Skills
//==================================================

enum Skill
{
    SKILL_ACROBATICS,
    SKILL_DIPLOMACY,
    SKILL_DISABLE_DEVICE,
    SKILL_INTIMIDATE,
    SKILL_PERCEPTION,
    SKILL_STEALTH,

    SKILL_COUNT
};

enum SkillAptitude
{
    SKILL_NONE,
    SKILL_POOR,
    SKILL_AVERAGE,
    SKILL_GOOD,
    SKILL_EXCELLENT
};


//==================================================
// Creature Types
//==================================================

enum CreatureType
{
    CREATURE_PLAYER,
    CREATURE_GOBLIN,
    CREATURE_SKELETON,
    CREATURE_WOLF,
    CREATURE_ORC,
    CREATURE_ZOMBIE,
    CREATURE_BEHOLDER
};

//==================================================
// Teams
//==================================================

enum Team
{
    TEAM_PLAYER,
    TEAM_MONSTER,
    TEAM_NEUTRAL
};

//==================================================
// Character State
//==================================================

enum CharacterState
{
    STATE_ALIVE,
    STATE_UNCONSCIOUS,
    STATE_DEAD,
    STATE_LOOTED
};
//==================================================
// Character
//==================================================


struct HealthData
{
    int currentHP;
    int maxHP;
};

struct MagicData
{
    int currentMP;
    int maxMP;

    AbilityID knownAbilities[MAX_KNOWN_ABILITIES];
    uint8_t knownAbilityCount;

    bool arcaneCaster;
    bool divineCaster;
};

struct EquipmentData
{
    ItemID equipped[NUM_EQUIPMENT_SLOTS];
};

struct InventoryData
{
    ItemID items[MAX_INVENTORY];
    uint8_t itemCount;
};

struct Character
{
    //==================================================
    // Identity
    //==================================================

    String name;

    CharacterClass characterClass;
    CreatureType creatureType;

    Team team;
    CharacterState state;

    //==================================================
    // Progression
    //==================================================

    uint8_t level;
    uint32_t xp;

    //==================================================
    // Movement
    //==================================================

    uint8_t speed;

    //==================================================
    // Ability Scores
    //==================================================

    AbilityScores abilities;

    //==================================================
    // Health
    //==================================================

    HealthData health;

    //==================================================
    // Magic
    //==================================================

    MagicData magic;

    //==================================================
    // Equipment
    //==================================================

    EquipmentData equipment;

    //==================================================
    // Inventory
    //==================================================

    InventoryData inventory;
};

//==================================================
// Character Rules
//==================================================

int getAbilityModifier(const Character& character, AbilityScore ability);

int getMaxHP(const Character& character);

const Weapon* getEquippedMeleeWeapon(const Character& character);

const Weapon* getEquippedRangedWeapon(const Character& character);

const Armor* getEquippedArmor(const Character& character);

const Shield* getEquippedShield(const Character& character);

int getArmorClass(const Character& character);

int getMeleeAttackBonus(const Character& character);

int getRangedAttackBonus(const Character& character);

int getMovementSpeed(const Character& character);

uint32_t getExperienceToNextLevel(const Character& character);

//==================================================
// Skills
//==================================================

SkillAptitude getSkillAptitude(CharacterClass characterClass,
                               Skill skill);

int getSkillRank(SkillAptitude aptitude,
                 uint8_t level);

int getSkillBonus(const Character& character,
                  Skill skill);

//==================================================
// Saving Throws
//==================================================

int getFortitudeSave(const Character& character);

int getReflexSave(const Character& character);

int getWillSave(const Character& character);

//==================================================
// Status
//==================================================

bool isAlive(const Character& character);

bool isDead(const Character& character);

bool isConscious(const Character& character);

bool canAct(const Character& character);

bool isLootable(const Character& character);

const char* getCharacterClassName(CharacterClass characterClass);

//==================================================
// Inventory & Equipment
//==================================================

bool canEquip(ItemID item);

EquipmentSlot getEquipmentSlot(ItemID item);

bool equipItem(Character& character, ItemID item);

bool unequipItem(Character& character, EquipmentSlot slot);

bool addItem(Character& character, ItemID item);

bool removeItem(Character& character, ItemID item);

bool hasItem(const Character& character, ItemID item);

bool inventoryFull(const Character& character);

ItemID getEquippedItem(const Character& character, EquipmentSlot slot);

const char* getEquippedItemName(const Character& character,
                                EquipmentSlot slot);

#endif // PATHFINDERMINIEXTREME_025_CHARACTERS_H