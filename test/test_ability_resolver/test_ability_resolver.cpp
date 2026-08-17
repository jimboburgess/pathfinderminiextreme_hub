#include <Arduino.h>
#include <unity.h>

#include "../../src/data/entities.h"
#include "../../src/dungeon/abilityresolver.h"
#include "../../src/dungeon/combat.h"
#include "../../src/dungeon/mapeffects.h"
#include "../../src/dungeon/movement.h"
#include "../../src/data/game.h"
#include "../../src/graphics/display.h"

static int controlledDistance = 1;
static bool controlledLineOfSight = true;
static int controlledSaveRoll = 10;
static uint8_t damageApplicationCount = 0;
static uint8_t saveRollCount = 0;
static uint8_t dirtyTileMarkCount = 0;
static bool controlledBaseTerrainDifficult = false;
static Entity activeTestEntities[4];
static uint8_t activeTestEntityCount = 0;

DirtyTile dirtyTiles[MAX_DIRTY_TILES];
uint8_t dirtyTileCount = 0;
bool backgroundNeedsRedraw = false;
RedrawType redrawType = REDRAW_NONE;
bool needsRedraw = false;

int getAbilityModifier(int score)
{
    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

int getAbilityModifier(const Character& character, AbilityScore ability)
{
    int score = 10;

    switch (ability)
    {
        case ABILITY_STRENGTH:
            score = character.abilities.strength;
            break;
        case ABILITY_DEXTERITY:
            score = character.abilities.dexterity;
            break;
        case ABILITY_CONSTITUTION:
            score = character.abilities.constitution;
            break;
        case ABILITY_INTELLIGENCE:
            score = character.abilities.intelligence;
            break;
        case ABILITY_WISDOM:
            score = character.abilities.wisdom;
            break;
        case ABILITY_CHARISMA:
            score = character.abilities.charisma;
            break;
    }

    return getAbilityModifier(score);
}

bool isConscious(const Character& character)
{
    return character.state == STATE_ALIVE;
}

int rollDie(int sides)
{
    if (sides == 20)
    {
        saveRollCount++;
        return controlledSaveRoll;
    }

    return 1;
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

int getEntityGridDistanceToTile(const Entity&, int, int)
{
    return controlledDistance;
}

bool hasLineOfSightFromFootprintAt(
    const Entity&,
    int,
    int,
    int,
    int)
{
    return controlledLineOfSight;
}

int getActiveMapWidth()
{
    return 10;
}

int getActiveMapHeight()
{
    return 10;
}

bool isInsideActiveMap(int x, int y)
{
    return x >= 0 && x < getActiveMapWidth() &&
           y >= 0 && y < getActiveMapHeight();
}

bool isBaseTerrainDifficultAt(int x, int y)
{
    return isInsideActiveMap(x, y) &&
           controlledBaseTerrainDifficult;
}

Entity* getActiveMapEntities(uint8_t& entityCount)
{
    entityCount = activeTestEntityCount;
    return activeTestEntityCount > 0 ? activeTestEntities : nullptr;
}

uint8_t getEntityTileWidth(const Entity&)
{
    return 1;
}

uint8_t getEntityTileHeight(const Entity&)
{
    return 1;
}

void markTileDirty(int, int)
{
    dirtyTileMarkCount++;

    if (dirtyTileCount < MAX_DIRTY_TILES)
        dirtyTileCount++;
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
    updateConditionsAfterDamage(target.character, damage);
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

int getFortitudeSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_FORTITUDE,
               character.level) +
           getAbilityModifier(character, ABILITY_CONSTITUTION);
}

int getReflexSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_REFLEX,
               character.level) +
           getAbilityModifier(character, ABILITY_DEXTERITY);
}

int getWillSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_WILL,
               character.level) +
           getAbilityModifier(character, ABILITY_WISDOM);
}

#include "../../src/characters/conditions.cpp"
#include "../../src/dungeon/abilityresolver.cpp"
#include "../../src/dungeon/mapeffects.cpp"
#include "../../src/dungeon/movement.cpp"

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
    entity.character.creatureType = type == ENTITY_PLAYER
        ? CREATURE_PLAYER
        : CREATURE_GOBLIN;
    entity.character.level = 1;
    entity.character.abilities = {10, 10, 10, 10, 10, 10};
    entity.character.health.currentHP = hitPoints;
    entity.character.health.maxHP = hitPoints;
    entity.character.magic.currentMP = 10;
    entity.character.magic.maxMP = 10;
}

static void resetResolverControls()
{
    clearMapEffects();
    controlledDistance = 1;
    controlledLineOfSight = true;
    controlledSaveRoll = 10;
    damageApplicationCount = 0;
    saveRollCount = 0;
    dirtyTileMarkCount = 0;
    dirtyTileCount = 0;
    backgroundNeedsRedraw = false;
    redrawType = REDRAW_NONE;
    needsRedraw = false;
    controlledBaseTerrainDifficult = false;
    activeTestEntityCount = 0;
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
    TEST_ASSERT_EQUAL_UINT8(3, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL(
        ABILITY_MAGIC_MISSILE, wizard.magic.knownAbilities[0]);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_GREASE));

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
    TEST_ASSERT_EQUAL(
        SAVE_RESULT_NOT_REQUIRED, result.savingThrow.result);
    TEST_ASSERT_EQUAL_INT(5, result.damage);
    TEST_ASSERT_EQUAL_INT(15, monster.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, damageApplicationCount);
}

void test_wizard_supported_spell_filter_exposes_grease()
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

    TEST_ASSERT_EQUAL_UINT8(12, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL_UINT8(3, supportedCount);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_SLEEP));
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_GREASE));
    TEST_ASSERT_FALSE(isAbilitySupported(ABILITY_FIREBALL));
    TEST_ASSERT_FALSE(isAbilitySupported(ABILITY_ICE_STORM));
}

void test_sleep_metadata_save_dc_and_will_bonus()
{
    resetResolverControls();
    const Ability* sleep = getAbility(ABILITY_SLEEP);
    const Ability* grease = getAbility(ABILITY_GREASE);

    TEST_ASSERT_NOT_NULL(sleep);
    TEST_ASSERT_EQUAL_UINT8(2, sleep->mpCost);
    TEST_ASSERT_EQUAL(TARGET_ENEMY, sleep->target);
    TEST_ASSERT_EQUAL(DELIVERY_TARGET, sleep->delivery);
    TEST_ASSERT_EQUAL(DURATION_ROUNDS, sleep->duration);
    TEST_ASSERT_EQUAL_UINT8(6, sleep->rangeTiles);
    TEST_ASSERT_EQUAL(SAVE_WILL, sleep->saveType);
    TEST_ASSERT_EQUAL(
        CONDITION_SLEEPING, sleep->effects[0].conditionType);
    TEST_ASSERT_EQUAL_INT(3, sleep->effects[0].duration);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_SLEEP));
    TEST_ASSERT_NOT_NULL(grease);
    TEST_ASSERT_EQUAL(TARGET_AREA, grease->target);
    TEST_ASSERT_EQUAL(DELIVERY_AREA, grease->delivery);
    TEST_ASSERT_EQUAL(DURATION_ROUNDS, grease->duration);
    TEST_ASSERT_EQUAL_UINT8(6, grease->rangeTiles);
    TEST_ASSERT_EQUAL(SAVE_REFLEX, grease->saveType);
    TEST_ASSERT_EQUAL_UINT8(1, grease->areaRadiusTiles);
    TEST_ASSERT_EQUAL(MAP_EFFECT_GREASE, grease->mapEffectType);
    TEST_ASSERT_EQUAL_UINT8(3, grease->mapEffectDurationRounds);
    TEST_ASSERT_EQUAL(
        CONDITION_PRONE, grease->effects[0].conditionType);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_GREASE));

    Entity wizard;
    Entity target;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(target, ENTITY_MONSTER, TEAM_MONSTER);
    wizard.character.abilities.intelligence = 18;
    target.character.characterClass = CLASS_WIZARD;
    target.character.level = 3;
    target.character.abilities.wisdom = 14;
    controlledSaveRoll = 9;

    AbilitySavingThrow savingThrow = resolveAbilitySavingThrow(
        wizard, target, *sleep);

    TEST_ASSERT_EQUAL_INT(15, getAbilitySaveDC(wizard, *sleep));
    TEST_ASSERT_EQUAL_INT(5, getAbilitySaveBonus(
        target.character, SAVE_WILL));
    TEST_ASSERT_EQUAL_INT(9, savingThrow.roll);
    TEST_ASSERT_EQUAL_INT(5, savingThrow.bonus);
    TEST_ASSERT_EQUAL_INT(14, savingThrow.total);
    TEST_ASSERT_EQUAL_INT(15, savingThrow.dc);
    TEST_ASSERT_EQUAL(SAVE_RESULT_FAILURE, savingThrow.result);
}

void test_failed_sleep_save_applies_condition_and_spends_cast()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    wizard.character.abilities.intelligence = 18;
    controlledSaveRoll = 1;

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_SLEEP);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL(SAVE_RESULT_FAILURE, result.savingThrow.result);
    TEST_ASSERT_EQUAL(CONDITION_SLEEPING, result.conditionApplied);
    TEST_ASSERT_EQUAL_INT(3, result.conditionDuration);
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_SLEEPING));
    TEST_ASSERT_EQUAL_INT(
        3,
        getCondition(
            monster.character,
            CONDITION_SLEEPING)->roundsRemaining);
    TEST_ASSERT_EQUAL_INT(
        0,
        getCondition(
            monster.character,
            CONDITION_SLEEPING)->value);
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
}

void test_successful_sleep_save_resists_but_spends_cast()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    wizard.character.abilities.intelligence = 18;
    controlledSaveRoll = 20;

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_SLEEP);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL(SAVE_RESULT_SUCCESS, result.savingThrow.result);
    TEST_ASSERT_EQUAL(CONDITION_NONE, result.conditionApplied);
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_SLEEPING));
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
}

void test_invalid_and_undead_sleep_targets_spend_nothing()
{
    resetResolverControls();
    Entity wizard;
    Entity target;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(target, ENTITY_PLAYER, TEAM_PLAYER);

    AbilityResolution friendly = resolveAbility(
        wizard, &target, ABILITY_SLEEP);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_INVALID_TARGET, friendly.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);

    target.character.team = TEAM_MONSTER;
    target.character.creatureType = CREATURE_ZOMBIE;
    AbilityResolution undead = resolveAbility(
        wizard, &target, ABILITY_SLEEP);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_TARGET_IMMUNE, undead.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
    TEST_ASSERT_FALSE(hasCondition(
        target.character, CONDITION_SLEEPING));
}

void test_condition_capacity_failure_does_not_spend_cast()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    controlledSaveRoll = 1;

    for (int rawType = CONDITION_NONE + 1;
         rawType < CONDITION_MAX &&
         monster.character.conditions.count <
             MAX_CONDITIONS_PER_CHARACTER;
         rawType++)
    {
        ConditionType type = static_cast<ConditionType>(rawType);
        if (type != CONDITION_SLEEPING)
        {
            TEST_ASSERT_TRUE(addCondition(
                monster.character, type, 0, 2));
        }
    }

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_SLEEP);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_CONDITION_LIMIT, result.result);
    TEST_ASSERT_EQUAL_UINT8(
        MAX_CONDITIONS_PER_CHARACTER,
        monster.character.conditions.count);
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_SLEEPING));
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
}

void test_actual_damage_wakes_sleeping_target()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    TEST_ASSERT_TRUE(addCondition(
        monster.character, CONDITION_SLEEPING, 0, 3));

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_SLEEPING));
}

void test_monster_sleep_uses_the_same_save_condition_resolver()
{
    resetResolverControls();
    Entity monsterCaster;
    Entity player;
    initializeEntity(monsterCaster, ENTITY_MONSTER, TEAM_MONSTER);
    initializeEntity(player, ENTITY_PLAYER, TEAM_PLAYER);
    monsterCaster.character.abilities.intelligence = 18;
    monsterCaster.character.magic.currentMP = 2;
    monsterCaster.character.magic.maxMP = 2;
    controlledSaveRoll = 1;

    AbilityResolution result = resolveAbility(
        monsterCaster, &player, ABILITY_SLEEP);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL(SAVE_RESULT_FAILURE, result.savingThrow.result);
    TEST_ASSERT_TRUE(hasCondition(
        player.character, CONDITION_SLEEPING));
    TEST_ASSERT_EQUAL_INT(0, monsterCaster.character.magic.currentMP);
    TEST_ASSERT_TRUE(monsterCaster.turn.standardActionUsed);
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

void test_grease_cast_creates_area_and_trips_initial_occupant()
{
    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& wizard = activeTestEntities[0];
    Entity& monster = activeTestEntities[1];
    wizard.x = 0;
    wizard.y = 0;
    monster.x = 3;
    monster.y = 3;
    wizard.character.abilities.intelligence = 18;
    controlledSaveRoll = 1;

    AbilityResolution result = resolveAbilityAt(
        wizard, 3, 3, ABILITY_GREASE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_TRUE(result.mapEffectCreated);
    TEST_ASSERT_EQUAL_UINT8(1, result.targetsAffected);
    TEST_ASSERT_EQUAL_UINT8(0, result.targetsResisted);
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_PRONE));
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
    TEST_ASSERT_TRUE(hasMapEffectAt(MAP_EFFECT_GREASE, 2, 2));
    TEST_ASSERT_TRUE(hasMapEffectAt(MAP_EFFECT_GREASE, 4, 4));
    TEST_ASSERT_FALSE(hasMapEffectAt(MAP_EFFECT_GREASE, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 3, 3));
    TEST_ASSERT_GREATER_THAN_UINT8(0, dirtyTileMarkCount);
}

void test_grease_successful_reflex_save_resists_but_spends_cast()
{
    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& wizard = activeTestEntities[0];
    Entity& monster = activeTestEntities[1];
    wizard.x = 0;
    wizard.y = 0;
    monster.x = 3;
    monster.y = 3;
    controlledSaveRoll = 20;

    AbilityResolution result = resolveAbilityAt(
        wizard, 3, 3, ABILITY_GREASE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL_UINT8(0, result.targetsAffected);
    TEST_ASSERT_EQUAL_UINT8(1, result.targetsResisted);
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_PRONE));
    TEST_ASSERT_EQUAL_INT(8, wizard.character.magic.currentMP);
    TEST_ASSERT_TRUE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
}

void test_invalid_grease_casts_are_transactional()
{
    Entity wizard;

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    controlledDistance = 7;
    AbilityResolution range = resolveAbilityAt(
        wizard, 3, 3, ABILITY_GREASE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_OUT_OF_RANGE, range.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
    TEST_ASSERT_NULL(getMapEffectAt(3, 3));

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    controlledLineOfSight = false;
    AbilityResolution los = resolveAbilityAt(
        wizard, 3, 3, ABILITY_GREASE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_NO_LINE_OF_SIGHT, los.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
    TEST_ASSERT_NULL(getMapEffectAt(3, 3));

    resetResolverControls();
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    MapEffect effect;
    effect.active = true;
    effect.type = MAP_EFFECT_GREASE;
    effect.roundsRemaining = 3;

    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        effect.x = static_cast<int8_t>(i);
        effect.y = 8;
        TEST_ASSERT_NOT_NULL(addMapEffect(effect));
    }

    AbilityResolution capacity = resolveAbilityAt(
        wizard, 3, 3, ABILITY_GREASE);
    TEST_ASSERT_EQUAL(
        ABILITY_RESULT_MAP_EFFECT_LIMIT, capacity.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
}

void test_movement_cost_and_grease_expiration_are_shared()
{
    resetResolverControls();
    Entity player;
    Entity monster;
    initializeEntity(player, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(monster, 5, 5));

    controlledBaseTerrainDifficult = true;
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 5, 5));
    controlledBaseTerrainDifficult = false;

    MapEffect effect;
    effect.active = true;
    effect.type = MAP_EFFECT_GREASE;
    effect.x = 5;
    effect.y = 5;
    effect.radius = 1;
    effect.roundsRemaining = 3;
    TEST_ASSERT_NOT_NULL(addMapEffect(effect));
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 5, 5));

    player.turn.movementRemaining = 1;
    TEST_ASSERT_FALSE(canAffordMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(1, player.turn.movementRemaining);
    player.turn.movementRemaining = 2;
    TEST_ASSERT_TRUE(canAffordMovementCost(player, 5, 5));
    spendMovementCost(player, 5, 5);
    TEST_ASSERT_EQUAL_UINT8(0, player.turn.movementRemaining);

    tickMapEffects();
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 5, 5));
    tickMapEffects();
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 5, 5));
    tickMapEffects();
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(monster, 5, 5));
}

void test_entering_grease_saves_once_and_prone_stands_without_moving()
{
    resetResolverControls();
    Entity monster;
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    monster.x = 5;
    monster.y = 5;
    monster.turn.movementRemaining = 3;
    controlledSaveRoll = 1;

    MapEffect effect;
    effect.active = true;
    effect.type = MAP_EFFECT_GREASE;
    effect.x = 5;
    effect.y = 5;
    effect.radius = 1;
    effect.roundsRemaining = 3;
    effect.saveType = SAVE_REFLEX;
    effect.saveDC = 20;
    effect.conditionType = CONDITION_PRONE;
    TEST_ASSERT_NOT_NULL(addMapEffect(effect));

    TEST_ASSERT_EQUAL(
        CONDITION_PRONE, handleEnteredTile(monster, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_PRONE));

    // Terrain queries and a round tick while stationary do not invoke the
    // movement-entry hook again.
    TEST_ASSERT_EQUAL_UINT8(2, getMovementCost(monster, 5, 5));
    TEST_ASSERT_NOT_NULL(getMapEffectAt(5, 5));
    tickMapEffects();
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);

    uint8_t oldX = monster.x;
    uint8_t oldY = monster.y;
    TEST_ASSERT_EQUAL(
        STAND_COMPLETED, tryStandForMovement(monster, true));
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_PRONE));
    TEST_ASSERT_EQUAL_UINT8(2, monster.turn.movementRemaining);
    TEST_ASSERT_EQUAL_UINT8(oldX, monster.x);
    TEST_ASSERT_EQUAL_UINT8(oldY, monster.y);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_wizard_magic_initialization_learns_magic_missile);
    RUN_TEST(test_magic_missile_success_uses_cost_action_and_damage);
    RUN_TEST(test_wizard_supported_spell_filter_exposes_grease);
    RUN_TEST(test_sleep_metadata_save_dc_and_will_bonus);
    RUN_TEST(test_failed_sleep_save_applies_condition_and_spends_cast);
    RUN_TEST(test_successful_sleep_save_resists_but_spends_cast);
    RUN_TEST(test_invalid_and_undead_sleep_targets_spend_nothing);
    RUN_TEST(test_condition_capacity_failure_does_not_spend_cast);
    RUN_TEST(test_actual_damage_wakes_sleeping_target);
    RUN_TEST(test_monster_sleep_uses_the_same_save_condition_resolver);
    RUN_TEST(test_failed_casts_do_not_mutate_state);
    RUN_TEST(test_invalid_friendly_dead_and_used_action_targets_are_rejected);
    RUN_TEST(test_duration_effect_is_rejected_without_state_changes);
    RUN_TEST(test_monster_magic_missile_uses_the_same_resolver);
    RUN_TEST(test_grease_cast_creates_area_and_trips_initial_occupant);
    RUN_TEST(test_grease_successful_reflex_save_resists_but_spends_cast);
    RUN_TEST(test_invalid_grease_casts_are_transactional);
    RUN_TEST(test_movement_cost_and_grease_expiration_are_shared);
    RUN_TEST(test_entering_grease_saves_once_and_prone_stands_without_moving);
    UNITY_END();
}

void loop()
{
}
