//
// Created by james on 7/13/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ABILITIES_H
#define PATHFINDERMINIEXTREME_025_ABILITIES_H

#include <stdint.h>

struct Character;

//--------------------------------------------------
// Ability IDs
//--------------------------------------------------

enum AbilityID
{
    ABILITY_NONE,

    //------------------------------------------------
    // Universal Abilities
    //------------------------------------------------

    ABILITY_FIRE_BREATH,
    ABILITY_HEAL,
    ABILITY_PARALYZE,
    ABILITY_POISON,
    ABILITY_REGENERATION,
    ABILITY_SUMMON,
    ABILITY_MELEE_ATTACK,
    ABILITY_RANGED_ATTACK,

    //------------------------------------------------
    // Class Features
    //------------------------------------------------

    ABILITY_CHANNEL_ENERGY,
    ABILITY_POWER_ATTACK,


    //------------------------------------------------
    // Cantrips
    //------------------------------------------------

    ABILITY_ACID_SPLASH,
    ABILITY_DISRUPT_UNDEAD,
    ABILITY_GUIDANCE,
    ABILITY_JOLT,
    ABILITY_LIGHT,
    ABILITY_RAY_OF_FROST,
    ABILITY_RESISTANCE,
    ABILITY_SPARK,

    //------------------------------------------------
    // Level 1
    //------------------------------------------------

    ABILITY_BANE,
    ABILITY_BLESS,
    ABILITY_BURNING_HANDS,
    ABILITY_CAUSE_FEAR,
    ABILITY_COLOR_SPRAY,
    ABILITY_CURE_LIGHT_WOUNDS,
    ABILITY_DOOM,
    ABILITY_ENLARGE_PERSON,
    ABILITY_EXPEDITIOUS_RETREAT,
    ABILITY_GREASE,
    ABILITY_MAGE_ARMOR,
    ABILITY_MAGIC_MISSILE,
    ABILITY_OBSCURING_MIST,
    ABILITY_RAY_OF_ENFEEBLEMENT,
    ABILITY_REDUCE_PERSON,
    ABILITY_REMOVE_FEAR,
    ABILITY_SHIELD,
    ABILITY_SHOCKING_GRASP,
    ABILITY_SLEEP,
    ABILITY_SNOWBALL,

    //------------------------------------------------
    // Level 2
    //------------------------------------------------

    ABILITY_ACID_ARROW,
    ABILITY_BEARS_ENDURANCE,
    ABILITY_BLINDNESS,
    ABILITY_BLUR,
    ABILITY_BULLS_STRENGTH,
    ABILITY_CATS_GRACE,
    ABILITY_CURE_MODERATE_WOUNDS,
    ABILITY_EAGLES_SPLENDOR,
    ABILITY_FLAMING_SPHERE,
    ABILITY_GLITTERDUST,
    ABILITY_HOLD_PERSON,
    ABILITY_LESSER_RESTORATION,
    ABILITY_OWLS_WISDOM,
    ABILITY_RESIST_ENERGY,
    ABILITY_SCORCHING_RAY,
    ABILITY_SHATTER,
    ABILITY_TOUCH_OF_IDIOCY,
    ABILITY_WEB,

    //------------------------------------------------
    // Level 3
    //------------------------------------------------

    ABILITY_BESTOW_CURSE,
    ABILITY_CURE_SERIOUS_WOUNDS,
    ABILITY_DEEP_SLUMBER,
    ABILITY_FIREBALL,
    ABILITY_HASTE,
    ABILITY_HEROISM,
    ABILITY_LIGHTNING_BOLT,
    ABILITY_MAGIC_CIRCLE,
    ABILITY_PROTECTION_FROM_ENERGY,
    ABILITY_REMOVE_DISEASE,
    ABILITY_SEARING_LIGHT,
    ABILITY_SLOW,
    ABILITY_STINKING_CLOUD,
    ABILITY_VAMPIRIC_TOUCH,

    //------------------------------------------------
    // Level 4
    //------------------------------------------------

    ABILITY_BLACK_TENTACLES,
    ABILITY_CONFUSION,
    ABILITY_CURE_CRITICAL_WOUNDS,
    ABILITY_FEAR,
    ABILITY_FIRE_SHIELD,
    ABILITY_FREEDOM_OF_MOVEMENT,
    ABILITY_GREATER_INVISIBILITY,
    ABILITY_ICE_STORM,
    ABILITY_RESTORATION,
    ABILITY_STONESKIN,
    ABILITY_WALL_OF_FIRE,

    //------------------------------------------------
    // Level 5+
    //------------------------------------------------

    ABILITY_CHAIN_LIGHTNING,
    ABILITY_CLOUDKILL,
    ABILITY_CONE_OF_COLD,
    ABILITY_DOMINATE_PERSON,
    ABILITY_FLAME_STRIKE,
    ABILITY_GREATER_HEROISM,
    ABILITY_HOLD_MONSTER,
    ABILITY_MASS_CURE_LIGHT_WOUNDS,
    ABILITY_MIND_BLANK,
    ABILITY_REGENERATE,
    ABILITY_TRUE_SEEING,
    ABILITY_WALL_OF_STONE,

    ABILITY_MAX
};

enum AbilityType
{
    ABILITY_ARCANE,
    ABILITY_DIVINE,
    ABILITY_MONSTER,
    ABILITY_MARTIAL
};

// What kind of gameplay ability this is. AbilityType remains the ability's
// tradition (arcane, divine, martial, and so on).
enum AbilityCategory : uint8_t
{
    // Kept at zero so existing spell rows can use aggregate
    // zero-initialization for the final Ability field.
    ABILITY_CATEGORY_SPELL = 0,
    ABILITY_CATEGORY_ATTACK,
    ABILITY_CATEGORY_CLASS_FEATURE,
    ABILITY_CATEGORY_MONSTER,
    ABILITY_CATEGORY_NONE
};

//--------------------------------------------------
// Actions
//--------------------------------------------------

enum AbilityAction
{
    ACTION_STANDARD,
    ACTION_MOVE,
    ACTION_SWIFT,
    ACTION_FULL_ROUND,
    ACTION_FREE
};

//--------------------------------------------------
// Delivery
//--------------------------------------------------

enum AbilityDelivery
{
    DELIVERY_AUTOMATIC,
    DELIVERY_TOUCH,
    DELIVERY_RANGED_TOUCH,
    DELIVERY_TARGET,
    DELIVERY_CONE,
    DELIVERY_AREA,
    DELIVERY_LINE,
    DELIVERY_SAVING_THROW
};

//--------------------------------------------------
// Targeting
//--------------------------------------------------

enum AbilityTarget
{
    TARGET_SELF,
    TARGET_ALLY,
    TARGET_ENEMY,
    TARGET_AREA,
    TARGET_ALL_ALLIES,
    TARGET_ALL_ALLIES_AREA,
    TARGET_ALL_ENEMIES,
    TARGET_EMPTY_TILE,
    TARGET_ALL_ENEMIES_AREA
};

//--------------------------------------------------
// Duration
//--------------------------------------------------

enum AbilityDuration
{
    DURATION_INSTANT,
    DURATION_ROUNDS,
    DURATION_COMBAT,
    DURATION_PERMANENT
};

//--------------------------------------------------
// Effects
//--------------------------------------------------

enum AbilityEffect
{
    EFFECT_NONE,
    EFFECT_DAMAGE,
    EFFECT_HEAL,

    EFFECT_BUFF_AC,
    EFFECT_BUFF_ATTACK,
    EFFECT_BUFF_DAMAGE,
    EFFECT_BUFF_SPEED,
    EFFECT_BUFF_STR,
    EFFECT_BUFF_DEX,
    EFFECT_BUFF_CON,
    EFFECT_BUFF_SAVE,
    EFFECT_BUFF_MISS_CHANCE,
    EFFECT_DAMAGE_RESISTANCE,

    EFFECT_DEBUFF_ATTACK,
    EFFECT_DEBUFF_DAMAGE,
    EFFECT_DEBUFF_SPEED,
    EFFECT_DEBUFF_AC,
    EFFECT_DEBUFF_STR,
    EFFECT_DEBUFF_DEX,

    EFFECT_SLEEP,
    EFFECT_BLIND,
    EFFECT_STUN,
    EFFECT_PARALYZE,
    EFFECT_CONFUSE,
    EFFECT_FEAR,
    EFFECT_PRONE,
    EFFECT_ENTANGLED,
    EFFECT_INVISIBLE,

    EFFECT_TEMP_HP,
    EFFECT_REMOVE_CONDITION,

    EFFECT_BURN,
    EFFECT_POISON,

    EFFECT_SUMMON
};

enum DamageType
{
    DAMAGE_NONE,

    DAMAGE_PHYSICAL,
    DAMAGE_FIRE,
    DAMAGE_COLD,
    DAMAGE_ACID,
    DAMAGE_ELECTRIC,
    DAMAGE_FORCE,
    DAMAGE_POSITIVE,
    DAMAGE_NEGATIVE,
    DAMAGE_SONIC,
    DAMAGE_BLUDGEONING,
    DAMAGE_PIERCING,
    DAMAGE_SLASHING
};

enum AbilityFlags
{
    FLAG_NONE          = 0,
    FLAG_REQUIRES_HIT  = 1 << 0,
    FLAG_ALLOW_SAVE    = 1 << 1,
    FLAG_FRIENDLY_FIRE = 1 << 2,
    FLAG_CONCENTRATION = 1 << 3,
    FLAG_IGNORE_ARMOR  = 1 << 4
};

enum AbilityAnimation
{
    ANIM_NONE,

    ANIM_MAGIC_MISSILE,
    ANIM_FIREBALL,
    ANIM_ACID_SPLASH,
    ANIM_LIGHTNING,

    ANIM_HEAL,
    ANIM_BLESS,

    ANIM_SLEEP,
    ANIM_BLUR,

    ANIM_SLASH,
    ANIM_BITE,
    ANIM_POISON,
    ANIM_FEAR,
    ANIM_BREATH
};

//--------------------------------------------------
// Ability Definition
//--------------------------------------------------

constexpr uint8_t MAX_KNOWN_ABILITIES = 16;
constexpr uint8_t MAX_ABILITY_EFFECTS = 3;


struct AbilityEffectData
{
    AbilityEffect effect;
    DamageType damageType;

    int baseValue;        // Base value
    int valuePerLevel;  // Scaling per caster level

    int duration;
};

struct Ability
{
    AbilityID id;
    const char* name;

    AbilityType type;

    uint8_t level;
    uint8_t mpCost;

    AbilityAction action;
    AbilityDelivery delivery;
    AbilityTarget target;
    AbilityDuration duration;

    AbilityEffectData effects[MAX_ABILITY_EFFECTS];
    uint8_t effectCount;

    AbilityAnimation animation;

    // Arcane/divine spell rows usually omit this and therefore initialize to
    // ABILITY_CATEGORY_SPELL; non-spell rows specify it explicitly.
    AbilityCategory category;

    // Zero means no ranged single-target support in the first resolver.
    // Existing definitions keep zero through aggregate initialization.
    uint8_t rangeTiles;
};

extern const Ability abilityDatabase[];

const Ability* getAbility(AbilityID id);

const char* getAbilityName(AbilityID id);

bool isValidAbility(AbilityID id);

bool knowsAbility(const Character& character, AbilityID id);
bool learnAbility(Character& character, AbilityID id);
bool forgetAbility(Character& character, AbilityID id);

// One-shot character creation initialization. Level refreshes use the
// idempotent progression helper instead of clearing known abilities.
void initializeCharacterMagic(Character& character);

// One-shot load initialization using persisted current MP and, for current
// saves, the bounded known-ability list. Class progression is merged afterward
// so mandatory class spells remain present without erasing scroll learning.
void restoreCharacterMagic(
    Character& character,
    int savedCurrentMP,
    const AbilityID* savedKnownAbilities = nullptr,
    uint8_t savedKnownAbilityCount = 0);

#endif
