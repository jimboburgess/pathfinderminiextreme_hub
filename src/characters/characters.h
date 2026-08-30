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
constexpr uint32_t STARTING_GOLD = 100;


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
    CREATURE_UNDEAD,
    CREATURE_BEHOLDER,
    CREATURE_MONSTER
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
    STATE_LOOTED,
    // Turned creatures have left the encounter without being killed.
    STATE_TURNED
};
//==================================================
// Character
//==================================================


struct HealthData
{
    int currentHP;
    int maxHP;
};

struct SpellLearningData
{
    bool active = false;
    AbilityID ability = ABILITY_NONE;
    uint8_t restsRemaining = 0;
};

struct MagicData
{
    int currentMP;
    int maxMP;

    AbilityID knownAbilities[MAX_KNOWN_ABILITIES];
    uint8_t knownAbilityCount;

    bool arcaneCaster;
    bool divineCaster;
    SpellLearningData learning;
};

struct ClassAbilityData
{
    uint8_t channelEnergyCurrent = 0;
    uint8_t channelEnergyMax = 0;
};

struct EquipmentData
{
    ItemInstance equipped[NUM_EQUIPMENT_SLOTS];
};

struct InventorySlot
{
    ItemInstance item =
    {
        ITEM_NONE,
        0,
        WEAPON_ENHANCEMENT_NONE
    };
    uint8_t quantity = 0;
};

static_assert(sizeof(InventorySlot) == 4,
              "InventorySlot must remain compact for entity storage.");

struct InventoryData
{
    InventorySlot slots[MAX_INVENTORY];
    uint8_t itemCount = 0;
    uint32_t gold = 0;
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
    // Limited-use class abilities
    //==================================================
    ClassAbilityData classAbilities;

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
int getEffectiveAbilityScore(const Character& character, AbilityScore ability);
int getEffectiveSpeed(const Character& character);

int getMaxHP(const Character& character);

// Applies positive damage to a living character and notifies the shared
// condition system. Defeat/XP/loot state remains with combat.
int damageCharacter(Character& character, int damage);

// Synchronizes recoverable HP state. An unconscious character becomes alive
// after healing raises HP above zero; genuinely dead/removed states are never
// revived. Entity-specific zero-HP defeat policy remains with combat.
void updateCharacterStateFromHP(Character& character);

// Applies positive healing to a living or unconscious character without
// exceeding max HP and returns the amount actually restored. Dead, looted,
// and turned entities cannot be revived through ordinary healing.
int healCharacter(Character& character, int healing);

// Restores a positive amount without exceeding max MP and returns the amount
// actually restored. This does not alter max MP.
int restoreMana(Character& character, int amount);

uint8_t getMaxChannelEnergyUses(const Character& character);
uint8_t getChannelEnergyDice(const Character& character);
void restoreClassAbilityUses(Character& character);

const uint16_t* getPlayerSprite(CharacterClass characterClass);

const Weapon* getEquippedMeleeWeapon(const Character& character);

const Weapon* getEquippedRangedWeapon(const Character& character);

const Armor* getEquippedArmor(const Character& character);

const Shield* getEquippedShield(const Character& character);

int getArmorClass(const Character& character, int dodgeBonus = 0);

// Touch AC deliberately ignores equipped armor, shields, natural armor, and
// untyped temporary AC. Dodge may be supplied explicitly; distinct
// deflection/size storage does not exist yet.
int getTouchArmorClass(const Character& character, int dodgeBonus = 0);

int getMeleeAttackBonus(const Character& character);

int getRangedAttackBonus(const Character& character);

// Spell ranged-touch attacks use BAB, effective Dexterity, and generic timed
// attack modifiers. Equipped-weapon enhancement bonuses never apply.
int getRangedTouchAttackBonus(const Character& character);

// Hostile spell touch attacks use BAB, effective Strength, and generic timed
// attack modifiers without applying equipped-weapon enhancement bonuses.
int getMeleeTouchAttackBonus(const Character& character);

// Rogue class feature progression. Returns zero for non-Rogues or an
// uninitialized level, otherwise 1d6 at level 1 and one additional d6 every
// two levels through level 20.
uint8_t getSneakAttackDice(const Character& character);

int getPowerAttackPenalty(const Character& character);
int getPowerAttackDamageBonus(const Character& character,
                              const Weapon& weapon);

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

enum EquipResult
{
    EQUIP_SUCCESS,
    EQUIP_INVALID_ITEM,
    EQUIP_NOT_OWNED,
    EQUIP_INVENTORY_ERROR,
    EQUIP_TWO_HANDED_CONFLICT
};

bool isSupportedEquipmentSlot(EquipmentSlot slot);
bool isItemCompatibleWithEquipmentSlot(const ItemInstance& item,
                                       EquipmentSlot slot);
EquipResult getEquipmentCompatibility(const Character& character,
                                      const ItemInstance& item);
EquipResult equipItemWithResult(Character& character,
                               const ItemInstance& item);

bool equipItem(Character& character, const ItemInstance& item);

bool equipItem(Character& character, ItemID item);

bool unequipItem(Character& character, EquipmentSlot slot);

void clearInventory(InventoryData& inventory);

const InventorySlot* getInventorySlot(const InventoryData& inventory,
                                      uint8_t index);

InventorySlot* getInventorySlot(InventoryData& inventory, uint8_t index);

uint16_t getItemQuantity(const InventoryData& inventory, ItemID item);

uint16_t getItemQuantity(const InventoryData& inventory,
                         const ItemInstance& item);

uint16_t getItemQuantity(const InventorySlot slots[],
                         uint8_t itemCount,
                         ItemID item);

uint16_t getItemQuantity(const InventorySlot slots[],
                         uint8_t itemCount,
                         const ItemInstance& item);

bool addItemToSlots(InventorySlot slots[],
                    uint8_t& itemCount,
                    uint8_t capacity,
                    const ItemInstance& item,
                    uint8_t quantity = 1);

bool addItemToSlots(InventorySlot slots[],
                    uint8_t& itemCount,
                    uint8_t capacity,
                    ItemID item,
                    uint8_t quantity = 1);

bool removeItemFromSlots(InventorySlot slots[],
                         uint8_t& itemCount,
                         uint8_t capacity,
                         const ItemInstance& item,
                         uint8_t quantity = 1);

bool removeItemFromSlots(InventorySlot slots[],
                         uint8_t& itemCount,
                         uint8_t capacity,
                         ItemID item,
                         uint8_t quantity = 1);

bool addInventoryItem(InventoryData& inventory,
                      const ItemInstance& item,
                      uint8_t quantity = 1);

bool addInventoryItem(InventoryData& inventory,
                      ItemID item,
                      uint8_t quantity = 1);

bool removeInventoryItem(InventoryData& inventory,
                         const ItemInstance& item,
                         uint8_t quantity = 1);

bool removeInventoryItem(InventoryData& inventory,
                         ItemID item,
                         uint8_t quantity = 1);

bool inventoryFull(const InventoryData& inventory);

bool addItem(Character& character,
             const ItemInstance& item,
             uint8_t quantity = 1);

bool addItem(Character& character,
             ItemID item,
             uint8_t quantity = 1);

bool removeItem(Character& character,
                const ItemInstance& item,
                uint8_t quantity = 1);

bool removeItem(Character& character,
                ItemID item,
                uint8_t quantity = 1);

bool hasItem(const Character& character, const ItemInstance& item);

bool hasItem(const Character& character, ItemID item);

uint16_t getItemQuantity(const Character& character,
                         const ItemInstance& item);

uint16_t getItemQuantity(const Character& character, ItemID item);

bool inventoryFull(const Character& character);

ItemID getEquippedItem(const Character& character, EquipmentSlot slot);

const char* getEquippedItemName(const Character& character,
                                EquipmentSlot slot);

#endif // PATHFINDERMINIEXTREME_025_CHARACTERS_H
