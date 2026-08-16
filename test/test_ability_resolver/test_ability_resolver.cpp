#include <Arduino.h>
#include <unity.h>

#include "../../src/data/entities.h"
#include "../../src/dungeon/abilityresolver.h"
#include "../../src/dungeon/combat.h"

static int controlledDistance = 1;
static bool controlledLineOfSight = true;
static bool controlledCanAct = true;
static uint8_t damageApplicationCount = 0;

bool canCharacterAct(const Character&)
{
    return controlledCanAct;
}

int healCharacter(Character& character, int healing)
{
    if (healing <= 0 ||
        character.health.currentHP >= character.health.maxHP)
    {
        return 0;
    }

    int missing = character.health.maxHP - character.health.currentHP;
    int restored = healing < missing ? healing : missing;
    character.health.currentHP += restored;
    return restored;
}

int getEntityGridDistance(const Entity&, const Entity&)
{
    return controlledDistance;
}

bool hasLineOfSightBetweenFootprintsAt(
    const Entity&,
    int,
    int,
    const Entity&)
{
    return controlledLineOfSight;
}

CombatDamageResult applyCombatDamage(Entity& target, int damage)
{
    CombatDamageResult result;

    if (damage <= 0 || !target.active ||
        target.character.state != STATE_ALIVE)
    {
        return result;
    }

    damageApplicationCount++;
    target.character.health.currentHP -= damage;
    result.applied = true;

    if (target.character.health.currentHP <= 0)
    {
        target.character.health.currentHP = 0;
        target.character.state = STATE_DEAD;
        result.defeated = true;
    }

    return result;
}

int getMaxHP(const Character& character)
{
    return character.health.maxHP;
}

#include "../../src/characters/abilities.cpp"
#include "../../src/data/progression.cpp"
#include "../../src/dungeon/abilityresolver.cpp"

static void initializeEntity(
    Entity& entity,
    EntityType type,
    Team team,
    int hitPoints = 20)
{
    entity = Entity{};
    entity.active = true;
    entity.type = type;
    entity.character.team = team;
    entity.character.state = STATE_ALIVE;
    entity.character.level = 1;
    entity.character.health.currentHP = hitPoints;
    entity.character.health.maxHP = hitPoints;
    entity.character.magic.currentMP = 10;
    entity.character.magic.maxMP = 10;
}

static void resetResolverControls()
{
    controlledDistance = 1;
    controlledLineOfSight = true;
    controlledCanAct = true;
    damageApplicationCount = 0;
}

void test_wizard_magic_initialization_learns_magic_missile()
{
    Character wizard = {};
    wizard.characterClass = CLASS_WIZARD;
    wizard.level = 1;

    initializeCharacterMagic(wizard);

    TEST_ASSERT_TRUE(wizard.magic.arcaneCaster);
    TEST_ASSERT_FALSE(wizard.magic.divineCaster);
    TEST_ASSERT_EQUAL_INT(6, wizard.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(6, wizard.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL(
        ABILITY_MAGIC_MISSILE, wizard.magic.knownAbilities[0]);

    const Ability* magicMissile = getAbility(ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_NOT_NULL(magicMissile);
    TEST_ASSERT_EQUAL(TARGET_ENEMY, magicMissile->target);
    TEST_ASSERT_EQUAL(DELIVERY_TARGET, magicMissile->delivery);
    TEST_ASSERT_EQUAL_UINT8(6, magicMissile->rangeTiles);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_MAGIC_MISSILE));
}

void test_magic_missile_success_uses_cost_action_and_damage()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL_INT(5, result.damage);
    TEST_ASSERT_EQUAL_INT(15, monster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, damageApplicationCount);
}

void test_wizard_can_know_spells_the_resolver_still_hides()
{
    Character wizard = {};
    wizard.characterClass = CLASS_WIZARD;
    wizard.level = 7;
    initializeCharacterMagic(wizard);

    uint8_t supportedCount = 0;

    for (uint8_t i = 0; i < wizard.magic.knownAbilityCount; i++)
    {
        if (isAbilitySupported(wizard.magic.knownAbilities[i]))
            supportedCount++;
    }

    TEST_ASSERT_EQUAL_UINT8(13, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL_UINT8(1, supportedCount);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_FALSE(isAbilitySupported(ABILITY_RAY_OF_FROST));
    TEST_ASSERT_FALSE(isAbilitySupported(ABILITY_FIREBALL));
    TEST_ASSERT_FALSE(isAbilitySupported(ABILITY_ICE_STORM));
}

void test_failed_casts_do_not_mutate_state()
{
    Entity wizard;
    Entity monster;

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    wizard.character.magic.currentMP = 1;

    AbilityResolution noMP = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_NOT_ENOUGH_MP, noMP.result);
    TEST_ASSERT_EQUAL_INT(20, monster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(1, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(0, damageApplicationCount);

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    controlledDistance = 7;

    AbilityResolution outOfRange = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_OUT_OF_RANGE, outOfRange.result);
    TEST_ASSERT_EQUAL_INT(20, monster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    controlledLineOfSight = false;

    AbilityResolution noLOS = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_NO_LINE_OF_SIGHT, noLOS.result);
    TEST_ASSERT_EQUAL_INT(20, monster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
}

void test_invalid_friendly_dead_and_used_action_targets_are_rejected()
{
    resetResolverControls();
    Entity wizard;
    Entity target;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(target, ENTITY_PLAYER, TEAM_PLAYER);

    AbilityResolution friendly = resolveAbility(
        wizard, &target, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_INVALID_TARGET, friendly.result);
    TEST_ASSERT_EQUAL_INT(20, target.character.health.currentHP);

    target.character.team = TEAM_MONSTER;
    target.character.state = STATE_DEAD;
    AbilityResolution dead = resolveAbility(
        wizard, &target, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_INVALID_TARGET, dead.result);

    target.character.state = STATE_ALIVE;
    wizard.turn.standardActionUsed = true;
    AbilityResolution usedAction = resolveAbility(
        wizard, &target, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(
        ABILITY_RESULT_NO_STANDARD_ACTION, usedAction.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_EQUAL_INT(20, target.character.health.currentHP);
    TEST_ASSERT_EQUAL_UINT8(0, damageApplicationCount);
}

void test_duration_effect_is_rejected_without_state_changes()
{
    resetResolverControls();
    Entity caster;
    initializeEntity(caster, ENTITY_MONSTER, TEAM_MONSTER, 10);
    caster.character.health.currentHP = 9;

    AbilityResolution result = resolveAbility(
        caster, nullptr, ABILITY_REGENERATION);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_UNSUPPORTED, result.result);
    TEST_ASSERT_EQUAL_INT(0, result.healing);
    TEST_ASSERT_EQUAL_INT(9, caster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(10, caster.character.magic.currentMP);
    TEST_ASSERT_FALSE(caster.turn.standardActionUsed);
}

void test_monster_magic_missile_uses_the_same_resolver()
{
    resetResolverControls();
    Entity spectator;
    Entity player;
    initializeEntity(spectator, ENTITY_MONSTER, TEAM_MONSTER);
    initializeEntity(player, ENTITY_PLAYER, TEAM_PLAYER);
    spectator.character.level = 4;
    spectator.character.magic.currentMP = 2;
    spectator.character.magic.maxMP = 2;

    AbilityResolution result = resolveAbility(
        spectator, &player, ABILITY_MAGIC_MISSILE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL_INT(8, result.damage);
    TEST_ASSERT_EQUAL_INT(12, player.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(0, spectator.character.magic.currentMP);
    TEST_ASSERT_TRUE(spectator.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, damageApplicationCount);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_wizard_magic_initialization_learns_magic_missile);
    RUN_TEST(test_magic_missile_success_uses_cost_action_and_damage);
    RUN_TEST(test_wizard_can_know_spells_the_resolver_still_hides);
    RUN_TEST(test_failed_casts_do_not_mutate_state);
    RUN_TEST(test_invalid_friendly_dead_and_used_action_targets_are_rejected);
    RUN_TEST(test_duration_effect_is_rejected_without_state_changes);
    RUN_TEST(test_monster_magic_missile_uses_the_same_resolver);
    UNITY_END();
}

void loop()
{
}
