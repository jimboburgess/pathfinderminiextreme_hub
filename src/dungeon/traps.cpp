#include "dungeon/traps.h"

#include "dungeon/dungeon.h"

namespace
{
constexpr uint8_t MIN_TRAP_LEVEL = 1;
constexpr uint8_t MAX_TRAP_LEVEL = 20;

const TrapDefinition SPIKE_PLATE_DEFINITION = {
    TRAP_SPIKE_PLATE,
    "pressure plate",
    1,  // minimumLevel
    12, // basePerceptionDC
    14, // baseDisableDC
    8,  // baseHP
    2,  // hardness
    DAMAGE_PIERCING,
    1,  // damageDice
    6,  // damageSides
    SAVE_REFLEX
};

uint8_t clampTrapLevel(int level)
{
    if (level < MIN_TRAP_LEVEL)
        return MIN_TRAP_LEVEL;
    if (level > MAX_TRAP_LEVEL)
        return MAX_TRAP_LEVEL;
    return static_cast<uint8_t>(level);
}

uint8_t effectiveTrapLevel(const TrapInstance& trap)
{
    return clampTrapLevel(trap.level);
}

uint8_t clampToByte(unsigned value)
{
    return static_cast<uint8_t>(value > 255U ? 255U : value);
}

uint16_t clampPositiveDamage(int damage)
{
    if (damage <= 0)
        return 0;
    if (damage > 65535)
        return 65535;
    return static_cast<uint16_t>(damage);
}

bool isInsideRoom(int x, int y)
{
    return x >= 0 && x < ROOM_SIZE && y >= 0 && y < ROOM_SIZE;
}

TrapDiscoveryResult attemptTrapDiscovery(
    TrapInstance& trap,
    int perceptionTotal,
    bool& attempted)
{
    if (getTrapDefinition(trap.id) == nullptr)
        return TRAP_DISCOVERY_NOT_ATTEMPTED;

    if (trap.discovered)
        return TRAP_DISCOVERY_ALREADY_KNOWN;

    if (attempted)
        return TRAP_DISCOVERY_NOT_ATTEMPTED;

    attempted = true;

    if (perceptionTotal < getTrapPerceptionDC(trap))
        return TRAP_DISCOVERY_FAILED;

    trap.discovered = true;
    return TRAP_DISCOVERY_SUCCESS;
}
}

const TrapDefinition* getTrapDefinition(TrapID id)
{
    switch (id)
    {
        case TRAP_SPIKE_PLATE:
            return &SPIKE_PLATE_DEFINITION;

        case TRAP_NONE:
        default:
            return nullptr;
    }
}

uint8_t selectTrapLevel(uint8_t challengeLevel, uint8_t percentile)
{
    const uint8_t roll = percentile % 100;
    int offset = 0;

    if (roll < 23)
        offset = -2;
    else if (roll < 45)
        offset = -1;
    else if (roll < 68)
        offset = 0;
    else if (roll < 90)
        offset = 1;
    else if (roll < 95)
        offset = 2;
    else
        offset = 3;

    return clampTrapLevel(static_cast<int>(challengeLevel) + offset);
}

uint8_t getTrapPerceptionDC(const TrapInstance& trap)
{
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    if (definition == nullptr)
        return 0;

    return clampToByte(
        static_cast<unsigned>(definition->basePerceptionDC) +
        effectiveTrapLevel(trap));
}

uint8_t getTrapDisableDC(const TrapInstance& trap)
{
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    if (definition == nullptr)
        return 0;

    return clampToByte(
        static_cast<unsigned>(definition->baseDisableDC) +
        effectiveTrapLevel(trap));
}

uint16_t getTrapMaxHP(const TrapInstance& trap)
{
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    if (definition == nullptr)
        return 0;

    return static_cast<uint16_t>(definition->baseHP) +
        static_cast<uint16_t>(effectiveTrapLevel(trap) - 1) * 2U;
}

uint8_t getTrapSaveDC(const TrapInstance& trap)
{
    if (getTrapDefinition(trap.id) == nullptr)
        return 0;

    return static_cast<uint8_t>(
        10U + (static_cast<unsigned>(effectiveTrapLevel(trap)) + 1U) / 2U);
}

uint8_t getTrapDamageDice(const TrapInstance& trap)
{
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    if (definition == nullptr)
        return 0;

    return clampToByte(
        static_cast<unsigned>(definition->damageDice) +
        (effectiveTrapLevel(trap) - 1U) / 4U);
}

uint8_t getTrapDamageSides(const TrapInstance& trap)
{
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    return definition != nullptr ? definition->damageSides : 0;
}

TrapInstance* getTrapAt(DungeonRoom& room, int x, int y)
{
    if (!isInsideRoom(x, y))
        return nullptr;

    for (TrapInstance& trap : room.traps)
    {
        if (trap.id != TRAP_NONE && trap.x == x && trap.y == y)
            return &trap;
    }

    return nullptr;
}

const TrapInstance* getTrapAt(const DungeonRoom& room, int x, int y)
{
    if (!isInsideRoom(x, y))
        return nullptr;

    for (const TrapInstance& trap : room.traps)
    {
        if (trap.id != TRAP_NONE && trap.x == x && trap.y == y)
            return &trap;
    }

    return nullptr;
}

bool addTrap(
    DungeonRoom& room,
    TrapID id,
    int x,
    int y,
    uint8_t level,
    SuspicionType suspicion,
    uint8_t controlGroup)
{
    const TrapDefinition* definition = getTrapDefinition(id);
    if (definition == nullptr || !isInsideRoom(x, y) ||
        getTrapAt(room, x, y) != nullptr)
    {
        return false;
    }

    for (TrapInstance& trap : room.traps)
    {
        if (trap.id != TRAP_NONE)
            continue;

        trap = TrapInstance{};
        trap.id = id;
        trap.x = static_cast<int8_t>(x);
        trap.y = static_cast<int8_t>(y);
        trap.level = clampTrapLevel(level);
        if (trap.level < definition->minimumLevel)
            trap.level = definition->minimumLevel;
        trap.suspicion = suspicion;
        trap.controlGroup = controlGroup;
        trap.hp = static_cast<int16_t>(getTrapMaxHP(trap));
        return true;
    }

    return false;
}

SuspicionType getSuspicionAt(const DungeonRoom& room, int x, int y)
{
    if (!isInsideRoom(x, y))
        return SUSPICION_NONE;

    for (const SuspicionInstance& suspicion : room.suspicions)
    {
        if (suspicion.type != SUSPICION_NONE &&
            suspicion.x == x && suspicion.y == y)
        {
            return suspicion.type;
        }
    }

    const TrapInstance* trap = getTrapAt(room, x, y);
    return trap != nullptr ? trap->suspicion : SUSPICION_NONE;
}

bool addSuspicion(
    DungeonRoom& room,
    SuspicionType type,
    int x,
    int y)
{
    if (type == SUSPICION_NONE || !isInsideRoom(x, y))
        return false;

    const SuspicionType existing = getSuspicionAt(room, x, y);
    if (existing != SUSPICION_NONE)
        return existing == type;

    for (SuspicionInstance& suspicion : room.suspicions)
    {
        if (suspicion.type != SUSPICION_NONE)
            continue;

        suspicion = SuspicionInstance{};
        suspicion.type = type;
        suspicion.x = static_cast<int8_t>(x);
        suspicion.y = static_cast<int8_t>(y);
        return true;
    }

    return false;
}

bool isTrapDiscovered(const DungeonRoom& room, int x, int y)
{
    const TrapInstance* trap = getTrapAt(room, x, y);
    return trap != nullptr && trap->discovered;
}

bool isTrapActive(const TrapInstance& trap)
{
    return getTrapDefinition(trap.id) != nullptr && trap.hp > 0 &&
        !trap.disabled && !trap.triggered && !trap.destroyed;
}

TrapDiscoveryResult attemptManualTrapDiscovery(
    TrapInstance& trap,
    int perceptionTotal)
{
    return attemptTrapDiscovery(
        trap, perceptionTotal, trap.manualPerceptionAttempted);
}

TrapDiscoveryResult attemptRogueTrapDiscovery(
    TrapInstance& trap,
    int perceptionTotal)
{
    return attemptTrapDiscovery(
        trap, perceptionTotal, trap.rogueDiscoveryAttempted);
}

const char* getTrapSearchFailureMessage(uint8_t index)
{
    static const char* const messages[] = {
        "You don't notice anything unusual.",
        "Nothing obvious stands out.",
        "You can't make anything of it.",
        "Your search is inconclusive.",
        "Whatever caught your attention remains unclear."
    };

    return messages[index % (sizeof(messages) / sizeof(messages[0]))];
}

TrapTriggerResult resolveTrapTrigger(
    TrapInstance& trap,
    bool saveSucceeded,
    int rolledDamage)
{
    TrapTriggerResult result;
    if (!isTrapActive(trap))
        return result;

    trap.triggered = true;
    trap.discovered = true;
    result.triggered = true;
    result.saveSucceeded = saveSucceeded;
    result.damage = saveSucceeded ? 0 : clampPositiveDamage(rolledDamage);
    return result;
}

TrapDamageResult damageTrap(
    TrapInstance& trap,
    int rawDamage,
    DamageType damageType)
{
    TrapDamageResult result;
    const TrapDefinition* definition = getTrapDefinition(trap.id);
    if (definition == nullptr || trap.destroyed || damageType == DAMAGE_NONE)
        return result;

    if (trap.hp <= 0)
    {
        trap.hp = 0;
        trap.destroyed = true;
        result.destroyed = true;
        return result;
    }

    result.incomingDamage = clampPositiveDamage(rawDamage);
    result.hardnessPrevented = result.incomingDamage < definition->hardness
        ? result.incomingDamage
        : definition->hardness;

    const uint16_t afterHardness =
        result.incomingDamage - result.hardnessPrevented;
    result.appliedDamage = afterHardness < static_cast<uint16_t>(trap.hp)
        ? afterHardness
        : static_cast<uint16_t>(trap.hp);

    trap.hp -= static_cast<int16_t>(result.appliedDamage);
    if (trap.hp <= 0)
    {
        trap.hp = 0;
        trap.destroyed = true;
        result.destroyed = true;
    }

    return result;
}

TrapDisableResult attemptDisableTrap(
    TrapInstance& trap,
    int disableTotal)
{
    if (getTrapDefinition(trap.id) == nullptr || !trap.discovered)
        return TRAP_DISABLE_NOT_ATTEMPTED;

    if (!isTrapActive(trap))
        return TRAP_DISABLE_ALREADY_INACTIVE;

    if (disableTotal < getTrapDisableDC(trap))
        return TRAP_DISABLE_FAILED;

    trap.disabled = true;
    return TRAP_DISABLE_SUCCESS;
}
