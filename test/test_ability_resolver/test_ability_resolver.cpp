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
static int controlledBlockedLOSX = -1;
static int controlledBlockedLOSY = -1;
static int controlledSaveRoll = 10;
static uint8_t damageApplicationCount = 0;
static uint8_t saveRollCount = 0;
static uint8_t dirtyTileMarkCount = 0;
static bool controlledBaseTerrainDifficult = false;
static Entity activeTestEntities[4];
static uint8_t activeTestEntityCount = 0;

extern const DirectionOffset directionOffsets[] =
{
    {  0, -1 },
    {  1, -1 },
    {  1,  0 },
    {  1,  1 },
    {  0,  1 },
    { -1,  1 },
    { -1,  0 },
    { -1, -1 }
};

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
    int targetX,
    int targetY)
{
    return controlledLineOfSight &&
           (targetX != controlledBlockedLOSX ||
            targetY != controlledBlockedLOSY);
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

TileType getActiveMapTile(int x, int y)
{
    return isInsideActiveMap(x, y) ? TILE_FLOOR : TILE_WALL;
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

bool entityOccupiesTile(const Entity& entity, int tileX, int tileY)
{
    return tileX >= entity.x &&
           tileX < entity.x + getEntityTileWidth(entity) &&
           tileY >= entity.y &&
           tileY < entity.y + getEntityTileHeight(entity);
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
#include "../../src/data/entitytraits.cpp"
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
    controlledBlockedLOSX = -1;
    controlledBlockedLOSY = -1;
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
    TEST_ASSERT_EQUAL_UINT8(4, wizard.magic.knownAbilityCount);
    TEST_ASSERT_EQUAL(
        ABILITY_MAGIC_MISSILE, wizard.magic.knownAbilities[0]);
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_SLEEP));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_GREASE));
    TEST_ASSERT_TRUE(knowsAbility(wizard, ABILITY_COLOR_SPRAY));

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

void test_wizard_supported_spell_filter_exposes_current_spells()
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
    TEST_ASSERT_EQUAL_UINT8(4, supportedCount);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_MAGIC_MISSILE));
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_SLEEP));
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_GREASE));
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_COLOR_SPRAY));
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
    TEST_ASSERT_TRUE(addCondition(
        monster.character, CONDITION_BLESSED, 1, 3));

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_FALSE(hasCondition(
        monster.character, CONDITION_SLEEPING));
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_BLESSED));
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

void test_sleeping_caster_is_rejected_by_generic_action_validation()
{
    resetResolverControls();
    Entity sleepingCaster;
    Entity target;
    initializeEntity(
        sleepingCaster, ENTITY_MONSTER, TEAM_MONSTER);
    initializeEntity(target, ENTITY_PLAYER, TEAM_PLAYER);
    TEST_ASSERT_TRUE(addCondition(
        sleepingCaster.character, CONDITION_SLEEPING, 0, 3));

    const int originalMP = sleepingCaster.character.magic.currentMP;
    const int originalHP = target.character.health.currentHP;
    AbilityResolution result = resolveAbility(
        sleepingCaster, &target, ABILITY_MAGIC_MISSILE);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_INVALID_CASTER, result.result);
    TEST_ASSERT_EQUAL_INT(
        originalMP, sleepingCaster.character.magic.currentMP);
    TEST_ASSERT_EQUAL_INT(originalHP, target.character.health.currentHP);
    TEST_ASSERT_FALSE(sleepingCaster.turn.standardActionUsed);
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
    uint8_t dirtyMarksBeforeExpiration = dirtyTileMarkCount;
    tickMapEffects();
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(player, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(1, getMovementCost(monster, 5, 5));
    TEST_ASSERT_GREATER_THAN_UINT8(
        dirtyMarksBeforeExpiration, dirtyTileMarkCount);
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
    TEST_ASSERT_TRUE(addCondition(
        monster.character, CONDITION_BLESSED, 1, 3));

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
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_BLESSED));

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
    TEST_ASSERT_TRUE(hasCondition(
        monster.character, CONDITION_BLESSED));
    TEST_ASSERT_EQUAL_UINT8(2, monster.turn.movementRemaining);
    TEST_ASSERT_EQUAL_UINT8(oldX, monster.x);
    TEST_ASSERT_EQUAL_UINT8(oldY, monster.y);
}

void test_color_spray_metadata_and_shared_cone_geometry()
{
    resetResolverControls();
    const Ability* colorSpray = getAbility(ABILITY_COLOR_SPRAY);

    TEST_ASSERT_NOT_NULL(colorSpray);
    TEST_ASSERT_EQUAL_UINT8(2, colorSpray->mpCost);
    TEST_ASSERT_EQUAL(TARGET_AREA, colorSpray->target);
    TEST_ASSERT_EQUAL(DELIVERY_CONE, colorSpray->delivery);
    TEST_ASSERT_EQUAL(DURATION_INSTANT, colorSpray->duration);
    TEST_ASSERT_EQUAL_UINT8(3, colorSpray->rangeTiles);
    TEST_ASSERT_EQUAL(SAVE_WILL, colorSpray->saveType);
    TEST_ASSERT_EQUAL_UINT8(2, colorSpray->effectCount);
    TEST_ASSERT_EQUAL(
        CONDITION_STUNNED, colorSpray->effects[0].conditionType);
    TEST_ASSERT_EQUAL_INT(2, colorSpray->effects[0].duration);
    TEST_ASSERT_EQUAL(
        CONDITION_BLINDED, colorSpray->effects[1].conditionType);
    TEST_ASSERT_EQUAL_INT(4, colorSpray->effects[1].duration);
    TEST_ASSERT_TRUE(isAbilitySupported(ABILITY_COLOR_SPRAY));
    TEST_ASSERT_TRUE(isDirectionalAbility(ABILITY_COLOR_SPRAY));

    Entity caster;
    initializeEntity(caster, ENTITY_PLAYER, TEAM_PLAYER);
    caster.x = 5;
    caster.y = 5;

    uint8_t northTileCount = 0;
    for (int y = 0; y < getActiveMapHeight(); y++)
    {
        for (int x = 0; x < getActiveMapWidth(); x++)
        {
            if (isTileInDirectionalAbilityArea(
                    caster, ABILITY_COLOR_SPRAY, DIR_NORTH, x, y))
            {
                northTileCount++;
            }
        }
    }

    // Length three uses rows 1, 3, and 5 tiles wide: nine tiles total.
    TEST_ASSERT_EQUAL_UINT8(9, northTileCount);
    TEST_ASSERT_TRUE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_NORTH, 5, 4));
    TEST_ASSERT_TRUE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_NORTH, 4, 3));
    TEST_ASSERT_TRUE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_NORTH, 7, 2));
    TEST_ASSERT_FALSE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_NORTH, 2, 2));
    TEST_ASSERT_FALSE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_NORTH, 5, 6));

    TEST_ASSERT_TRUE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_EAST, 6, 5));
    TEST_ASSERT_TRUE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_EAST, 8, 7));
    TEST_ASSERT_FALSE(isTileInDirectionalAbilityArea(
        caster, ABILITY_COLOR_SPRAY, DIR_EAST, 5, 4));
}

void test_color_spray_failed_saves_use_effective_hd_tiers()
{
    const uint8_t hitDice[] = { 2, 4, 5 };
    const int expectedStun[] = { 2, 1, 1 };
    const int expectedBlind[] = { 4, 2, 0 };

    for (uint8_t tier = 0; tier < 3; tier++)
    {
        resetResolverControls();
        activeTestEntityCount = 2;
        initializeEntity(
            activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
        initializeEntity(
            activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
        Entity& caster = activeTestEntities[0];
        Entity& target = activeTestEntities[1];
        Monster definition = {};
        definition.hitDice = hitDice[tier];
        target.monster = &definition;
        caster.x = 5;
        caster.y = 5;
        target.x = 5;
        target.y = 3;
        caster.character.abilities.intelligence = 18;
        controlledSaveRoll = 1;

        AbilityResolution result = resolveAbilityInDirection(
            caster, DIR_NORTH, ABILITY_COLOR_SPRAY);

        TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
        TEST_ASSERT_EQUAL(SAVE_RESULT_FAILURE, result.savingThrow.result);
        TEST_ASSERT_EQUAL_INT(15, result.savingThrow.dc);
        TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
        TEST_ASSERT_EQUAL_UINT8(1, result.targetsAffected);
        TEST_ASSERT_EQUAL_UINT8(0, result.targetsResisted);
        TEST_ASSERT_EQUAL_UINT8(0, result.targetsImmune);
        TEST_ASSERT_EQUAL_INT(8, caster.character.magic.currentMP);
        TEST_ASSERT_TRUE(caster.turn.standardActionUsed);
        TEST_ASSERT_TRUE(hasCondition(
            target.character, CONDITION_STUNNED));
        TEST_ASSERT_EQUAL_INT(
            expectedStun[tier],
            getCondition(
                target.character,
                CONDITION_STUNNED)->roundsRemaining);

        if (expectedBlind[tier] > 0)
        {
            TEST_ASSERT_TRUE(hasCondition(
                target.character, CONDITION_BLINDED));
            TEST_ASSERT_EQUAL_INT(
                expectedBlind[tier],
                getCondition(
                    target.character,
                    CONDITION_BLINDED)->roundsRemaining);
        }
        else
        {
            TEST_ASSERT_FALSE(hasCondition(
                target.character, CONDITION_BLINDED));
        }
    }
}

void test_color_spray_save_wall_team_and_area_filters_are_shared()
{
    resetResolverControls();
    activeTestEntityCount = 3;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    initializeEntity(
        activeTestEntities[2], ENTITY_PLAYER, TEAM_PLAYER);
    Entity& caster = activeTestEntities[0];
    Entity& enemy = activeTestEntities[1];
    Entity& ally = activeTestEntities[2];
    caster.x = 5;
    caster.y = 5;
    enemy.x = 5;
    enemy.y = 3;
    ally.x = 4;
    ally.y = 3;
    controlledSaveRoll = 20;

    AbilityResolution resisted = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, resisted.result);
    TEST_ASSERT_EQUAL(SAVE_RESULT_SUCCESS, resisted.savingThrow.result);
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(0, resisted.targetsAffected);
    TEST_ASSERT_EQUAL_UINT8(1, resisted.targetsResisted);
    TEST_ASSERT_FALSE(hasCondition(
        enemy.character, CONDITION_STUNNED));
    TEST_ASSERT_FALSE(hasCondition(
        ally.character, CONDITION_STUNNED));

    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& wallCaster = activeTestEntities[0];
    Entity& blockedEnemy = activeTestEntities[1];
    wallCaster.x = 5;
    wallCaster.y = 5;
    blockedEnemy.x = 5;
    blockedEnemy.y = 2;
    controlledBlockedLOSX = 5;
    controlledBlockedLOSY = 2;
    controlledSaveRoll = 1;

    AbilityResolution blocked = resolveAbilityInDirection(
        wallCaster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, blocked.result);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(0, blocked.targetsAffected);
    TEST_ASSERT_FALSE(hasCondition(
        blockedEnemy.character, CONDITION_STUNNED));

    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& areaCaster = activeTestEntities[0];
    Entity& outsideEnemy = activeTestEntities[1];
    areaCaster.x = 5;
    areaCaster.y = 5;
    outsideEnemy.x = 2;
    outsideEnemy.y = 2;
    controlledSaveRoll = 1;

    AbilityResolution outside = resolveAbilityInDirection(
        areaCaster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, outside.result);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(0, outside.targetsAffected);
}

void test_color_spray_sight_immunity_precedes_saving_throw()
{
    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& caster = activeTestEntities[0];
    Entity& sightlessTarget = activeTestEntities[1];
    Monster sightlessDefinition = {};
    sightlessDefinition.hitDice = 3;
    sightlessDefinition.sightless = true;
    sightlessTarget.monster = &sightlessDefinition;
    caster.x = 5;
    caster.y = 5;
    sightlessTarget.x = 5;
    sightlessTarget.y = 3;
    controlledSaveRoll = 1;

    TEST_ASSERT_FALSE(canSee(sightlessTarget));
    AbilityResolution sightless = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, sightless.result);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(1, sightless.targetsImmune);
    TEST_ASSERT_FALSE(hasCondition(
        sightlessTarget.character, CONDITION_STUNNED));

    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& secondCaster = activeTestEntities[0];
    Entity& blindTarget = activeTestEntities[1];
    secondCaster.x = 5;
    secondCaster.y = 5;
    blindTarget.x = 5;
    blindTarget.y = 3;
    TEST_ASSERT_TRUE(addCondition(
        blindTarget.character, CONDITION_BLINDED, 0, 2));

    TEST_ASSERT_FALSE(canSee(blindTarget));
    AbilityResolution blind = resolveAbilityInDirection(
        secondCaster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, blind.result);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(1, blind.targetsImmune);
    TEST_ASSERT_EQUAL_INT(
        2,
        getCondition(
            blindTarget.character,
            CONDITION_BLINDED)->roundsRemaining);
    TEST_ASSERT_FALSE(hasCondition(
        blindTarget.character, CONDITION_STUNNED));
}

void test_color_spray_conditions_expire_independently_and_preserve_others()
{
    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(
        activeTestEntities[1], ENTITY_MONSTER, TEAM_MONSTER);
    Entity& caster = activeTestEntities[0];
    Entity& target = activeTestEntities[1];
    Monster definition = {};
    definition.hitDice = 2;
    target.monster = &definition;
    caster.x = 5;
    caster.y = 5;
    target.x = 5;
    target.y = 3;
    controlledSaveRoll = 1;
    TEST_ASSERT_TRUE(addCondition(
        target.character, CONDITION_BLESSED, 1, 5));

    AbilityResolution result = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_FALSE(canCharacterAct(target.character));
    TEST_ASSERT_FALSE(canSee(target));
    TEST_ASSERT_EQUAL_INT(-1, getConditionAttackModifier(target.character));
    TEST_ASSERT_EQUAL_INT(-2, getConditionArmorClassModifier(target.character));

    ConditionTurnResult first = processConditionsAtTurnStart(
        target.character);
    TEST_ASSERT_TRUE(first.actionPrevented);
    TEST_ASSERT_EQUAL_INT(
        1,
        getCondition(
            target.character,
            CONDITION_STUNNED)->roundsRemaining);
    TEST_ASSERT_EQUAL_INT(
        3,
        getCondition(
            target.character,
            CONDITION_BLINDED)->roundsRemaining);

    ConditionTurnResult second = processConditionsAtTurnStart(
        target.character);
    TEST_ASSERT_TRUE(second.actionPrevented);
    TEST_ASSERT_FALSE(hasCondition(
        target.character, CONDITION_STUNNED));
    TEST_ASSERT_TRUE(canCharacterAct(target.character));
    TEST_ASSERT_TRUE(hasCondition(
        target.character, CONDITION_BLINDED));

    ConditionTurnResult third = processConditionsAtTurnStart(
        target.character);
    TEST_ASSERT_FALSE(third.actionPrevented);
    TEST_ASSERT_TRUE(hasCondition(
        target.character, CONDITION_BLINDED));
    ConditionTurnResult fourth = processConditionsAtTurnStart(
        target.character);
    TEST_ASSERT_FALSE(fourth.actionPrevented);
    TEST_ASSERT_FALSE(hasCondition(
        target.character, CONDITION_BLINDED));
    TEST_ASSERT_TRUE(hasCondition(
        target.character, CONDITION_BLESSED));
}

void test_entity_traits_and_blinded_visual_targeting_are_generic()
{
    resetResolverControls();
    Entity wizard;
    Entity monster;
    initializeEntity(wizard, ENTITY_PLAYER, TEAM_PLAYER);
    initializeEntity(monster, ENTITY_MONSTER, TEAM_MONSTER);
    wizard.character.level = 7;
    Monster definition = {};
    definition.hitDice = 4;
    monster.monster = &definition;

    TEST_ASSERT_EQUAL_UINT8(7, getEffectiveHitDice(wizard));
    TEST_ASSERT_EQUAL_UINT8(4, getEffectiveHitDice(monster));
    TEST_ASSERT_TRUE(canSee(wizard));
    TEST_ASSERT_TRUE(addCondition(
        wizard.character, CONDITION_BLINDED, 0, 2));
    TEST_ASSERT_FALSE(canSee(wizard));

    AbilityResolution result = resolveAbility(
        wizard, &monster, ABILITY_MAGIC_MISSILE);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_NO_LINE_OF_SIGHT, result.result);
    TEST_ASSERT_EQUAL_INT(10, wizard.character.magic.currentMP);
    TEST_ASSERT_FALSE(wizard.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_INT(20, monster.character.health.currentHP);
}

void test_directional_cast_validation_is_transactional_and_stun_blocks_casting()
{
    Entity caster;

    resetResolverControls();
    initializeEntity(caster, ENTITY_PLAYER, TEAM_PLAYER);
    caster.character.magic.currentMP = 1;
    AbilityResolution noMP = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_NOT_ENOUGH_MP, noMP.result);
    TEST_ASSERT_EQUAL_INT(1, caster.character.magic.currentMP);
    TEST_ASSERT_FALSE(caster.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);

    resetResolverControls();
    initializeEntity(caster, ENTITY_PLAYER, TEAM_PLAYER);
    caster.turn.standardActionUsed = true;
    AbilityResolution usedAction = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(
        ABILITY_RESULT_NO_STANDARD_ACTION, usedAction.result);
    TEST_ASSERT_EQUAL_INT(10, caster.character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(0, saveRollCount);

    resetResolverControls();
    initializeEntity(caster, ENTITY_PLAYER, TEAM_PLAYER);
    TEST_ASSERT_TRUE(addCondition(
        caster.character, CONDITION_STUNNED, 0, 1));
    TEST_ASSERT_FALSE(canCharacterAct(caster.character));
    AbilityResolution stunned = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);
    TEST_ASSERT_EQUAL(ABILITY_RESULT_INVALID_CASTER, stunned.result);
    TEST_ASSERT_EQUAL_INT(10, caster.character.magic.currentMP);
    TEST_ASSERT_FALSE(caster.turn.standardActionUsed);
}

void test_monster_caster_uses_the_same_directional_resolver()
{
    resetResolverControls();
    activeTestEntityCount = 2;
    initializeEntity(
        activeTestEntities[0], ENTITY_MONSTER, TEAM_MONSTER);
    initializeEntity(
        activeTestEntities[1], ENTITY_PLAYER, TEAM_PLAYER);
    Entity& caster = activeTestEntities[0];
    Entity& player = activeTestEntities[1];
    caster.x = 5;
    caster.y = 5;
    player.x = 5;
    player.y = 3;
    caster.character.abilities.intelligence = 18;
    controlledSaveRoll = 1;

    AbilityResolution result = resolveAbilityInDirection(
        caster, DIR_NORTH, ABILITY_COLOR_SPRAY);

    TEST_ASSERT_EQUAL(ABILITY_RESULT_SUCCESS, result.result);
    TEST_ASSERT_EQUAL_UINT8(1, saveRollCount);
    TEST_ASSERT_EQUAL_UINT8(1, result.targetsAffected);
    TEST_ASSERT_TRUE(hasCondition(
        player.character, CONDITION_STUNNED));
    TEST_ASSERT_TRUE(hasCondition(
        player.character, CONDITION_BLINDED));
    TEST_ASSERT_EQUAL_INT(8, caster.character.magic.currentMP);
    TEST_ASSERT_TRUE(caster.turn.standardActionUsed);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_wizard_magic_initialization_learns_magic_missile);
    RUN_TEST(test_magic_missile_success_uses_cost_action_and_damage);
    RUN_TEST(test_wizard_supported_spell_filter_exposes_current_spells);
    RUN_TEST(test_sleep_metadata_save_dc_and_will_bonus);
    RUN_TEST(test_failed_sleep_save_applies_condition_and_spends_cast);
    RUN_TEST(test_successful_sleep_save_resists_but_spends_cast);
    RUN_TEST(test_invalid_and_undead_sleep_targets_spend_nothing);
    RUN_TEST(test_condition_capacity_failure_does_not_spend_cast);
    RUN_TEST(test_actual_damage_wakes_sleeping_target);
    RUN_TEST(test_monster_sleep_uses_the_same_save_condition_resolver);
    RUN_TEST(test_sleeping_caster_is_rejected_by_generic_action_validation);
    RUN_TEST(test_failed_casts_do_not_mutate_state);
    RUN_TEST(test_invalid_friendly_dead_and_used_action_targets_are_rejected);
    RUN_TEST(test_duration_effect_is_rejected_without_state_changes);
    RUN_TEST(test_monster_magic_missile_uses_the_same_resolver);
    RUN_TEST(test_grease_cast_creates_area_and_trips_initial_occupant);
    RUN_TEST(test_grease_successful_reflex_save_resists_but_spends_cast);
    RUN_TEST(test_invalid_grease_casts_are_transactional);
    RUN_TEST(test_movement_cost_and_grease_expiration_are_shared);
    RUN_TEST(test_entering_grease_saves_once_and_prone_stands_without_moving);
    RUN_TEST(test_color_spray_metadata_and_shared_cone_geometry);
    RUN_TEST(test_color_spray_failed_saves_use_effective_hd_tiers);
    RUN_TEST(test_color_spray_save_wall_team_and_area_filters_are_shared);
    RUN_TEST(test_color_spray_sight_immunity_precedes_saving_throw);
    RUN_TEST(
        test_color_spray_conditions_expire_independently_and_preserve_others);
    RUN_TEST(test_entity_traits_and_blinded_visual_targeting_are_generic);
    RUN_TEST(
        test_directional_cast_validation_is_transactional_and_stun_blocks_casting);
    RUN_TEST(test_monster_caster_uses_the_same_directional_resolver);
    UNITY_END();
}

void loop()
{
}
