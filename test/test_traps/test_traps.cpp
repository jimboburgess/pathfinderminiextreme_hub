#include <Arduino.h>
#include <unity.h>

#include "../../src/dungeon/traps.cpp"

namespace
{
TrapInstance makeSpikeTrap(uint8_t level = 1)
{
    TrapInstance trap;
    trap.id = TRAP_SPIKE_PLATE;
    trap.x = 4;
    trap.y = 5;
    trap.level = level;
    trap.hp = static_cast<int16_t>(getTrapMaxHP(trap));
    trap.suspicion = SUSPICION_RAISED_TILE;
    return trap;
}
}

void setUp()
{
}

void tearDown()
{
}

void test_spike_plate_definition_uses_shared_damage_and_save_types()
{
    const TrapDefinition* definition =
        getTrapDefinition(TRAP_SPIKE_PLATE);

    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL(TRAP_SPIKE_PLATE, definition->id);
    TEST_ASSERT_EQUAL_STRING("Pressure Plate", definition->name);
    TEST_ASSERT_EQUAL(DAMAGE_PIERCING, definition->damageType);
    TEST_ASSERT_EQUAL(SAVE_REFLEX, definition->saveType);
    TEST_ASSERT_TRUE(definition->baseHP > 0);
    TEST_ASSERT_TRUE(definition->hardness > 0);
    TEST_ASSERT_NULL(getTrapDefinition(TRAP_NONE));
}

void test_projectile_trap_definitions_and_launcher_state_are_data_driven()
{
    const TrapDefinition* arrow = getTrapDefinition(TRAP_ARROW);
    const TrapDefinition* dart = getTrapDefinition(TRAP_POISON_DART);
    TEST_ASSERT_NOT_NULL(arrow);
    TEST_ASSERT_NOT_NULL(dart);
    TEST_ASSERT_EQUAL(DAMAGE_PIERCING, arrow->damageType);
    TEST_ASSERT_EQUAL(SAVE_REFLEX, arrow->saveType);
    TEST_ASSERT_EQUAL_UINT8(1, dart->damageDice);
    TEST_ASSERT_EQUAL_UINT8(1, dart->damageSides);

    TrapInstance trap{};
    trap.id = TRAP_ARROW;
    TEST_ASSERT_TRUE(configureProjectileTrap(trap, 1, 4, DIR_EAST));
    TEST_ASSERT_TRUE(isProjectileTrap(trap));
    TEST_ASSERT_EQUAL_INT(1, trap.sourceX);
    TEST_ASSERT_EQUAL_INT(4, trap.sourceY);
    TEST_ASSERT_EQUAL(DIR_EAST, trap.direction);
}

void test_trap_level_selection_varies_and_stays_in_intended_ranges()
{
    bool differsFromChallenge = false;

    for (uint8_t percentile = 0; percentile < 100; percentile++)
    {
        const uint8_t level = selectTrapLevel(10, percentile);
        TEST_ASSERT_TRUE(level >= 8 && level <= 13);

        if (percentile < 90)
            TEST_ASSERT_TRUE(level >= 8 && level <= 11);
        else
            TEST_ASSERT_TRUE(level == 12 || level == 13);

        differsFromChallenge = differsFromChallenge || level != 10;
    }

    TEST_ASSERT_TRUE(differsFromChallenge);
    TEST_ASSERT_EQUAL_UINT8(1, selectTrapLevel(1, 0));
    TEST_ASSERT_EQUAL_UINT8(20, selectTrapLevel(20, 99));
}

void test_trap_scaling_is_linear_and_bounded()
{
    const TrapInstance low = makeSpikeTrap(1);
    const TrapInstance high = makeSpikeTrap(20);

    TEST_ASSERT_TRUE(getTrapPerceptionDC(high) > getTrapPerceptionDC(low));
    TEST_ASSERT_TRUE(getTrapDisableDC(high) > getTrapDisableDC(low));
    TEST_ASSERT_TRUE(getTrapMaxHP(high) > getTrapMaxHP(low));
    TEST_ASSERT_TRUE(getTrapSaveDC(high) > getTrapSaveDC(low));
    TEST_ASSERT_TRUE(getTrapDamageDice(high) > getTrapDamageDice(low));
    TEST_ASSERT_EQUAL_UINT8(
        getTrapDamageSides(low), getTrapDamageSides(high));

    // Twenty levels remain suitable for small integer state and do not use
    // exponential progression.
    TEST_ASSERT_TRUE(getTrapMaxHP(high) < 64);
    TEST_ASSERT_TRUE(getTrapDamageDice(high) <= 5);
}

void test_room_helpers_keep_clues_independent_from_traps()
{
    DungeonRoom room{};

    TEST_ASSERT_TRUE(addSuspicion(
        room, SUSPICION_BONES, 2, 3));
    TEST_ASSERT_EQUAL(
        SUSPICION_BONES, getSuspicionAt(room, 2, 3));
    TEST_ASSERT_NULL(getTrapAt(room, 2, 3));

    TEST_ASSERT_TRUE(addTrap(
        room,
        TRAP_SPIKE_PLATE,
        6,
        7,
        4,
        SUSPICION_FLOOR_GROOVES,
        9));

    TrapInstance* trap = getTrapAt(room, 6, 7);
    TEST_ASSERT_NOT_NULL(trap);
    TEST_ASSERT_EQUAL_UINT8(4, trap->level);
    TEST_ASSERT_EQUAL_UINT8(9, trap->controlGroup);
    TEST_ASSERT_EQUAL_INT(getTrapMaxHP(*trap), trap->hp);
    TEST_ASSERT_EQUAL(
        SUSPICION_FLOOR_GROOVES, getSuspicionAt(room, 6, 7));
    TEST_ASSERT_FALSE(trap->discovered);
}

void test_successful_perception_reveals_and_failure_does_not()
{
    TrapInstance success = makeSpikeTrap(3);
    TrapInstance failure = makeSpikeTrap(3);
    const int dc = getTrapPerceptionDC(success);

    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_SUCCESS,
        attemptManualTrapDiscovery(success, dc));
    TEST_ASSERT_TRUE(success.discovered);
    TEST_ASSERT_TRUE(success.manualPerceptionAttempted);

    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_FAILED,
        attemptManualTrapDiscovery(failure, dc - 1));
    TEST_ASSERT_FALSE(failure.discovered);
    TEST_ASSERT_TRUE(failure.manualPerceptionAttempted);
}

void test_manual_and_rogue_discovery_attempts_are_each_one_shot()
{
    TrapInstance trap = makeSpikeTrap(5);
    const int dc = getTrapPerceptionDC(trap);

    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_FAILED,
        attemptRogueTrapDiscovery(trap, dc - 1));
    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_NOT_ATTEMPTED,
        attemptRogueTrapDiscovery(trap, dc + 20));
    TEST_ASSERT_FALSE(trap.discovered);

    // The explicit player action remains a distinct opportunity.
    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_SUCCESS,
        attemptManualTrapDiscovery(trap, dc));
    TEST_ASSERT_TRUE(trap.discovered);
    TEST_ASSERT_EQUAL(
        TRAP_DISCOVERY_ALREADY_KNOWN,
        attemptManualTrapDiscovery(trap, dc));
}

void test_failure_messages_are_ambiguous_and_reusable()
{
    TEST_ASSERT_EQUAL_STRING(
        "You don't notice anything unusual.",
        getTrapSearchFailureMessage(0));
    TEST_ASSERT_EQUAL_STRING(
        "Nothing obvious stands out.",
        getTrapSearchFailureMessage(1));
    TEST_ASSERT_EQUAL_STRING(
        getTrapSearchFailureMessage(0),
        getTrapSearchFailureMessage(5));
}

void test_active_spike_trap_triggers_once_and_failed_save_deals_damage()
{
    TrapInstance trap = makeSpikeTrap(2);

    TrapTriggerResult first = resolveTrapTrigger(trap, false, 7);
    TEST_ASSERT_TRUE(first.triggered);
    TEST_ASSERT_FALSE(first.saveSucceeded);
    TEST_ASSERT_EQUAL_UINT16(7, first.damage);
    TEST_ASSERT_TRUE(trap.triggered);
    TEST_ASSERT_TRUE(trap.discovered);
    TEST_ASSERT_FALSE(trap.destroyed);
    TEST_ASSERT_FALSE(isTrapActive(trap));

    TrapTriggerResult second = resolveTrapTrigger(trap, false, 99);
    TEST_ASSERT_FALSE(second.triggered);
    TEST_ASSERT_EQUAL_UINT16(0, second.damage);
}

void test_successful_reflex_save_negates_spike_damage()
{
    TrapInstance trap = makeSpikeTrap(2);
    TrapTriggerResult result = resolveTrapTrigger(trap, true, 12);

    TEST_ASSERT_TRUE(result.triggered);
    TEST_ASSERT_TRUE(result.saveSucceeded);
    TEST_ASSERT_EQUAL_UINT16(0, result.damage);
    TEST_ASSERT_TRUE(trap.triggered);
}

void test_disabled_and_destroyed_traps_do_not_trigger()
{
    TrapInstance disabled = makeSpikeTrap();
    disabled.disabled = true;
    TrapInstance destroyed = makeSpikeTrap();
    destroyed.destroyed = true;
    destroyed.hp = 0;

    TEST_ASSERT_FALSE(resolveTrapTrigger(disabled, false, 8).triggered);
    TEST_ASSERT_FALSE(resolveTrapTrigger(destroyed, false, 8).triggered);
}

void test_trap_visual_state_uses_persistent_state_priority()
{
    TrapInstance trap = makeSpikeTrap();
    TEST_ASSERT_EQUAL(TRAP_VISUAL_HIDDEN, getTrapVisualState(trap));

    trap.discovered = true;
    TEST_ASSERT_EQUAL(TRAP_VISUAL_ARMED, getTrapVisualState(trap));

    trap.triggered = true;
    TEST_ASSERT_EQUAL(TRAP_VISUAL_TRIGGERED, getTrapVisualState(trap));

    trap.disabled = true;
    TEST_ASSERT_EQUAL(TRAP_VISUAL_DISABLED, getTrapVisualState(trap));

    trap.destroyed = true;
    TEST_ASSERT_EQUAL(TRAP_VISUAL_DESTROYED, getTrapVisualState(trap));
}

void test_hardness_never_creates_negative_damage()
{
    TrapInstance trap = makeSpikeTrap();
    const int startingHP = trap.hp;

    TrapDamageResult result = damageTrap(trap, 1, DAMAGE_SLASHING);
    TEST_ASSERT_EQUAL_UINT16(1, result.incomingDamage);
    TEST_ASSERT_EQUAL_UINT16(1, result.hardnessPrevented);
    TEST_ASSERT_EQUAL_UINT16(0, result.appliedDamage);
    TEST_ASSERT_EQUAL_INT(startingHP, trap.hp);
    TEST_ASSERT_FALSE(result.destroyed);

    result = damageTrap(trap, -20, DAMAGE_PIERCING);
    TEST_ASSERT_EQUAL_UINT16(0, result.incomingDamage);
    TEST_ASSERT_EQUAL_UINT16(0, result.appliedDamage);
    TEST_ASSERT_EQUAL_INT(startingHP, trap.hp);
}

void test_trap_damage_clamps_hp_to_zero_and_only_destroys()
{
    TrapInstance trap = makeSpikeTrap();
    trap.discovered = true;

    TrapDamageResult result = damageTrap(trap, 1000, DAMAGE_BLUDGEONING);
    TEST_ASSERT_TRUE(result.destroyed);
    TEST_ASSERT_EQUAL_INT(0, trap.hp);
    TEST_ASSERT_TRUE(trap.destroyed);
    TEST_ASSERT_TRUE(trap.discovered);
    TEST_ASSERT_FALSE(trap.disabled);
    TEST_ASSERT_FALSE(trap.triggered);

    result = damageTrap(trap, 1000, DAMAGE_BLUDGEONING);
    TEST_ASSERT_FALSE(result.destroyed);
    TEST_ASSERT_EQUAL_UINT16(0, result.appliedDamage);
    TEST_ASSERT_EQUAL_INT(0, trap.hp);
}

void test_disable_requires_discovery_and_persists_on_success()
{
    TrapInstance trap = makeSpikeTrap(4);
    const int dc = getTrapDisableDC(trap);

    TEST_ASSERT_EQUAL(
        TRAP_DISABLE_NOT_ATTEMPTED,
        attemptDisableTrap(trap, dc));
    TEST_ASSERT_FALSE(trap.disabled);

    trap.discovered = true;
    TEST_ASSERT_EQUAL(
        TRAP_DISABLE_FAILED,
        attemptDisableTrap(trap, dc - 1));
    TEST_ASSERT_FALSE(trap.disabled);
    TEST_ASSERT_EQUAL(
        TRAP_DISABLE_SUCCESS,
        attemptDisableTrap(trap, dc));
    TEST_ASSERT_TRUE(trap.disabled);
    TEST_ASSERT_EQUAL(
        TRAP_DISABLE_ALREADY_INACTIVE,
        attemptDisableTrap(trap, dc + 100));
}

void test_room_runtime_representation_preserves_all_trap_flags()
{
    DungeonRoom room{};
    TEST_ASSERT_TRUE(addTrap(
        room,
        TRAP_SPIKE_PLATE,
        8,
        9,
        6,
        SUSPICION_BLOODSTAIN));

    TrapInstance* trap = getTrapAt(room, 8, 9);
    TEST_ASSERT_NOT_NULL(trap);
    trap->discovered = true;
    trap->disabled = true;
    trap->triggered = true;
    trap->destroyed = true;
    trap->rogueDiscoveryAttempted = true;
    trap->manualPerceptionAttempted = true;
    trap->hp = 0;

    // A room transition rebinds the room but does not reconstruct this array.
    TrapInstance* sameTrap = getTrapAt(room, 8, 9);
    TEST_ASSERT_EQUAL_PTR(trap, sameTrap);
    TEST_ASSERT_TRUE(sameTrap->discovered);
    TEST_ASSERT_TRUE(sameTrap->disabled);
    TEST_ASSERT_TRUE(sameTrap->triggered);
    TEST_ASSERT_TRUE(sameTrap->destroyed);
    TEST_ASSERT_TRUE(sameTrap->rogueDiscoveryAttempted);
    TEST_ASSERT_TRUE(sameTrap->manualPerceptionAttempted);
    TEST_ASSERT_EQUAL_INT(0, sameTrap->hp);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_spike_plate_definition_uses_shared_damage_and_save_types);
    RUN_TEST(test_projectile_trap_definitions_and_launcher_state_are_data_driven);
    RUN_TEST(test_trap_level_selection_varies_and_stays_in_intended_ranges);
    RUN_TEST(test_trap_scaling_is_linear_and_bounded);
    RUN_TEST(test_room_helpers_keep_clues_independent_from_traps);
    RUN_TEST(test_successful_perception_reveals_and_failure_does_not);
    RUN_TEST(test_manual_and_rogue_discovery_attempts_are_each_one_shot);
    RUN_TEST(test_failure_messages_are_ambiguous_and_reusable);
    RUN_TEST(test_active_spike_trap_triggers_once_and_failed_save_deals_damage);
    RUN_TEST(test_successful_reflex_save_negates_spike_damage);
    RUN_TEST(test_disabled_and_destroyed_traps_do_not_trigger);
    RUN_TEST(test_trap_visual_state_uses_persistent_state_priority);
    RUN_TEST(test_hardness_never_creates_negative_damage);
    RUN_TEST(test_trap_damage_clamps_hp_to_zero_and_only_destroys);
    RUN_TEST(test_disable_requires_discovery_and_persists_on_success);
    RUN_TEST(test_room_runtime_representation_preserves_all_trap_flags);
    UNITY_END();
}

void loop()
{
}
