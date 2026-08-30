#include <Arduino.h>
#include <unity.h>

#include "../../src/dungeon/combatpolicy.h"
#include "../../src/map/skillactions.h"
#include "../../src/map/awareness.h"

bool hasCondition(const Character& character, ConditionType type)
{
    for (uint8_t i = 0; i < character.conditions.count; i++)
    {
        if (character.conditions.conditions[i].type == type)
            return true;
    }

    return false;
}

int getEffectiveSpeed(const Character& character)
{
    return character.speed;
}

static Entity makeEntity(
    EntityType type,
    Team team,
    CharacterState state = STATE_ALIVE)
{
    Entity entity = {};
    entity.active = true;
    entity.type = type;
    entity.character.team = team;
    entity.character.state = state;
    return entity;
}

static uint8_t makeRoster(
    Entity entities[],
    uint8_t entityCount,
    Entity* roster[])
{
    Entity* player = nullptr;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].type == ENTITY_PLAYER)
        {
            player = &entities[i];
            break;
        }
    }

    return buildCombatRoster(
        entities,
        entityCount,
        player,
        roster,
        MAX_ENTITIES);
}

void setUp()
{
}

void tearDown()
{
}

void test_detecting_monster_starts_a_full_active_map_roster()
{
    Entity entities[3] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    TEST_ASSERT_TRUE(applyMonsterDetectionResult(
        entities[1], monsterPerceptionBeatsStealth(16, 15)));
    TEST_ASSERT_TRUE(entities[1].awareOfPlayer);
    TEST_ASSERT_TRUE(entities[1].revealedToPlayer);
    Entity* roster[MAX_ENTITIES] = {};

    TEST_ASSERT_EQUAL_UINT8(2, makeRoster(entities, 3, roster));
}

void test_all_living_current_room_hostiles_join_dungeon_roster()
{
    Entity entities[4] = {
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER),
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    entities[0].awareOfPlayer = true;
    Entity* roster[MAX_ENTITIES] = {};

    TEST_ASSERT_EQUAL_UINT8(2, makeRoster(entities, 4, roster));
}

void test_detection_range_does_not_limit_dungeon_membership()
{
    Entity entities[2] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    entities[0].x = 1;
    entities[0].y = 1;
    entities[1].x = 14;
    entities[1].y = 14;
    Entity* roster[MAX_ENTITIES] = {};

    TEST_ASSERT_EQUAL_UINT8(1, makeRoster(entities, 2, roster));
}

void test_wall_hidden_combatant_is_not_visible()
{
    TEST_ASSERT_FALSE(shouldMonsterBeVisible(true, true, false));
}

void test_combatant_becomes_visible_with_line_of_sight()
{
    TEST_ASSERT_TRUE(shouldMonsterBeVisible(true, false, true));
}

void test_stale_discovery_cannot_hide_combatant_in_line_of_sight()
{
    TEST_ASSERT_TRUE(shouldMonsterBeVisible(true, false, true));
}

void test_provisional_ambush_does_not_reveal_other_unaware_monsters()
{
    TEST_ASSERT_FALSE(combatParticipationGrantsVisibility(
        true, true, false));
    TEST_ASSERT_TRUE(combatParticipationGrantsVisibility(
        true, true, true));
}

void test_dead_looted_and_inactive_monsters_do_not_join()
{
    Entity entities[5] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER, STATE_ALIVE),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER, STATE_DEAD),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER, STATE_LOOTED),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER, STATE_ALIVE)};
    entities[4].active = false;
    entities[1].awareOfPlayer = true;
    Entity* roster[MAX_ENTITIES] = {};

    TEST_ASSERT_EQUAL_UINT8(2, makeRoster(entities, 5, roster));
}

void test_entities_from_another_room_are_not_in_active_room_roster()
{
    Entity currentRoom[2] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    Entity otherRoom = makeEntity(ENTITY_MONSTER, TEAM_MONSTER);
    currentRoom[1].awareOfPlayer = true;
    Entity* roster[MAX_ENTITIES] = {};
    const uint8_t count = makeRoster(currentRoom, 2, roster);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_TRUE(roster[0] != &otherRoom && roster[1] != &otherRoom);
}

void test_unaware_monster_joins_without_gaining_player_knowledge()
{
    Entity entities[2] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    Entity* roster[MAX_ENTITIES] = {};

    TEST_ASSERT_EQUAL_UINT8(1, makeRoster(entities, 2, roster));
    TEST_ASSERT_FALSE(entities[1].awareOfPlayer);
    TEST_ASSERT_FALSE(shouldMonsterRunCombatAI(entities[1]));
}

void test_successful_in_combat_detection_enables_normal_ai()
{
    Entity monster = makeEntity(ENTITY_MONSTER, TEAM_MONSTER);
    TEST_ASSERT_TRUE(applyMonsterDetectionResult(
        monster, monsterPerceptionBeatsStealth(16, 15)));
    TEST_ASSERT_TRUE(shouldMonsterRunCombatAI(monster));
}

void test_losing_line_of_sight_does_not_erase_awareness()
{
    Entity monster = makeEntity(ENTITY_MONSTER, TEAM_MONSTER);
    monster.awareOfPlayer = true;

    TEST_ASSERT_FALSE(shouldMonsterBeVisible(true, true, false));
    TEST_ASSERT_TRUE(applyMonsterDetectionResult(monster, false));
    TEST_ASSERT_TRUE(monster.awareOfPlayer);
    TEST_ASSERT_TRUE(shouldMonsterRunCombatAI(monster));
}

void test_revealed_unaware_monster_can_remain_visible_before_combat()
{
    TEST_ASSERT_TRUE(shouldMonsterBeVisible(false, true, true));
}

void test_unobserved_ambush_kill_does_not_continue_combat()
{
    TEST_ASSERT_FALSE(shouldContinueCombatAfterOpeningAttack(
        true, true, false));
}

void test_second_monster_detection_establishes_combat_after_ambush_kill()
{
    TEST_ASSERT_TRUE(shouldContinueCombatAfterOpeningAttack(
        true, true, true));
}

void test_flat_footed_condition_preserves_existing_sneak_attack_rule()
{
    Entity rogue = makeEntity(ENTITY_PLAYER, TEAM_PLAYER);
    rogue.character.characterClass = CLASS_ROGUE;
    Entity monster = makeEntity(ENTITY_MONSTER, TEAM_MONSTER);

    TEST_ASSERT_FALSE(qualifiesForSneakAttack(rogue, monster));
    monster.character.conditions.count = 1;
    monster.character.conditions.conditions[0].type =
        CONDITION_FLAT_FOOTED;
    TEST_ASSERT_TRUE(qualifiesForSneakAttack(rogue, monster));
}

void test_combat_start_enemy_count_matches_living_roster()
{
    Entity entities[4] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER, STATE_DEAD)};
    Entity* roster[MAX_ENTITIES] = {};
    entities[1].awareOfPlayer = true;
    entities[2].awareOfPlayer = true;
    const uint8_t count = makeRoster(entities, 4, roster);

    TEST_ASSERT_EQUAL_UINT8(3, count);
    TEST_ASSERT_EQUAL_UINT8(
        2, countLivingHostilesInCombatRoster(roster, count));
}

void test_reinforcement_waits_until_following_round()
{
    Entity monster = makeEntity(ENTITY_MONSTER, TEAM_MONSTER);
    monster.reinforcementJoinedRound = 2;
    TEST_ASSERT_FALSE(reinforcementMayAct(monster, 2));
    TEST_ASSERT_TRUE(reinforcementMayAct(monster, 3));
    monster.reinforcementJoinedRound = 0;
    TEST_ASSERT_TRUE(reinforcementMayAct(monster, 1));
}

void test_neutral_and_unaware_hostiles_are_not_initial_combatants()
{
    Entity entities[3] = {
        makeEntity(ENTITY_PLAYER, TEAM_PLAYER),
        makeEntity(ENTITY_MONSTER, TEAM_NEUTRAL),
        makeEntity(ENTITY_MONSTER, TEAM_MONSTER)};
    entities[1].awareOfPlayer = true;
    Entity* roster[MAX_ENTITIES] = {};
    TEST_ASSERT_EQUAL_UINT8(1, makeRoster(entities, 3, roster));
}

void test_pathfinder_iterative_attack_progression()
{
    TEST_ASSERT_EQUAL_UINT8(1, getIterativeAttackCount(5));
    TEST_ASSERT_EQUAL_INT(5, getIterativeBAB(5, 0));

    TEST_ASSERT_EQUAL_UINT8(2, getIterativeAttackCount(6));
    TEST_ASSERT_EQUAL_INT(6, getIterativeBAB(6, 0));
    TEST_ASSERT_EQUAL_INT(1, getIterativeBAB(6, 1));

    TEST_ASSERT_EQUAL_UINT8(2, getIterativeAttackCount(10));
    TEST_ASSERT_EQUAL_INT(5, getIterativeBAB(10, 1));
    TEST_ASSERT_EQUAL_UINT8(3, getIterativeAttackCount(11));
    TEST_ASSERT_EQUAL_INT(1, getIterativeBAB(11, 2));
    TEST_ASSERT_EQUAL_UINT8(3, getIterativeAttackCount(15));
    TEST_ASSERT_EQUAL_INT(5, getIterativeBAB(15, 2));
    TEST_ASSERT_EQUAL_UINT8(4, getIterativeAttackCount(16));
    TEST_ASSERT_EQUAL_INT(1, getIterativeBAB(16, 3));
    TEST_ASSERT_EQUAL_UINT8(4, getIterativeAttackCount(20));
    TEST_ASSERT_EQUAL_INT(5, getIterativeBAB(20, 3));
}

void test_full_attack_requires_unused_normal_movement()
{
    Entity attacker = makeEntity(ENTITY_PLAYER, TEAM_PLAYER);
    attacker.character.speed = 6;
    attacker.turn.movementRemaining = 6;
    TEST_ASSERT_TRUE(canMakeFullAttack(attacker));

    attacker.turn.movementRemaining = 5;
    TEST_ASSERT_FALSE(canMakeFullAttack(attacker));

    attacker.turn.movementRemaining = 6;
    attacker.turn.moveActionUsed = true;
    TEST_ASSERT_FALSE(canMakeFullAttack(attacker));

    attacker.turn.moveActionUsed = false;
    attacker.turn.fiveFootStepUsed = true;
    TEST_ASSERT_TRUE(canMakeFullAttack(attacker));
}

void test_natural_weapons_do_not_gain_iteratives()
{
    TEST_ASSERT_TRUE(isNaturalWeaponItem(ITEM_BITE));
    TEST_ASSERT_TRUE(isNaturalWeaponItem(ITEM_CLAWS));
    TEST_ASSERT_TRUE(isNaturalWeaponItem(ITEM_SLAM));
    TEST_ASSERT_FALSE(isNaturalWeaponItem(ITEM_LONGSWORD));
    TEST_ASSERT_FALSE(isNaturalWeaponItem(ITEM_LONGBOW));
}

void test_iterative_sequence_stops_at_end_or_when_target_dies()
{
    TEST_ASSERT_TRUE(shouldContinueIterativeAttack(0, 3, true, true));
    TEST_ASSERT_TRUE(shouldContinueIterativeAttack(1, 3, true, true));
    TEST_ASSERT_FALSE(shouldContinueIterativeAttack(2, 3, true, true));
    TEST_ASSERT_FALSE(shouldContinueIterativeAttack(0, 3, true, false));
    TEST_ASSERT_FALSE(shouldContinueIterativeAttack(0, 3, false, true));
}

void test_skill_action_policy()
{
    TEST_ASSERT_EQUAL_INT(17, calculateIntimidateDC(5, 2));
    TEST_ASSERT_TRUE(intimidateSucceeds(17, 17));
    TEST_ASSERT_FALSE(intimidateSucceeds(16, 17));
    TEST_ASSERT_EQUAL(SOCIAL_FAVORABLE, resolveSocialCheckTotal(15, 15));
    TEST_ASSERT_EQUAL(SOCIAL_NEUTRAL, resolveSocialCheckTotal(14, 15));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_detecting_monster_starts_a_full_active_map_roster);
    RUN_TEST(test_all_living_current_room_hostiles_join_dungeon_roster);
    RUN_TEST(test_detection_range_does_not_limit_dungeon_membership);
    RUN_TEST(test_wall_hidden_combatant_is_not_visible);
    RUN_TEST(test_combatant_becomes_visible_with_line_of_sight);
    RUN_TEST(test_stale_discovery_cannot_hide_combatant_in_line_of_sight);
    RUN_TEST(test_provisional_ambush_does_not_reveal_other_unaware_monsters);
    RUN_TEST(test_dead_looted_and_inactive_monsters_do_not_join);
    RUN_TEST(test_entities_from_another_room_are_not_in_active_room_roster);
    RUN_TEST(test_unaware_monster_joins_without_gaining_player_knowledge);
    RUN_TEST(test_successful_in_combat_detection_enables_normal_ai);
    RUN_TEST(test_losing_line_of_sight_does_not_erase_awareness);
    RUN_TEST(test_revealed_unaware_monster_can_remain_visible_before_combat);
    RUN_TEST(test_unobserved_ambush_kill_does_not_continue_combat);
    RUN_TEST(test_second_monster_detection_establishes_combat_after_ambush_kill);
    RUN_TEST(test_flat_footed_condition_preserves_existing_sneak_attack_rule);
    RUN_TEST(test_combat_start_enemy_count_matches_living_roster);
    RUN_TEST(test_reinforcement_waits_until_following_round);
    RUN_TEST(test_neutral_and_unaware_hostiles_are_not_initial_combatants);
    RUN_TEST(test_pathfinder_iterative_attack_progression);
    RUN_TEST(test_full_attack_requires_unused_normal_movement);
    RUN_TEST(test_natural_weapons_do_not_gain_iteratives);
    RUN_TEST(test_iterative_sequence_stops_at_end_or_when_target_dies);
    RUN_TEST(test_skill_action_policy);
    UNITY_END();
}

void loop()
{
}
