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
    CONDITION_WEBBED,

    // Buffs
    CONDITION_BLESSED,
    CONDITION_HASTED,
    CONDITION_BLUR,
    CONDITION_MAGE_ARMOR,
    CONDITION_BARKSKIN,
    CONDITION_BUFF_AC,
    CONDITION_BUFF_ATTACK,
    CONDITION_BUFF_SAVE,
    CONDITION_BULLS_STRENGTH,
    CONDITION_CATS_GRACE,
    CONDITION_EAGLES_SPLENDOR,
    CONDITION_OWLS_WISDOM,
    CONDITION_DIVINE_FAVOR,
    CONDITION_SHIELD_OF_FAITH,
    CONDITION_HEROISM,
    CONDITION_BANE,
    CONDITION_ENFEEBLED,
    CONDITION_SLOWED,

    // Long-term
    CONDITION_CURSED,
    CONDITION_DISEASED,

    // Movement
    CONDITION_PRONE,

    CONDITION_MAX
};

// Compact aggregate view of all timed condition bonuses.
struct ConditionModifiers
{
    int8_t attackBonus = 0;
    int8_t damageBonus = 0;
    int8_t acBonus = 0;
    int8_t saveBonus = 0;
    int8_t strBonus = 0;
    int8_t dexBonus = 0;
    int8_t conBonus = 0;
    int8_t intBonus = 0;
    int8_t wisBonus = 0;
    int8_t chaBonus = 0;
    int8_t speedBonus = 0;
    uint8_t bonusAttacks = 0;
};

struct Condition
{
    ConditionType type = CONDITION_NONE;
    int value = 0;
    int roundsRemaining = 0;
    ConditionModifiers modifiers{};
};

constexpr uint8_t MAX_CONDITIONS_PER_CHARACTER = 12;
constexpr uint8_t MAX_TIMED_DAMAGE_EFFECTS = 4;
constexpr uint8_t MAX_ENERGY_RESISTANCES = 5;
constexpr uint8_t MAX_ENERGY_PROTECTIONS = 4;

// DamageType deliberately remains owned by abilities.h. Store its compact
// numeric value here to keep the condition layer independent of spell data.
struct TimedDamageEffect
{
    uint8_t damageType = 0;
    uint8_t diceCount = 0;
    uint8_t diceSides = 0;
    uint8_t roundsRemaining = 0;
    uint16_t sourceAbility = 0;
};

struct EnergyResistance
{
    uint8_t damageType = 0;
    int16_t amount = 0;
    uint8_t roundsRemaining = 0;
};

struct EnergyProtection
{
    uint8_t damageType = 0;
    int16_t remainingAbsorption = 0;
    uint8_t roundsRemaining = 0;
};

struct ConditionData
{
    Condition conditions[MAX_CONDITIONS_PER_CHARACTER];
    uint8_t count = 0;
    TimedDamageEffect timedDamage[MAX_TIMED_DAMAGE_EFFECTS];
    uint8_t timedDamageCount = 0;
    EnergyResistance energyResistances[MAX_ENERGY_RESISTANCES];
    uint8_t energyResistanceCount = 0;
    EnergyProtection energyProtections[MAX_ENERGY_PROTECTIONS];
    uint8_t energyProtectionCount = 0;
};

// Generic information about effects resolved at the start of one creature's
// turn.  Entity/UI work remains with combat; condition damage and duration
// advancement remain in the condition system.
struct ConditionTurnResult
{
    int damage = 0;
    ConditionType damageCondition = CONDITION_NONE;
    uint8_t damageType = 0;
    bool poisonExpired = false;
    bool actionPrevented = false;
    TimedDamageEffect timedDamage[MAX_TIMED_DAMAGE_EFFECTS];
    uint8_t timedDamageCount = 0;
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
bool addCondition(Character& character,
                  ConditionType type,
                  const ConditionModifiers& modifiers,
                  int rounds);

bool removeCondition(Character& character, ConditionType type);

bool addTimedDamageEffect(Character& character,
                          const TimedDamageEffect& effect);
bool addEnergyResistance(Character& character,
                         uint8_t damageType,
                         int amount,
                         int rounds);
int getEnergyResistance(const Character& character, uint8_t damageType);
bool addEnergyProtection(Character& character,
                         uint8_t damageType,
                         int amount,
                         int rounds);
int getEnergyProtection(const Character& character, uint8_t damageType);
int absorbEnergyProtection(Character& character,
                           uint8_t damageType,
                           int incomingDamage);
int applyEnergyMitigation(Character& character,
                          uint8_t damageType,
                          int incomingDamage);

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
int getConditionSaveModifier(const Character& character);
ConditionModifiers getActiveConditionModifiers(const Character& character);

// Returns player-facing start-of-turn feedback for persistent conditions that
// materially change available actions. Add future condition summaries here.
const char* getActionAffectingConditionMessage(const Character& character);

#endif //PATHFINDERMINIEXTREME_025_CONDITIONS_H
