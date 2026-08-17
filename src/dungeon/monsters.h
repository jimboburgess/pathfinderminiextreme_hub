//
// Created by james on 7/13/2026.
//



#ifndef PATHFINDERMINIEXTREME_025_MONSTERS_H
#define PATHFINDERMINIEXTREME_025_MONSTERS_H


#include "characters/characters.h"
#include "characters/items.h"
#include "characters/abilities.h"

enum ChallengeRating
{
    CR_ONE_EIGHTH,
    CR_ONE_QUARTER,
    CR_ONE_THIRD,
    CR_ONE_HALF,

    CR_ONE,
    CR_TWO,
    CR_THREE,
    CR_FOUR,
    CR_FIVE,

    CR_COUNT
};

// enum AbilityID
// {
//     ABILITY_NONE,
//
//     // Basic attacks
//     ABILITY_MELEE_ATTACK,
//     ABILITY_RANGED_ATTACK,
//
//     // Status effects
//     ABILITY_POISON,
//     ABILITY_PARALYZE,
//     ABILITY_TRIP,
//     ABILITY_GRAPPLE,
//
//     // Future magic
//     ABILITY_MAGIC_MISSILE,
//     ABILITY_RAISE_DEAD,
//     ABILITY_FEAR,
//
//     ABILITY_COUNT
// };


enum LootTableID
{
    LOOT_NONE,

    LOOT_POOR,
    LOOT_COMMON,
    LOOT_UNCOMMON,
    LOOT_RARE,
    LOOT_BOSS,
    LOOT_MONSTER,
    LOOT_HUMANOID,
    LOOT_BEAST,
    LOOT_UNDEAD,
    LOOT_ABERRATION,


    LOOT_CHEST_SMALL,
    LOOT_CHEST_MEDIUM,
    LOOT_CHEST_LARGE,

    LOOT_COUNT
};

enum MonsterID
{
    MONSTER_NONE,

    MONSTER_GOBLIN_SCIMITAR,
    MONSTER_GOBLIN_ARCHER,
    MONSTER_BUGBEAR,

    MONSTER_SKELETON,
    MONSTER_ZOMBIE,
    MONSTER_GHOUL,
    MONSTER_WIGHT,

    MONSTER_GIANT_SPIDER,
    MONSTER_GRAY_OOZE,
    MONSTER_VIOLET_FUNGUS,
    MONSTER_CHOKER,
    MONSTER_SPECTATOR,

    MONSTER_COUNT
};

enum MonsterScript
{
    SCRIPT_NONE,

    SCRIPT_MELEE,         // Close with the enemy and fight in melee.
    SCRIPT_RANGED,        // Keep distance and attack from range.
    SCRIPT_COWARD,        // Retreat and heal when wounded.
    SCRIPT_GUARD,         // Hold a position until an enemy is spotted.
    SCRIPT_WANDER,        // Roam when idle.
    SCRIPT_SUPPORT,       // Heal or assist allies.
    SCRIPT_SPELLCASTER,   // Cast spells based on the situation.

    SCRIPT_BOSS,          // Uses multiple attacks and special abilities.
    SCRIPT_PASSIVE,       // Will not attack unless provoked.
    SCRIPT_DEBUG          // For testing combat behavior.
};

// Poison is attack metadata only.  The active condition itself remains in
// Character::conditions and is resolved by the shared condition system.
struct MonsterPoisonData
{
    uint8_t saveDC;
    uint8_t rounds;
};

struct Monster
{
    const char* name;

    const uint16_t* sprite;

    AbilityScores abilities;

    uint8_t hitDice;
    uint8_t baseAttack;
    uint8_t armorClass;

    int8_t fortitude;
    int8_t reflex;
    int8_t will;

    uint8_t speed;
    //uint8_t perception;

    ItemID weapon;
    ItemID armor;

    ChallengeRating challengeRating;

    LootTableID lootTable;

    AbilityID specialAbilities[4];

    MonsterScript script;

    MonsterPoisonData poison;

    // Static spellcasting data copied into the runtime Character on spawn.
    // Existing non-casters omit these trailing fields and receive zero.
    uint8_t maxMP;
    uint8_t casterLevel;

    // Runtime creature identity used by generic condition immunities.
    CreatureType creatureType;

    // Visual abilities query this trait instead of hardcoding MonsterIDs.
    // Omitted trailing aggregate fields are false for ordinary monsters.
    bool sightless;
};

const Monster* getMonster(MonsterID id);

bool monsterHasSpecialAbility(const Monster& monster, AbilityID ability);

uint16_t getMonsterMaxHP(const Monster& monster);

// Pathfinder Table: Experience Point Awards, Total XP column.
uint32_t getExperienceAward(ChallengeRating challengeRating);

extern const Monster monsterDatabase[MONSTER_COUNT];

#endif //PATHFINDERMINIEXTREME_025_MONSTERS_H
