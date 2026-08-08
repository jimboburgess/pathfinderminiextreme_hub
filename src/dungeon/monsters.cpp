//
// Created by james on 7/13/2026.
//

#include "monsters.h"

#include "data/dice.h"
#include "characters/characters.h"
#include "graphics/monstersprites.h"


const Monster monsterDatabase[MONSTER_COUNT] =
{
    //======================================================
    // None
    //======================================================
    {
        "None",
        nullptr,
        {0,0,0,0,0,0},
        0,
        0,
        0,
        0,0,0,
        6,
        ITEM_NONE,
        ITEM_NONE,
        CR_ONE_EIGHTH,
        LOOT_NONE,
        { ABILITY_NONE, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_NONE,
    },

    //======================================================
    // Goblins
    //======================================================
    {
        "Goblin",
        goblinSprite16x16r1,
        {11,15,12,10,9,6},
        1,
        1,
        16,
        3,3,-1,6,
        ITEM_SCIMITAR,
        ITEM_LEATHER_ARMOR,
        CR_ONE_THIRD,
        LOOT_HUMANOID,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Goblin Archer",
        goblinArcher16x16,
        {11,15,12,10,9,6},
        1,
        1,
        15,
        3,3,-1,
        6,
        ITEM_SHORTBOW,
        ITEM_NONE,
        CR_ONE_THIRD,
        LOOT_HUMANOID,
        { ABILITY_RANGED_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_RANGED
    },

    {
        "Bugbear",
        bugbear16x16,
        {15,14,13,10,10,9},
        3,
        3,
        18,
        4,3,1,
        8,
        ITEM_MORNINGSTAR,
        ITEM_HIDE_ARMOR,
        CR_TWO,
        LOOT_HUMANOID,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    //======================================================
    // Undead
    //======================================================
    {
        "Skeleton",
        skeleton16x16,
        {12,14,0,0,10,0},
        1,
        1,
        15,
        2,2,0,
        6,
        ITEM_LONGSWORD,
        ITEM_NATURAL_ARMOR_1,
        CR_ONE_THIRD,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Zombie",
        zombie16x16,
        {13,8,0,0,10,0},
        2,
        1,
        14,
        3,0,3,
        4,
        ITEM_SLAM,
        ITEM_NATURAL_ARMOR_2,
        CR_ONE_HALF,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Ghoul",
        ghoul16x16,
        {13,15,0,13,14,14},
        2,
        2,
        16,
        3,5,5,
        6,
        ITEM_CLAWS,
        ITEM_NATURAL_ARMOR_1,
        CR_ONE,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Wight",
        wight16x16,
        {12,12,0,11,13,15},
        4,
        4,
        17,
        4,4,6,
        6,
        ITEM_SLAM,
        ITEM_NATURAL_ARMOR_3,
        CR_THREE,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    //======================================================
    // General Monsters
    //======================================================
    {
        "Giant Spider",
        giantspider32x32,
        {13,15,12,0,10,2},
        2,
        2,
        14,
        3,5,1,
        8,
        ITEM_BITE,
        ITEM_NATURAL_ARMOR_2,
        CR_ONE,
        LOOT_BEAST,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Gray Ooze",
        grayOoze16x16,
        {12,1,0,0,1,1},
        3,
        3,
        15,
        5,0,0,
        4,
        ITEM_PSEUDOPOD,
        ITEM_NATURAL_ARMOR_4,
        CR_TWO,
        LOOT_MONSTER,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Violet Fungus",
        violetFungus16x16,
        {10,5,0,0,10,1},
        4,
        3,
        18,
        6,1,4,
        0,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_3,
        CR_THREE,
        LOOT_MONSTER,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_SUPPORT
    },

    {
        "Choker",
        choker16x16,
        {16,14,13,4,13,7},
        3,
        3,
        17,
        3,5,2,6,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_2,
        CR_TWO,
        LOOT_ABERRATION,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_MELEE,
    },

    {
        "Spectator",
        spectator32x32,
        {14,20,16,16,14,14},
        4,
        3,
        20,
        11,13,14,
        4,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_2,
        CR_FOUR,
        LOOT_ABERRATION,
        { ABILITY_RANGED_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }, SCRIPT_SPELLCASTER,
    },
};

static_assert(
    MONSTER_COUNT == (sizeof(monsterDatabase) / sizeof(monsterDatabase[0])),
    "MonsterID enum and monsterDatabase[] are out of sync."
);

const Monster* getMonster(MonsterID id)
{
    if (id <= MONSTER_NONE || id >= MONSTER_COUNT)
        return nullptr;

    return &monsterDatabase[id];
}

uint16_t getMonsterMaxHP(const Monster& monster)
{
    uint16_t hp = rollDice(monster.hitDice, 8);

    hp += monster.hitDice * getAbilityModifier(monster.abilities.constitution);

    if (hp < monster.hitDice)
        hp = monster.hitDice;

    return hp;
}

uint32_t getExperienceAward(ChallengeRating challengeRating)
{
    // Pathfinder Table: Experience Point Awards, Total XP column.
    switch (challengeRating)
    {
        case CR_ONE_EIGHTH: return 50;
        case CR_ONE_QUARTER: return 100;
        case CR_ONE_THIRD:   return 135;
        case CR_ONE_HALF:    return 200;
        case CR_ONE:         return 400;
        case CR_TWO:         return 600;
        case CR_THREE:       return 800;
        case CR_FOUR:        return 1200;
        case CR_FIVE:        return 1600;
        default:             return 0;
    }
}

