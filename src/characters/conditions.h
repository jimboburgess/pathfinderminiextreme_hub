//
// Created by james on 7/25/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_CONDITIONS_H
#define PATHFINDERMINIEXTREME_025_CONDITIONS_H

#include <stdint.h>

struct Character;

enum ConditionType
{
    CONDITION_NONE,

    // Combat
    CONDITION_FLAT_FOOTED,
    CONDITION_POISONED,
    CONDITION_BURNING,
    CONDITION_SLEEPING,
    CONDITION_STUNNED,
    CONDITION_PARALYZED,
    CONDITION_BLINDED,
    CONDITION_CONFUSED,
    CONDITION_FRIGHTENED,

    // Buffs
    CONDITION_BLESSED,
    CONDITION_HASTED,
    CONDITION_BLUR,
    CONDITION_MAGE_ARMOR,
    CONDITION_BARKSKIN,

    // Long-term
    CONDITION_CURSED,
    CONDITION_DISEASED,

    // Movement
    CONDITION_PRONE,

    CONDITION_MAX
};

struct Condition
{
    ConditionType type = CONDITION_NONE;

    // The magnitude is condition-specific, such as an attack penalty or
    // an armor class bonus.
    int value = 0;

    // Zero means the condition does not expire from turn ticking.
    int roundsRemaining = 0;
};

constexpr uint8_t MAX_CONDITIONS_PER_CHARACTER = 12;

struct ConditionData
{
    Condition conditions[MAX_CONDITIONS_PER_CHARACTER];
    uint8_t count = 0;
};

// Generic information about effects resolved at the start of one creature's
// turn.  Entity/UI work remains with combat; condition damage and duration
// advancement remain in the condition system.
struct ConditionTurnResult
{
    int damage = 0;
    ConditionType damageCondition = CONDITION_NONE;
    bool poisonExpired = false;
    bool actionPrevented = false;
};

bool hasCondition(const Character& character, ConditionType type);

bool isValidConditionType(ConditionType type);

// Generic condition eligibility hook used by ability resolution. The first
// immunity rule is intentionally narrow: undead cannot be put to sleep.
bool canReceiveCondition(
    const Character& character,
    ConditionType type);

Condition* getCondition(Character& character, ConditionType type);
const Condition* getCondition(const Character& character,
                              ConditionType type);

bool addCondition(Character& character,
                  ConditionType type,
                  int value,
                  int rounds);

bool removeCondition(Character& character, ConditionType type);

void clearConditions(Character& character);

// Central reaction hook for condition changes caused by actual damage.
// Zero or negative damage deliberately has no effect.
void updateConditionsAfterDamage(Character& character, int damage);

// Advance positive condition durations once.  Zero-duration conditions
// remain until explicitly removed.
void tickConditions(Character& character);

// Call once when a living creature's combat turn begins.  Resolves generic
// start-of-turn effects, then advances timed conditions exactly once.
ConditionTurnResult processConditionsAtTurnStart(Character& character);

bool canCharacterAct(const Character& character);

int getConditionAttackModifier(const Character& character);
int getConditionArmorClassModifier(const Character& character);

#endif //PATHFINDERMINIEXTREME_025_CONDITIONS_H
