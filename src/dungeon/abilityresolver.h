#ifndef PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H
#define PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H

#include <stdint.h>

#include "characters/abilities.h"
#include "data/game.h"

struct Entity;
struct Character;

enum AbilityResult : uint8_t
{
    ABILITY_RESULT_SUCCESS,
    ABILITY_RESULT_INVALID_ABILITY,
    ABILITY_RESULT_UNSUPPORTED,
    ABILITY_RESULT_INVALID_CASTER,
    ABILITY_RESULT_INVALID_TARGET,
    ABILITY_RESULT_TARGET_IMMUNE,
    ABILITY_RESULT_OUT_OF_RANGE,
    ABILITY_RESULT_NO_LINE_OF_SIGHT,
    ABILITY_RESULT_NOT_ENOUGH_MP,
    ABILITY_RESULT_NO_STANDARD_ACTION,
    ABILITY_RESULT_CONDITION_LIMIT,
    ABILITY_RESULT_MAP_EFFECT_LIMIT
};

enum SaveResult : uint8_t
{
    SAVE_RESULT_NOT_REQUIRED,
    SAVE_RESULT_SUCCESS,
    SAVE_RESULT_FAILURE
};

enum class AbilityCastSource : uint8_t
{
    NORMAL,
    SCROLL
};

enum class EnergyInteraction : uint8_t
{
    NONE,
    DAMAGE,
    HEAL
};

struct AbilitySavingThrow
{
    SaveResult result = SAVE_RESULT_NOT_REQUIRED;
    int roll = 0;
    int bonus = 0;
    int total = 0;
    int dc = 0;
};

struct AbilityAttackRoll
{
    bool required = false;
    bool hit = false;
    int roll = 0;
    int bonus = 0;
    int total = 0;
    int targetAC = 0;
};

struct AbilityResolution
{
    AbilityResult result = ABILITY_RESULT_INVALID_ABILITY;
    int damage = 0;
    int healing = 0;
    ConditionType conditionApplied = CONDITION_NONE;
    int conditionDuration = 0;
    AbilitySavingThrow savingThrow;
    AbilityAttackRoll attackRoll;
    bool targetDefeated = false;
    uint8_t levelReached = 0;
    bool mapEffectCreated = false;
    uint8_t targetsAffected = 0;
    uint8_t targetsResisted = 0;
    uint8_t targetsImmune = 0;
    DamageType resistanceType = DAMAGE_NONE;
    int resistanceAmount = 0;
    DamageType protectionType = DAMAGE_NONE;
    int protectionAmount = 0;
};

struct EnvironmentalAbilityContext
{
    int8_t sourceX = -1;
    int8_t sourceY = -1;
    Direction direction = DIR_NORTH;
    uint8_t effectiveLevel = 1;
    int16_t saveDC = 10;
};

// The resolver deliberately accepts only standard-action abilities handled
// by the current narrow feature set: entity-target instant damage/healing or
// one timed condition, one persistent ground/area effect, and the supported
// directional/cone effect profile.
bool isAbilitySupported(AbilityID abilityID);
bool isGroundTargetAbility(AbilityID abilityID);
bool isDirectionalAbility(AbilityID abilityID);
bool canResolveEnvironmentally(AbilityID abilityID);
AbilityResolution resolveEnvironmentalAbility(
    const EnvironmentalAbilityContext& context, AbilityID abilityID);
bool isTileInDirectionalAbilityAreaFromSource(
    int sourceX, int sourceY, AbilityID abilityID, Direction direction,
    int tileX, int tileY);
EnergyInteraction getEnergyInteraction(
    DamageType energyType,
    CreatureType creatureType);
bool isAbilityEffectHostileToTarget(
    const Ability& ability,
    const Character& target);

// Shared cone geometry for both rendering and execution. The predicate
// includes map bounds and line of effect, so previewed tiles are authoritative.
bool isTileInDirectionalAbilityArea(
    const Entity& caster,
    AbilityID abilityID,
    Direction direction,
    int tileX,
    int tileY);

// Shared player/monster saving-throw helpers. No natural-1/natural-20 rule is
// added here because the existing game does not define one for saves.
int getAbilitySaveDC(const Entity& caster, const Ability& ability);
int getAbilitySaveBonus(const Character& target, SaveType saveType);
AbilitySavingThrow resolveAbilitySavingThrow(
    const Entity& caster,
    const Entity& target,
    const Ability& ability);

// Lower-level shared save roller used by persistent map effects after their
// original caster's DC has been captured at cast time.
AbilitySavingThrow resolveSavingThrow(
    const Character& target,
    SaveType saveType,
    int dc);

bool canPayAbilityCost(
    const Character& caster,
    const Ability& ability,
    AbilityCastSource source);
void payAbilityCost(
    Character& caster,
    const Ability& ability,
    AbilityCastSource source);

// Performs every legality check without changing either entity.
AbilityResult validateAbility(
    const Entity& caster,
    const Entity* target,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL,
    DamageType selectedDamageType = DAMAGE_NONE);

// Authoritative execution path shared by player and monster spellcasting.
AbilityResolution resolveAbility(
    Entity& caster,
    Entity* target,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL,
    DamageType selectedDamageType = DAMAGE_NONE);

// Pathfinder-inspired strength shared by every spell/item that later applies
// this generic resistance effect.
int getEnergyResistanceAmountForCasterLevel(int casterLevel);
int getEnergyProtectionAmountForCasterLevel(int casterLevel);

// Coordinate-target counterpart used only by supported ground/area
// abilities. Existing entity-target calls continue using resolveAbility().
AbilityResult validateAbilityAt(
    const Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL);
AbilityResolution resolveAbilityAt(
    Entity& caster,
    int targetX,
    int targetY,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL);

// Directional counterpart used by cone abilities. Current support is Color
// Spray; the geometry and target enumeration are reusable by future cones.
AbilityResult validateDirectionalAbility(
    const Entity& caster,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL);
AbilityResolution resolveAbilityInDirection(
    Entity& caster,
    Direction direction,
    AbilityID abilityID,
    AbilityCastSource source = AbilityCastSource::NORMAL);

const char* getAbilityResultMessage(AbilityResult result);

#endif // PATHFINDERMINIEXTREME_025_ABILITY_RESOLVER_H
