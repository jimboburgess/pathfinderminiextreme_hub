//
// Created by james on 7/13/2026.
//

#include "monsters.h"

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
        ITEM_NONE,
        ITEM_NONE,
        CR_ONE_EIGHTH,
        LOOT_NONE,
        { ABILITY_NONE, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    //======================================================
    // Goblins
    //======================================================
    {
        "Goblin",
        goblin16x16,
        {11,15,12,10,9,6},
        1,
        1,
        16,
        3,3,-1,
        ITEM_SCIMITAR,
        ITEM_LEATHER_ARMOR,
        CR_ONE_THIRD,
        LOOT_HUMANOID,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Goblin Archer",
        goblinArcher16x16,
        {11,15,12,10,9,6},
        1,
        1,
        15,
        3,3,-1,
        ITEM_SHORTBOW,
        ITEM_NONE,
        CR_ONE_THIRD,
        LOOT_HUMANOID,
        { ABILITY_RANGED_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Bugbear",
        bugbear16x16,
        {15,14,13,10,10,9},
        3,
        3,
        18,
        4,3,1,
        ITEM_MORNINGSTAR,
        ITEM_HIDE_ARMOR,
        CR_TWO,
        LOOT_HUMANOID,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
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
        ITEM_LONGSWORD,
        ITEM_NATURAL_ARMOR_1,
        CR_ONE_THIRD,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Zombie",
        zombie16x16,
        {13,8,0,0,10,0},
        2,
        1,
        14,
        3,0,3,
        ITEM_SLAM,
        ITEM_NATURAL_ARMOR_2,
        CR_ONE_HALF,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Ghoul",
        ghoul16x16,
        {13,15,0,13,14,14},
        2,
        2,
        16,
        3,5,5,
        ITEM_CLAWS,
        ITEM_NATURAL_ARMOR_1,
        CR_ONE,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Wight",
        wight16x16,
        {12,12,0,11,13,15},
        4,
        4,
        17,
        4,4,6,
        ITEM_SLAM,
        ITEM_NATURAL_ARMOR_3,
        CR_THREE,
        LOOT_UNDEAD,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    //======================================================
    // General Monsters
    //======================================================
    {
        "Giant Spider",
        giantSpider16x16,
        {13,15,12,0,10,2},
        2,
        2,
        14,
        3,5,1,
        ITEM_BITE,
        ITEM_NATURAL_ARMOR_2,
        CR_ONE,
        LOOT_BEAST,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Gray Ooze",
        grayOoze16x16,
        {12,1,0,0,1,1},
        3,
        3,
        15,
        5,0,0,
        ITEM_PSEUDOPOD,
        ITEM_NATURAL_ARMOR_4,
        CR_TWO,
        LOOT_MONSTER,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Violet Fungus",
        violetFungus16x16,
        {10,5,0,0,10,1},
        4,
        3,
        18,
        6,1,4,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_3,
        CR_THREE,
        LOOT_MONSTER,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Choker",
        choker16x16,
        {16,14,13,4,13,7},
        3,
        3,
        17,
        3,5,2,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_2,
        CR_TWO,
        LOOT_ABERRATION,
        { ABILITY_MELEE_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
    },

    {
        "Spectator",
        spectator16x16,
        {14,20,16,16,14,14},
        4,
        3,
        20,
        11,13,14,
        ITEM_TENTACLE,
        ITEM_NATURAL_ARMOR_2,
        CR_FOUR,
        LOOT_ABERRATION,
        { ABILITY_RANGED_ATTACK, ABILITY_NONE, ABILITY_NONE, ABILITY_NONE }
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