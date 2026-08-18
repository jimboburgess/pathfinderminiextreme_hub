#include <Arduino.h>
#include <unity.h>

#include "../../src/dungeon/dungeon.h"
#include "../../src/dungeon/combat.h"
#include "../../src/data/entityspawn.h"

// This embedded Unity suite compiles the dungeon lifecycle implementation
// directly and supplies narrow stubs for geometry, rendering, and hardware-
// adjacent systems. It exercises the real room-runtime persistence code.
Character player = {};
GameState gameState = GAME_TOWN;
TownOption townSelection = TOWN_STAY_HOME;
RedrawType redrawType = REDRAW_NONE;
Direction moveDirection = DIR_NORTH;
Direction previousMoveDirection = DIR_NORTH;
MapPosition previousPlayerPosition = {};
bool needsRedraw = false;
bool backgroundNeedsRedraw = false;

static uint8_t generatedRoomCount = 0;
static uint8_t mapEffectClearCount = 0;
static Entity* activeTestEntities = nullptr;
static uint8_t activeTestEntityCount = 0;

Combat combat = {};

Entity* getActiveMapEntities(uint8_t& entityCount)
{
    entityCount = activeTestEntityCount;
    return activeTestEntities;
}

void clearMapEffects()
{
    mapEffectClearCount++;
}

bool removeCondition(Character& character, ConditionType type)
{
    for (uint8_t i = 0; i < character.conditions.count; i++)
    {
        if (character.conditions.conditions[i].type != type)
            continue;

        for (uint8_t j = i + 1; j < character.conditions.count; j++)
        {
            character.conditions.conditions[j - 1] =
                character.conditions.conditions[j];
        }

        character.conditions.count--;
        character.conditions.conditions[character.conditions.count] =
            Condition{};
        return true;
    }

    return false;
}

void setGameMessage(const char*)
{
}

const uint16_t* getPlayerSprite(CharacterClass)
{
    return nullptr;
}

void clearRoomConnections(DungeonRoom& room)
{
    room.connectionCount = 0;
}

void populateRoomConnections(DungeonRoom&)
{
}

RoomShape randomProductionRoomShape(const DungeonRoom&)
{
    return SHAPE_SQUARE;
}

void generateRoom(DungeonRoom& room)
{
    generatedRoomCount++;

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
    }
}

bool placeGiantSpiderEncounter(DungeonRoom&)
{
    return true;
}

bool getRoomEntryPosition(
    const DungeonRoom&,
    RoomEntry,
    uint8_t& x,
    uint8_t& y)
{
    x = ROOM_SIZE / 2;
    y = ROOM_SIZE / 2;
    return true;
}

Entity* findFreeEntity(Entity entities[], uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            return &entities[i];
    }

    return nullptr;
}

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = findFreeEntity(entities, entityCount);

    if (entity == nullptr)
    {
        if (entityCount >= MAX_ENTITIES)
            return nullptr;

        entity = &entities[entityCount++];
    }

    *entity = Entity{};
    entity->active = true;
    entity->type = type;
    entity->x = x;
    entity->y = y;
    return entity;
}

Entity* spawnMonster(
    Entity* entities,
    uint8_t& entityCount,
    MonsterID monsterID,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = spawnEntity(
        entities, entityCount, ENTITY_MONSTER, x, y);

    if (entity != nullptr)
    {
        entity->monsterID = monsterID;
        entity->character.team = TEAM_MONSTER;
        entity->character.state = STATE_ALIVE;
    }

    return entity;
}

Entity* getPlayerEntity(Entity entities[], uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].active && entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].active && entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

Entity* getEntityAt(
    Entity entities[],
    uint8_t entityCount,
    uint8_t x,
    uint8_t y)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].active && entities[i].x == x && entities[i].y == y)
            return &entities[i];
    }

    return nullptr;
}

#include "../../src/dungeon/combatabort.cpp"
#include "../../src/dungeon/dungeon.cpp"

static void configureLoadedRoom(uint8_t roomIndex)
{
    resetDungeonRun(dungeon);
    dungeon.runActive = true;
    dungeon.currentRoom = roomIndex;
    dungeon.loadedRoom = roomIndex;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];
    runtime.initialized = true;
    runtime.entityCount = 2;
    runtime.playerSlot = 1;

    runtime.entities[0] = Entity{};
    runtime.entities[0].active = true;
    runtime.entities[0].type = ENTITY_MONSTER;
    runtime.entities[0].monsterID = MONSTER_GOBLIN_SCIMITAR;
    runtime.entities[0].character.team = TEAM_MONSTER;
    runtime.entities[0].character.state = STATE_ALIVE;
    runtime.entities[0].character.health.currentHP = 3;
    runtime.entities[0].character.health.maxHP = 8;
    runtime.entities[0].turn.standardActionUsed = true;

    runtime.entities[1] = Entity{};
    runtime.entities[1].active = true;
    runtime.entities[1].type = ENTITY_PLAYER;
    runtime.entities[1].character.state = STATE_ALIVE;
    runtime.entities[1].character.health.currentHP = 7;
    runtime.entities[1].character.health.maxHP = 12;
    runtime.entities[1].character.magic.currentMP = 2;
    runtime.entities[1].character.magic.maxMP = 6;
    runtime.entities[1].character.inventory.gold = 41;
    runtime.entities[1].character.conditions.count = 1;
    runtime.entities[1].character.conditions.conditions[0].type =
        CONDITION_POISONED;

    dungeon.entities = runtime.entities;
    dungeon.entityCount = runtime.entityCount;
    dungeon.rooms[roomIndex].discovered = true;
}

void setUp()
{
    generatedRoomCount = 0;
    mapEffectClearCount = 0;
    activeTestEntities = nullptr;
    activeTestEntityCount = 0;
    combat = Combat{};
    player = Character{};
    gameState = GAME_TOWN;
    resetDungeonRun(dungeon);
}

void tearDown()
{
}

void test_suspend_keeps_character_and_room_runtime_state()
{
    configureLoadedRoom(2);

    suspendDungeonRun(dungeon);

    const DungeonRoomRuntime& runtime = dungeon.roomRuntime[2];
    TEST_ASSERT_TRUE(dungeon.runActive);
    TEST_ASSERT_EQUAL_INT(7, player.health.currentHP);
    TEST_ASSERT_EQUAL_INT(2, player.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT32(41, player.inventory.gold);
    TEST_ASSERT_EQUAL_UINT8(1, player.conditions.count);
    TEST_ASSERT_EQUAL(CONDITION_POISONED,
                      player.conditions.conditions[0].type);
    TEST_ASSERT_FALSE(runtime.entities[runtime.playerSlot].active);
    TEST_ASSERT_EQUAL_INT(3,
        runtime.entities[0].character.health.currentHP);
    TEST_ASSERT_FALSE(runtime.entities[0].turn.standardActionUsed);
    TEST_ASSERT_FALSE(dungeon.rooms[2].completed);
}

void test_dead_unlooted_and_looted_state_survive_room_reload()
{
    configureLoadedRoom(1);
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[1];
    runtime.entities[0].character.state = STATE_DEAD;
    runtime.entities[0].loot.generated = true;
    runtime.entities[0].loot.gold = 9;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 1;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(runtime.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_DEAD,
                      runtime.entities[0].character.state);
    TEST_ASSERT_TRUE(runtime.entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(9, runtime.entities[0].loot.gold);

    runtime.entities[0].character.state = STATE_LOOTED;
    runtime.entities[0].active = false;
    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 1;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_FALSE(runtime.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_LOOTED,
                      runtime.entities[0].character.state);
}

void test_living_monster_hp_and_conditions_survive_room_reload()
{
    configureLoadedRoom(2);
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[2];
    runtime.entities[0].character.health.currentHP = 2;
    runtime.entities[0].character.conditions.count = 1;
    runtime.entities[0].character.conditions.conditions[0].type =
        CONDITION_BLINDED;
    runtime.entities[0].character.conditions.conditions[0].roundsRemaining = 3;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 2;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(runtime.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_ALIVE,
                      runtime.entities[0].character.state);
    TEST_ASSERT_EQUAL_INT(2,
                          runtime.entities[0].character.health.currentHP);
    TEST_ASSERT_EQUAL_UINT8(1,
                            runtime.entities[0].character.conditions.count);
    TEST_ASSERT_EQUAL(CONDITION_BLINDED,
        runtime.entities[0].character.conditions.conditions[0].type);
    TEST_ASSERT_EQUAL_INT(3,
        runtime.entities[0].character.conditions.conditions[0].roundsRemaining);
}

void test_resume_uses_existing_layout_and_does_not_regenerate()
{
    configureLoadedRoom(3);
    dungeon.rooms[0].map.tiles[4][5] = TILE_WALL;

    DungeonRoomRuntime& entrance = dungeon.roomRuntime[0];
    entrance.initialized = true;
    entrance.entityCount = 1;
    entrance.playerSlot = 0;
    entrance.entities[0] = Entity{};

    suspendDungeonRun(dungeon);
    enterDungeon();

    TEST_ASSERT_EQUAL_UINT8(0, dungeon.currentRoom);
    TEST_ASSERT_EQUAL_UINT8(0, generatedRoomCount);
    TEST_ASSERT_EQUAL(TILE_WALL, dungeon.rooms[0].map.tiles[4][5]);
    TEST_ASSERT_NOT_NULL(getPlayerEntity(
        dungeon.entities, dungeon.entityCount));
    TEST_ASSERT_FALSE(combat.active);
}

void test_new_run_generates_only_when_no_run_is_active()
{
    TEST_ASSERT_FALSE(dungeon.runActive);

    enterDungeon();

    TEST_ASSERT_TRUE(dungeon.runActive);
    TEST_ASSERT_EQUAL_UINT8(MAX_ROOMS, generatedRoomCount);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.currentRoom);
}

void test_unlooted_corpse_blocks_completion()
{
    resetDungeonRun(dungeon);
    dungeon.runActive = true;

    for (uint8_t i = 0; i < MAX_ROOMS; i++)
    {
        dungeon.rooms[i].discovered = true;
        dungeon.rooms[i].completed = true;
        dungeon.roomRuntime[i].initialized = true;
    }

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[4];
    runtime.entityCount = 1;
    runtime.entities[0] = Entity{};
    runtime.entities[0].active = true;
    runtime.entities[0].type = ENTITY_MONSTER;
    runtime.entities[0].character.state = STATE_DEAD;

    TEST_ASSERT_FALSE(isDungeonRunComplete(dungeon));

    runtime.entities[0].character.state = STATE_LOOTED;
    runtime.entities[0].active = false;
    TEST_ASSERT_TRUE(isDungeonRunComplete(dungeon));
}

void test_reset_discards_runtime_run_without_touching_player()
{
    configureLoadedRoom(1);
    player.health.currentHP = 5;
    player.magic.currentMP = 1;

    resetDungeonRun(dungeon);

    TEST_ASSERT_FALSE(dungeon.runActive);
    TEST_ASSERT_NULL(dungeon.entities);
    TEST_ASSERT_EQUAL_UINT8(NO_ROOM, dungeon.loadedRoom);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[1].initialized);
    TEST_ASSERT_EQUAL_INT(5, player.health.currentHP);
    TEST_ASSERT_EQUAL_INT(1, player.magic.currentMP);
}

void test_abort_clears_combat_only_state_and_preserves_characters()
{
    Entity entities[3] = {};
    activeTestEntities = entities;
    activeTestEntityCount = 3;

    Entity& playerEntity = entities[0];
    playerEntity.active = true;
    playerEntity.type = ENTITY_PLAYER;
    playerEntity.character.state = STATE_ALIVE;
    playerEntity.character.health.currentHP = 6;
    playerEntity.character.magic.currentMP = 2;
    playerEntity.character.xp = 1234;
    playerEntity.character.inventory.gold = 19;
    playerEntity.character.conditions.count = 2;
    playerEntity.character.conditions.conditions[0].type =
        CONDITION_FLAT_FOOTED;
    playerEntity.character.conditions.conditions[1].type =
        CONDITION_POISONED;
    playerEntity.turn.standardActionUsed = true;
    playerEntity.turn.movementRemaining = 1;
    playerEntity.turn.turnActive = true;
    playerEntity.turn.monsterState = MONSTER_ATTACK;

    Entity& monster = entities[1];
    monster.active = true;
    monster.type = ENTITY_MONSTER;
    monster.character.state = STATE_ALIVE;
    monster.character.health.currentHP = 4;
    monster.turn.fullDefense = true;

    Entity& nonParticipant = entities[2];
    nonParticipant.active = true;
    nonParticipant.type = ENTITY_CHEST;
    nonParticipant.turn.moveActionUsed = true;

    combat.active = true;
    combat.phase = COMBAT_TURN;
    combat.initiativeOrder[0] = &playerEntity;
    combat.initiativeOrder[1] = &monster;
    combat.combatantCount = 2;
    combat.currentTurnIndex = 1;
    combat.combatRound = 7;
    combat.experienceGained = 900;
    combat.waitingForPlayer = true;
    combat.selectedAbility = ABILITY_MAGIC_MISSILE;
    combat.selectedAbilityX = 4;
    combat.selectedAbilityY = 5;
    combat.abilityCaster = &playerEntity;
    combat.pendingAttackTarget = &monster;
    combat.attackingMonster = &monster;
    combat.monsterAttackTarget = &playerEntity;
    combat.attackResolutionPending = true;
    combat.inspecting = true;
    combat.inspectedEntityIndex = 1;

    abortCombat();

    TEST_ASSERT_FALSE(combat.active);
    TEST_ASSERT_EQUAL(COMBAT_NONE, combat.phase);
    TEST_ASSERT_EQUAL_UINT8(0, combat.combatantCount);
    TEST_ASSERT_EQUAL_UINT8(0, combat.currentTurnIndex);
    TEST_ASSERT_EQUAL_UINT8(0, combat.combatRound);
    TEST_ASSERT_NULL(combat.initiativeOrder[0]);
    TEST_ASSERT_NULL(combat.pendingAttackTarget);
    TEST_ASSERT_NULL(combat.attackingMonster);
    TEST_ASSERT_NULL(combat.monsterAttackTarget);
    TEST_ASSERT_NULL(combat.abilityCaster);
    TEST_ASSERT_EQUAL(ABILITY_NONE, combat.selectedAbility);
    TEST_ASSERT_FALSE(combat.attackResolutionPending);
    TEST_ASSERT_FALSE(combat.inspecting);
    TEST_ASSERT_EQUAL_UINT8(1, mapEffectClearCount);

    TEST_ASSERT_EQUAL_INT(6, playerEntity.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(2, playerEntity.character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT32(1234, playerEntity.character.xp);
    TEST_ASSERT_EQUAL_UINT32(19, playerEntity.character.inventory.gold);
    TEST_ASSERT_EQUAL(STATE_ALIVE, playerEntity.character.state);
    TEST_ASSERT_EQUAL_UINT8(1, playerEntity.character.conditions.count);
    TEST_ASSERT_EQUAL(CONDITION_POISONED,
        playerEntity.character.conditions.conditions[0].type);
    TEST_ASSERT_FALSE(playerEntity.turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(0, playerEntity.turn.movementRemaining);
    TEST_ASSERT_FALSE(playerEntity.turn.turnActive);
    TEST_ASSERT_EQUAL(MONSTER_START, playerEntity.turn.monsterState);

    TEST_ASSERT_EQUAL(STATE_ALIVE, monster.character.state);
    TEST_ASSERT_EQUAL_INT(4, monster.character.health.currentHP);
    TEST_ASSERT_FALSE(monster.turn.fullDefense);
    TEST_ASSERT_FALSE(nonParticipant.turn.moveActionUsed);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_suspend_keeps_character_and_room_runtime_state);
    RUN_TEST(test_dead_unlooted_and_looted_state_survive_room_reload);
    RUN_TEST(test_living_monster_hp_and_conditions_survive_room_reload);
    RUN_TEST(test_resume_uses_existing_layout_and_does_not_regenerate);
    RUN_TEST(test_new_run_generates_only_when_no_run_is_active);
    RUN_TEST(test_unlooted_corpse_blocks_completion);
    RUN_TEST(test_reset_discards_runtime_run_without_touching_player);
    RUN_TEST(test_abort_clears_combat_only_state_and_preserves_characters);
    UNITY_END();
}

void loop()
{
}
