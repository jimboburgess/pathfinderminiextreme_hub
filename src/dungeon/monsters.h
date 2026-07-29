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

enum AIType
{
    AI_NONE,
    AI_MELEE,          // Run toward nearest enemy and attack.
    AI_RANGED,         // Keep distance and shoot.
    AI_COWARD,         // Run away when wounded.
    AI_GUARD,          // Stay near a location until provoked.
    AI_WANDER,         // Roam randomly when idle.
    AI_SUPPORT,         // Buff/heal allies.
    AI_SPELLCASTER,    //CAST SPELLS DUH
    AI_DEBUG,
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

    ItemID weapon;
    ItemID armor;

    ChallengeRating challengeRating;

    LootTableID lootTable;

    AbilityID specialAbilities[4];

    AIType aiType;
};

const Monster* getMonster(MonsterID id);

uint16_t getMonsterMaxHP(const Monster& monster);


extern const Monster monsterDatabase[MONSTER_COUNT];

#endif //PATHFINDERMINIEXTREME_025_MONSTERS_H
