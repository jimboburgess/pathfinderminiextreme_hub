#ifndef PATHFINDERMINIEXTREME_025_TRAPS_H
#define PATHFINDERMINIEXTREME_025_TRAPS_H

#include <stdint.h>

#include "characters/abilities.h"
#include "data/game.h"

struct DungeonRoom;
struct Entity;
struct EnvironmentalAbilityContext;

enum TrapID : uint8_t
{
    TRAP_NONE,
    TRAP_SPIKE_PLATE,
    TRAP_ARROW,
    TRAP_POISON_DART,
    TRAP_FIRE,
    TRAP_FROST,
    TRAP_ELECTRIC,
    TRAP_ACID,
    TRAP_GREASE,
    TRAP_WEB,
    TRAP_SLEEP,
    TRAP_COLOR_SPRAY
};

enum SuspicionType : uint8_t
{
    SUSPICION_NONE,

    SUSPICION_CRACKED_FLOOR,
    SUSPICION_RAISED_TILE,
    SUSPICION_DISCOLORED_TILE,
    SUSPICION_FLOOR_GROOVES,
    SUSPICION_SMALL_HOLES,

    SUSPICION_WALL_HOLE,
    SUSPICION_LOOSE_STONE,
    SUSPICION_WALL_SEAM,

    SUSPICION_SCRATCHES,
    SUSPICION_DAMAGED_LOCK,
    SUSPICION_WIRE_OR_STRING,

    SUSPICION_SCORCH_MARK,
    SUSPICION_FROST,
    SUSPICION_CORROSION,
    SUSPICION_OZONE_MARK,

    SUSPICION_BONES,
    SUSPICION_BLOODSTAIN,
    SUSPICION_DISTURBED_DUST
};

struct TrapDefinition
{
    TrapID id;
    const char* name;

    uint8_t minimumLevel;
    uint8_t basePerceptionDC;
    uint8_t baseDisableDC;
    uint8_t baseHP;
    uint8_t hardness;

    DamageType damageType;
    uint8_t damageDice;
    uint8_t damageSides;
    SaveType saveType;
    AbilityID abilityId;
};

struct TrapInstance
{
    TrapID id = TRAP_NONE;
    int8_t x = -1;
    int8_t y = -1;
    // Projectile traps keep their pressure plate at x/y and their launcher in
    // a nearby wall. Spike plates simply leave these at their defaults.
    int8_t sourceX = -1;
    int8_t sourceY = -1;
    Direction direction = DIR_NORTH;
    uint8_t level = 1;
    int16_t hp = 0;

    bool discovered = false;
    bool disabled = false;
    bool triggered = false;
    bool charging = false;
    bool destroyed = false;
    uint8_t chargingCombatRound = 0;
    uint32_t chargingUntilMillis = 0;

    // Manual searching and Rogue trapfinding are separate opportunities, but
    // neither can be repeated by reopening a menu or leaving detection range.
    bool rogueDiscoveryAttempted = false;
    bool manualPerceptionAttempted = false;

    SuspicionType suspicion = SUSPICION_NONE;
    uint8_t controlGroup = 0;
};

// Rendering derives directly from persistent trap state; no visual state is
// stored separately in the dungeon runtime.
enum TrapVisualState : uint8_t
{
    TRAP_VISUAL_HIDDEN,
    TRAP_VISUAL_ARMED,
    TRAP_VISUAL_CHARGING,
    TRAP_VISUAL_TRIGGERED,
    TRAP_VISUAL_DISABLED,
    TRAP_VISUAL_DESTROYED
};

// Compatibility with the small authored-trap foundation that preceded the
// reusable definition/instance split.
using DungeonTrap = TrapInstance;

struct SuspicionInstance
{
    SuspicionType type = SUSPICION_NONE;
    int8_t x = -1;
    int8_t y = -1;
};

constexpr uint8_t MAX_TRAPS_PER_ROOM = 8;
constexpr uint8_t MAX_SUSPICIONS_PER_ROOM = 8;
constexpr uint8_t TRAP_OBJECT_AC = 5;

enum TrapDiscoveryResult : uint8_t
{
    TRAP_DISCOVERY_NOT_ATTEMPTED,
    TRAP_DISCOVERY_FAILED,
    TRAP_DISCOVERY_SUCCESS,
    TRAP_DISCOVERY_ALREADY_KNOWN
};

enum TrapDisableResult : uint8_t
{
    TRAP_DISABLE_NOT_ATTEMPTED,
    TRAP_DISABLE_FAILED,
    TRAP_DISABLE_SUCCESS,
    TRAP_DISABLE_ALREADY_INACTIVE
};

struct TrapTriggerResult
{
    bool triggered = false;
    bool saveSucceeded = false;
    uint16_t damage = 0;
};

struct TrapDamageResult
{
    uint16_t incomingDamage = 0;
    uint16_t hardnessPrevented = 0;
    uint16_t appliedDamage = 0;
    // True only when this application reduces the trap to zero HP.
    bool destroyed = false;
};

const TrapDefinition* getTrapDefinition(TrapID id);

// Percentile is deterministic input in the inclusive conceptual range 0..99.
// Values outside it wrap, which keeps the helper safe for byte-sized RNGs.
uint8_t selectTrapLevel(uint8_t challengeLevel, uint8_t percentile);
TrapID selectGeneratedTrapID(
    uint8_t challengeLevel,
    uint8_t categoryPercentile,
    uint8_t variant);

uint8_t getTrapPerceptionDC(const TrapInstance& trap);
uint8_t getTrapDisableDC(const TrapInstance& trap);
uint16_t getTrapMaxHP(const TrapInstance& trap);
uint8_t getTrapSaveDC(const TrapInstance& trap);
uint8_t getTrapDamageDice(const TrapInstance& trap);
uint8_t getTrapDamageSides(const TrapInstance& trap);

TrapInstance* getTrapAt(DungeonRoom& room, int x, int y);
const TrapInstance* getTrapAt(const DungeonRoom& room, int x, int y);
bool addTrap(
    DungeonRoom& room,
    TrapID id,
    int x,
    int y,
    uint8_t level,
    SuspicionType suspicion = SUSPICION_NONE,
    uint8_t controlGroup = 0);
bool configureProjectileTrap(
    TrapInstance& trap, int sourceX, int sourceY, Direction direction);
bool isProjectileTrap(const TrapInstance& trap);
bool isElementalTrap(const TrapInstance& trap);
bool isSpellTrap(const TrapInstance& trap);
AbilityID getTrapAbilityID(const TrapInstance& trap);
bool configureDirectionalTrap(
    TrapInstance& trap, int sourceX, int sourceY, Direction direction);
bool buildEnvironmentalAbilityContext(
    const TrapInstance& trap,
    EnvironmentalAbilityContext& context);
void updateElementalTrapCharges();

SuspicionType getSuspicionAt(const DungeonRoom& room, int x, int y);
bool addSuspicion(
    DungeonRoom& room,
    SuspicionType type,
    int x,
    int y);

bool isTrapDiscovered(const DungeonRoom& room, int x, int y);
bool isTrapActive(const TrapInstance& trap);
TrapVisualState getTrapVisualState(const TrapInstance& trap);

TrapDiscoveryResult attemptManualTrapDiscovery(
    TrapInstance& trap,
    int perceptionTotal);
TrapDiscoveryResult attemptRogueTrapDiscovery(
    TrapInstance& trap,
    int perceptionTotal);

const char* getTrapSearchFailureMessage(uint8_t index);

TrapTriggerResult resolveTrapTrigger(
    TrapInstance& trap,
    bool saveSucceeded,
    int rolledDamage);
TrapDamageResult damageTrap(
    TrapInstance& trap,
    int rawDamage,
    DamageType damageType);
TrapDisableResult attemptDisableTrap(
    TrapInstance& trap,
    int disableTotal);

// Runtime integration stays thin: these helpers reuse the generated room,
// active-map visibility, shared dice/save/damage, rendering, and message paths.
uint8_t randomTrapLevel(uint8_t challengeLevel);
void populateDungeonRoomFeatures(
    DungeonRoom& room,
    uint8_t challengeLevel,
    bool allowRandomTrap = true);
void updateRogueTrapAwareness(Entity& player);
bool triggerTrapForEntityAt(Entity& mover, int targetX, int targetY);

#endif // PATHFINDERMINIEXTREME_025_TRAPS_H
