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
// Character Races
//==================================================

enum Race
{
    RACE_HUMAN,
    RACE_ELF,
    RACE_DWARF,
    RACE_HALF_ORC,

    RACE_COUNT
};

struct RaceData
{
    const char* name;

    AbilityScores modifiers;

    AbilityID abilities[4];

    //TODO
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

struct InventorySlot
{
    ItemID item = ITEM_NONE;
    uint8_t quantity = 0;
};

static_assert(sizeof(InventorySlot) == 2,
              "InventorySlot must remain compact for entity storage.");

struct InventoryData
{
    InventorySlot slots[MAX_INVENTORY];
    uint8_t itemCount = 0;
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
    // Initiative
    //==================================================
    int8_t initiative;

    //==================================================
    // Ability Scores
    //==================================================
    AbilityScores abilities;

    //==================================================
    // Health
    //==================================================
    HealthData health;

    //==================================================
    // Conditions
    //==================================================
    ConditionData conditions;

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

int getAbilityModifier(int score);
int getAbilityModifier(const Character& character, AbilityScore ability);

int getMaxHP(const Character& character);

const uint16_t* getPlayerSprite(CharacterClass characterClass);

const Weapon* getEquippedMeleeWeapon(const Character& character);

const Weapon* getEquippedRangedWeapon(const Character& character);

const Armor* getEquippedArmor(const Character& character);

const Shield* getEquippedShield(const Character& character);

int getArmorClass(const Character& character, int dodgeBonus = 0);

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

void clearInventory(InventoryData& inventory);

const InventorySlot* getInventorySlot(const InventoryData& inventory,
                                      uint8_t index);

InventorySlot* getInventorySlot(InventoryData& inventory, uint8_t index);

uint16_t getItemQuantity(const InventoryData& inventory, ItemID item);

uint16_t getItemQuantity(const InventorySlot slots[],
                         uint8_t itemCount,
                         ItemID item);

bool addItemToSlots(InventorySlot slots[],
                    uint8_t& itemCount,
                    uint8_t capacity,
                    ItemID item,
                    uint8_t quantity = 1);

bool removeItemFromSlots(InventorySlot slots[],
                         uint8_t& itemCount,
                         uint8_t capacity,
                         ItemID item,
                         uint8_t quantity = 1);

bool addInventoryItem(InventoryData& inventory,
                      ItemID item,
                      uint8_t quantity = 1);

bool removeInventoryItem(InventoryData& inventory,
                         ItemID item,
                         uint8_t quantity = 1);

bool inventoryFull(const InventoryData& inventory);

bool addItem(Character& character,
             ItemID item,
             uint8_t quantity = 1);

bool removeItem(Character& character,
                ItemID item,
                uint8_t quantity = 1);

bool hasItem(const Character& character, ItemID item);

uint16_t getItemQuantity(const Character& character, ItemID item);

bool inventoryFull(const Character& character);

ItemID getEquippedItem(const Character& character, EquipmentSlot slot);

const char* getEquippedItemName(const Character& character,
                                EquipmentSlot slot);

#endif // PATHFINDERMINIEXTREME_025_CHARACTERS_H
