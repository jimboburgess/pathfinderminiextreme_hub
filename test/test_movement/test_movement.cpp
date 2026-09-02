#include <Arduino.h>
#include <unity.h>

#include "../../src/map/movement.h"

void test_base_terrain_movement_costs()
{
    TEST_ASSERT_EQUAL_UINT8(1, getTerrainMovementCost(TILE_FLOOR));
    TEST_ASSERT_EQUAL_UINT8(2, getTerrainMovementCost(TILE_RUBBLE));
    TEST_ASSERT_TRUE(isDungeonFloorTerrain(TILE_RUBBLE));
}

void test_rubble_acrobatics_resolves_one_attempt_cost()
{
    TEST_ASSERT_EQUAL_UINT8(
        1, resolveTerrainMovementCost(TILE_RUBBLE, true));
    TEST_ASSERT_EQUAL_UINT8(
        2, resolveTerrainMovementCost(TILE_RUBBLE, false));
    TEST_ASSERT_EQUAL_UINT8(
        1, resolveTerrainMovementCost(TILE_FLOOR, false));
}

void test_resolved_cost_controls_affordability_without_underflow()
{
    const uint8_t failedRubbleCost =
        resolveTerrainMovementCost(TILE_RUBBLE, false);
    const uint8_t successfulRubbleCost =
        resolveTerrainMovementCost(TILE_RUBBLE, true);

    // Failure leaves movement legal, but a combatant with one point cannot
    // pay the resulting two-point cost. Success permits the same attempt.
    TEST_ASSERT_TRUE(failedRubbleCost > 0);
    TEST_ASSERT_FALSE(canPayMovementCost(1, failedRubbleCost));
    TEST_ASSERT_TRUE(canPayMovementCost(1, successfulRubbleCost));
    TEST_ASSERT_FALSE(canPayMovementCost(0, successfulRubbleCost));
    TEST_ASSERT_FALSE(canPayMovementCost(255, 0));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_base_terrain_movement_costs);
    RUN_TEST(test_rubble_acrobatics_resolves_one_attempt_cost);
    RUN_TEST(test_resolved_cost_controls_affordability_without_underflow);
    UNITY_END();
}

void loop()
{
}
