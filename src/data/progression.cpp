//
// Created by james on 7/13/2026.
//

#include "progression.h"

namespace
{
const uint16_t wizardMPProgression[MAX_CHARACTER_LEVEL] =
{
      6,   8,  11,  14,  18,
     22,  27,  32,  38,  44,
     51,  58,  66,  74,  83,
     92, 102, 112, 123, 134
};

struct LearnedAbilityAtLevel
{
    uint8_t characterLevel;
    AbilityID ability;
};

// This is the single Wizard known-spell progression. It deliberately includes
// spells whose effects are not executable yet; the Cast Spell menu continues
// to filter those through isAbilitySupported().
const LearnedAbilityAtLevel wizardSpellProgression[] =
{
    { 1, ABILITY_MAGIC_MISSILE },
    { 1, ABILITY_SLEEP },
    { 1, ABILITY_GREASE },

    { 3, ABILITY_ACID_ARROW },
    { 3, ABILITY_SCORCHING_RAY },
    { 3, ABILITY_WEB },

    { 5, ABILITY_FIREBALL },
    { 5, ABILITY_LIGHTNING_BOLT },
    { 5, ABILITY_HASTE },

    { 7, ABILITY_ICE_STORM },
    { 7, ABILITY_GREATER_INVISIBILITY },
    { 7, ABILITY_STONESKIN }
};

static_assert(
    sizeof(wizardMPProgression) /
        sizeof(wizardMPProgression[0]) == MAX_CHARACTER_LEVEL,
    "Wizard MP progression must cover every character level.");

static_assert(
    sizeof(wizardSpellProgression) /
        sizeof(wizardSpellProgression[0]) <= MAX_KNOWN_ABILITIES,
    "Wizard spell progression exceeds fixed known-ability capacity.");

uint8_t getBoundedCharacterLevel(uint8_t level)
{
    if (level < 1)
        return 1;

    return level > MAX_CHARACTER_LEVEL
        ? MAX_CHARACTER_LEVEL
        : level;
}
}

//==================================================
// Experience Progression (Medium)
//==================================================

const uint32_t mediumExperienceProgression[] =
{
    0,       // Level 1
 2000,       // Level 2
 5000,       // Level 3
 9000,       // Level 4
15000,       // Level 5
23000,       // Level 6
35000,       // Level 7
51000,       // Level 8
75000,       // Level 9
105000,       // Level 10
155000,       // Level 11
220000,       // Level 12
315000,       // Level 13
445000,       // Level 14
635000,       // Level 15
890000,       // Level 16
1300000,       // Level 17
1800000,       // Level 18
2550000,       // Level 19
3600000        // Level 20
};

uint32_t getExperienceForLevel(uint8_t level)
{
    if (level <= 1)
        return 0;

    if (level > MAX_CHARACTER_LEVEL)
        level = MAX_CHARACTER_LEVEL;

    return mediumExperienceProgression[level - 1];
}

uint8_t getLevelForExperience(uint32_t experience)
{
    uint8_t level = 1;

    while (level < MAX_CHARACTER_LEVEL &&
           experience >= getExperienceForLevel(level + 1))
    {
        level++;
    }

    return level;
}

bool canLevelUp(const Character& character)
{
    if (character.level >= MAX_CHARACTER_LEVEL)
        return false;

    return character.xp >=
           getExperienceForLevel(character.level + 1);
}

int getMaxMPForCharacter(const Character& character)
{
    if (character.characterClass != CLASS_WIZARD)
        return 0;

    uint8_t level = getBoundedCharacterLevel(character.level);
    return wizardMPProgression[level - 1];
}

int clampCurrentMPForCharacter(
    const Character& character,
    int currentMP)
{
    int maxMP = getMaxMPForCharacter(character);

    if (currentMP < 0)
        return 0;

    return currentMP > maxMP ? maxMP : currentMP;
}

void refreshCharacterMagicProgression(Character& character)
{
    int currentMP = character.magic.currentMP;
    character.magic.maxMP = getMaxMPForCharacter(character);
    character.magic.currentMP = clampCurrentMPForCharacter(
        character, currentMP);

    if (character.characterClass != CLASS_WIZARD)
        return;

    uint8_t level = getBoundedCharacterLevel(character.level);

    for (uint8_t i = 0;
         i < sizeof(wizardSpellProgression) /
                 sizeof(wizardSpellProgression[0]);
         i++)
    {
        if (wizardSpellProgression[i].characterLevel <= level)
            learnAbility(character, wizardSpellProgression[i].ability);
    }
}

static void increasePrimaryClassAbility(Character& character)
{
    uint8_t* ability = nullptr;

    switch (character.characterClass)
    {
        case CLASS_FIGHTER:
            ability = &character.abilities.strength;
            break;

        case CLASS_ROGUE:
            ability = &character.abilities.dexterity;
            break;

        case CLASS_WIZARD:
            ability = &character.abilities.intelligence;
            break;

        case CLASS_CLERIC:
            ability = &character.abilities.wisdom;
            break;
    }

    if (ability != nullptr && *ability < UINT8_MAX)
        (*ability)++;
}

static void applyLevelAdvancement(Character& character, uint8_t newLevel)
{
    character.level = newLevel;

    // Permanent class-based ability advancement happens only while crossing
    // the milestone. It is deliberately separate from derived-stat queries,
    // so recalculating HP, attacks, saves, or the character sheet cannot add
    // the bonus again.
    if (newLevel % 4 == 0)
        increasePrimaryClassAbility(character);

    // The class HP arrays are cumulative. Recompute the maximum for the new
    // level but leave currentHP untouched so existing damage is preserved.
    character.health.maxHP = getMaxHP(character);

    // Like HP damage, spent MP is preserved. Only the derived maximum and
    // newly unlocked known spells change at this level boundary.
    refreshCharacterMagicProgression(character);
}

uint8_t awardExperience(Character& character, uint32_t amount)
{
    if (amount > UINT32_MAX - character.xp)
        character.xp = UINT32_MAX;
    else
        character.xp += amount;

    if (character.level < 1)
        character.level = 1;
    else if (character.level > MAX_CHARACTER_LEVEL)
        character.level = MAX_CHARACTER_LEVEL;

    uint8_t targetLevel = getLevelForExperience(character.xp);

    if (targetLevel <= character.level)
        return 0;

    uint8_t previousLevel = character.level;

    while (character.level < targetLevel)
        applyLevelAdvancement(character, character.level + 1);

    return character.level - previousLevel;
}

static const uint8_t noneProgression[20] =
{
    0,0,0,0,0,
    0,0,0,0,0,
    0,0,0,0,0,
    0,0,0,0,0
  };

static const uint8_t poorProgression[20] =
{
    0,0,0,0,
    1,1,1,1,
    2,2,2,2,
    3,3,3,3,
    4,4,4,5
  };

static const uint8_t averageProgression[20] =
{
    1,1,2,2,
    3,3,4,4,
    5,5,6,6,
    7,7,8,8,
    9,9,10,10
  };

static const uint8_t goodProgression[20] =
{
    1,2,2,3,
    4,4,5,6,
    7,7,8,9,
    10,10,11,12,
    13,13,14,15
  };

static const uint8_t excellentProgression[20] =
{
    1,2,3,4,
    5,6,7,8,
    9,10,11,12,
    13,14,15,16,
    17,18,19,20
  };

//==================================================
// Base Attack Bonus Progression
//==================================================

static const uint8_t highBAB[20] =
{
    1,2,3,4,5,
    6,7,8,9,10,
    11,12,13,14,15,
    16,17,18,19,20
};

static const uint8_t mediumBAB[20] =
{
    0,1,2,3,3,
    4,5,6,6,7,
    8,9,9,10,11,
    12,12,13,14,15
};

static const uint8_t lowBAB[20] =
{
    0,1,1,2,2,
    3,3,4,4,5,
    5,6,6,7,7,
    8,8,9,9,10
};

//==================================================
// Saves Progression
//==================================================
int getBaseSave(
    CharacterClass characterClass,
    SaveType saveType,
    uint8_t level)
{
    if (saveType == SAVE_NONE)
        return 0;

    bool goodSave = false;

    switch (characterClass)
    {
        case CLASS_FIGHTER:
            goodSave = (saveType == SAVE_FORTITUDE);
            break;

        case CLASS_ROGUE:
            goodSave = (saveType == SAVE_REFLEX);
            break;

        case CLASS_WIZARD:
            goodSave = (saveType == SAVE_WILL);
            break;

        case CLASS_CLERIC:
            goodSave =
                (saveType == SAVE_FORTITUDE) ||
                (saveType == SAVE_WILL);
            break;
    }

    if (goodSave)
        return 2 + level / 2;

    return level / 3;
}
//==================================================
// Hit Point Progression
//==================================================

static const uint8_t fighterHP[20] =
{
    10,15,21,26,32,
    37,43,48,54,59,
    65,70,76,81,87,
    92,98,103,109,114
};

static const uint8_t rogueHP[20] =
{
    6,9,13,16,20,
    23,27,30,34,37,
    41,44,48,51,55,
    58,62,65,69,72
};

static const uint8_t wizardHP[20] =
{
    4,6,9,11,14,
    16,19,21,24,26,
    29,31,34,36,39,
    41,44,46,49,51
};

static const uint8_t clericHP[20] =
{
    8,12,17,21,26,
    30,35,39,44,48,
    53,57,62,66,71,
    75,80,84,89,93
};

//==================================================
// Saving Throws
//==================================================

static const uint8_t goodSave[20] =
{
    2,3,3,4,4,
    5,5,6,6,6,
    7,8,8,9,9,
    10,10,11,11,12
};

static const uint8_t poorSave[20] =
{
    0,0,1,1,1,
    2,2,2,3,3,
    3,4,4,4,5,
    5,5,6,6,6
};

//==================================================
// Base Attack Bonus
//==================================================

int getBaseAttackBonus(CharacterClass characterClass, uint8_t level)
{
    if (level < 1) level = 1;
    if (level > MAX_CHARACTER_LEVEL) level = MAX_CHARACTER_LEVEL;

    switch (characterClass)
    {
        case CLASS_FIGHTER:
            return highBAB[level - 1];

        case CLASS_ROGUE:
        case CLASS_CLERIC:
            return mediumBAB[level - 1];

        case CLASS_WIZARD:
            return lowBAB[level - 1];
    }

    return 0;
}

//==================================================
// Base Hit Points
//==================================================

int getBaseHitPoints(CharacterClass characterClass, uint8_t level)
{
    if (level < 1) level = 1;
    if (level > MAX_CHARACTER_LEVEL) level = MAX_CHARACTER_LEVEL;

    switch (characterClass)
    {
        case CLASS_FIGHTER:
            return fighterHP[level - 1];

        case CLASS_ROGUE:
            return rogueHP[level - 1];

        case CLASS_WIZARD:
            return wizardHP[level - 1];

        case CLASS_CLERIC:
            return clericHP[level - 1];
    }

    return 0;
}

//==================================================
// Saving Throws
//==================================================

int getFortitudeSave(CharacterClass characterClass, uint8_t level)
{
    if (level < 1) level = 1;
    if (level > MAX_CHARACTER_LEVEL) level = MAX_CHARACTER_LEVEL;

    switch (characterClass)
    {
        case CLASS_FIGHTER:
        case CLASS_CLERIC:
            return goodSave[level - 1];

        default:
            return poorSave[level - 1];
    }
}

int getReflexSave(CharacterClass characterClass, uint8_t level)
{
    if (level < 1) level = 1;
    if (level > MAX_CHARACTER_LEVEL) level = MAX_CHARACTER_LEVEL;

    switch (characterClass)
    {
        case CLASS_ROGUE:
            return goodSave[level - 1];

        default:
            return poorSave[level - 1];
    }
}

int getWillSave(CharacterClass characterClass, uint8_t level)
{
    if (level < 1) level = 1;
    if (level > MAX_CHARACTER_LEVEL) level = MAX_CHARACTER_LEVEL;

    switch (characterClass)
    {
        case CLASS_WIZARD:
        case CLASS_CLERIC:
            return goodSave[level - 1];

        default:
            return poorSave[level - 1];
    }
}
