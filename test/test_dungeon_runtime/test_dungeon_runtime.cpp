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

const uint16_t chestclosed[16 * 16] = {};

Entity* getActiveMapEntities(uint8_t& entityCount)
{
    entityCount = activeTestEntityCount;
    return activeTestEntities;
}

void clearMapEffects()
{
    mapEffectClearCount++;
}

void resetAwarenessTimer()
{
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

int healCharacter(Character& character, int healing)
{
    const int missing = character.health.maxHP - character.health.currentHP;
    const int restored = healing < missing ? healing : missing;
    character.health.currentHP += restored;
    return restored;
}

int restoreMana(Character& character, int amount)
{
    const int missing = character.magic.maxMP - character.magic.currentMP;
    const int restored = amount < missing ? amount : missing;
    character.magic.currentMP += restored;
    return restored;
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

void populateDungeonRoomFeatures(DungeonRoom&, uint8_t, bool)
{
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
#include "../../src/dungeon/traps.cpp"
#include "../../src/dungeon/fountain.cpp"
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
    runtime.entities[0].awareOfPlayer = true;
    runtime.entities[0].revealedToPlayer = true;

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
    TEST_ASSERT_TRUE(runtime.entities[0].awareOfPlayer);
    TEST_ASSERT_TRUE(runtime.entities[0].revealedToPlayer);
}

void test_chest_lock_and_open_state_survive_room_reload()
{
    configureLoadedRoom(2);
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[2];
    Entity& chest = runtime.entities[0];
    chest.type = ENTITY_CHEST;
    chest.locked = false;
    chest.opened = true;
    chest.loot.generated = true;
    chest.loot.gold = 7;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 2;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(runtime.entities[0].active);
    TEST_ASSERT_EQUAL(ENTITY_CHEST, runtime.entities[0].type);
    TEST_ASSERT_FALSE(runtime.entities[0].locked);
    TEST_ASSERT_TRUE(runtime.entities[0].opened);
    TEST_ASSERT_TRUE(runtime.entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(7, runtime.entities[0].loot.gold);
}

void test_trap_state_persists_across_room_reload()
{
    configureLoadedRoom(2);
    DungeonRoom& room = dungeon.rooms[2];

    TEST_ASSERT_TRUE(addTrap(
        room, TRAP_SPIKE_PLATE, 4, 5, 7,
        SUSPICION_BONES, 9));
    TEST_ASSERT_TRUE(addTrap(
        room, TRAP_SPIKE_PLATE, 6, 5, 4,
        SUSPICION_FLOOR_GROOVES));
    TEST_ASSERT_TRUE(addTrap(
        room, TRAP_SPIKE_PLATE, 8, 5, 2,
        SUSPICION_BLOODSTAIN));

    TrapInstance* disabledTrap = getTrapAt(room, 4, 5);
    TrapInstance* triggeredTrap = getTrapAt(room, 6, 5);
    TrapInstance* destroyedTrap = getTrapAt(room, 8, 5);
    TEST_ASSERT_NOT_NULL(disabledTrap);
    TEST_ASSERT_NOT_NULL(triggeredTrap);
    TEST_ASSERT_NOT_NULL(destroyedTrap);

    disabledTrap->discovered = true;
    disabledTrap->disabled = true;
    disabledTrap->manualPerceptionAttempted = true;
    disabledTrap->rogueDiscoveryAttempted = true;
    disabledTrap->hp--;

    TEST_ASSERT_TRUE(resolveTrapTrigger(
        *triggeredTrap, false, 4).triggered);
    destroyedTrap->discovered = true;
    TEST_ASSERT_TRUE(damageTrap(
        *destroyedTrap, 100, DAMAGE_PIERCING).destroyed);

    const int16_t persistedHP = disabledTrap->hp;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 2;
    loadRoom(dungeon, ENTRY_START);

    disabledTrap = getTrapAt(dungeon.rooms[2], 4, 5);
    triggeredTrap = getTrapAt(dungeon.rooms[2], 6, 5);
    destroyedTrap = getTrapAt(dungeon.rooms[2], 8, 5);

    TEST_ASSERT_NOT_NULL(disabledTrap);
    TEST_ASSERT_TRUE(disabledTrap->discovered);
    TEST_ASSERT_TRUE(disabledTrap->disabled);
    TEST_ASSERT_FALSE(disabledTrap->triggered);
    TEST_ASSERT_FALSE(disabledTrap->destroyed);
    TEST_ASSERT_TRUE(disabledTrap->manualPerceptionAttempted);
    TEST_ASSERT_TRUE(disabledTrap->rogueDiscoveryAttempted);
    TEST_ASSERT_EQUAL_INT(persistedHP, disabledTrap->hp);
    TEST_ASSERT_EQUAL_UINT8(9, disabledTrap->controlGroup);
    TEST_ASSERT_EQUAL(SUSPICION_BONES,
                      getSuspicionAt(dungeon.rooms[2], 4, 5));

    TEST_ASSERT_NOT_NULL(triggeredTrap);
    TEST_ASSERT_TRUE(triggeredTrap->triggered);
    TEST_ASSERT_FALSE(triggeredTrap->destroyed);
    TEST_ASSERT_FALSE(triggeredTrap->disabled);

    TEST_ASSERT_NOT_NULL(destroyedTrap);
    TEST_ASSERT_TRUE(destroyedTrap->destroyed);
    TEST_ASSERT_FALSE(destroyedTrap->triggered);
    TEST_ASSERT_FALSE(destroyedTrap->disabled);
    TEST_ASSERT_EQUAL_INT(0, destroyedTrap->hp);
}

void test_trap_uses_level_scaled_statistics()
{
    DungeonRoom room = {};
    TEST_ASSERT_TRUE(addTrap(
        room, TRAP_SPIKE_PLATE, 3, 4, 7));

    const TrapInstance* trap = getTrapAt(room, 3, 4);
    TEST_ASSERT_NOT_NULL(trap);
    TEST_ASSERT_EQUAL_UINT8(19, getTrapPerceptionDC(*trap));
    TEST_ASSERT_EQUAL_UINT8(21, getTrapDisableDC(*trap));
    TEST_ASSERT_EQUAL_UINT16(20, getTrapMaxHP(*trap));
    TEST_ASSERT_EQUAL_UINT16(getTrapMaxHP(*trap), trap->hp);
}

void test_resume_uses_existing_layout_and_does_not_regenerate()
{
    configureLoadedRoom(3);
    dungeon.rooms[0].map.tiles[4][5] = TILE_WALL;
    dungeon.rooms[3].map.tiles[7][8] = TILE_WALL;
    dungeon.roomRuntime[3].entities[0].character.state = STATE_DEAD;
    dungeon.roomRuntime[3].entities[0].loot.generated = true;
    dungeon.roomRuntime[3].entities[0].loot.gold = 17;

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
    TEST_ASSERT_EQUAL(TILE_WALL, dungeon.rooms[3].map.tiles[7][8]);
    TEST_ASSERT_EQUAL(STATE_DEAD,
        dungeon.roomRuntime[3].entities[0].character.state);
    TEST_ASSERT_TRUE(dungeon.roomRuntime[3].entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(17,
        dungeon.roomRuntime[3].entities[0].loot.gold);
    TEST_ASSERT_NOT_NULL(getPlayerEntity(
        dungeon.entities, dungeon.entityCount));
    TEST_ASSERT_FALSE(combat.active);
}

void test_only_unfinished_runs_are_resumable()
{
    configureLoadedRoom(1);
    TEST_ASSERT_TRUE(hasResumableDungeon(dungeon));

    dungeon.finalEncounterCleared = true;
    dungeon.finalTreasureLooted = true;
    TEST_ASSERT_TRUE(isDungeonRunComplete(dungeon));
    TEST_ASSERT_TRUE(hasResumableDungeon(dungeon));

    markDungeonCompletedOnTownReturn(dungeon);
    TEST_ASSERT_TRUE(dungeon.completed);
    TEST_ASSERT_FALSE(hasResumableDungeon(dungeon));
}

void test_new_run_generates_only_when_no_run_is_active()
{
    TEST_ASSERT_FALSE(dungeon.runActive);

    enterDungeon();

    TEST_ASSERT_TRUE(dungeon.runActive);
    TEST_ASSERT_EQUAL_UINT8(MAX_ROOMS, generatedRoomCount);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.currentRoom);
}

void test_themed_encounters_spawn_only_their_theme_monsters()
{
    static constexpr EncounterTheme themes[] = {
        ENCOUNTER_GOBLIN,
        ENCOUNTER_UNDEAD,
        ENCOUNTER_ABERRATION};

    for (EncounterTheme theme : themes)
    {
        resetDungeonRun(dungeon);
        DungeonRoom& room = dungeon.rooms[1];
        DungeonRoomRuntime& runtime = dungeon.roomRuntime[1];
        room.encounterTheme = theme;

        for (uint8_t y = 0; y < ROOM_SIZE; y++)
            for (uint8_t x = 0; x < ROOM_SIZE; x++)
                room.map.tiles[y][x] = TILE_FLOOR;

        room.map.tiles[3][3] = TILE_ENEMY_START;
        room.map.tiles[3][5] = TILE_ENEMY_START;
        dungeon.entities = runtime.entities;

        initializeRoomEntities(dungeon, room, runtime);

        TEST_ASSERT_EQUAL_UINT8(2, runtime.entityCount);
        for (uint8_t i = 0; i < runtime.entityCount; i++)
        {
            const MonsterID id = runtime.entities[i].monsterID;
            if (theme == ENCOUNTER_GOBLIN)
                TEST_ASSERT_TRUE(id == MONSTER_GOBLIN_SCIMITAR ||
                    id == MONSTER_GOBLIN_ARCHER || id == MONSTER_BUGBEAR);
            else if (theme == ENCOUNTER_UNDEAD)
                TEST_ASSERT_TRUE(id == MONSTER_SKELETON || id == MONSTER_ZOMBIE ||
                    id == MONSTER_GHOUL || id == MONSTER_WIGHT);
            else
                TEST_ASSERT_TRUE(id == MONSTER_GRAY_OOZE ||
                    id == MONSTER_VIOLET_FUNGUS || id == MONSTER_CHOKER ||
                    id == MONSTER_SPECTATOR);
        }
    }
}

void test_final_encounter_must_be_fully_defeated_before_completion()
{
    resetDungeonRun(dungeon);
    dungeon.runActive = true;
    dungeon.currentRoom = BOSS_ROOM_INDEX;
    dungeon.loadedRoom = BOSS_ROOM_INDEX;
    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[BOSS_ROOM_INDEX];
    runtime.initialized = true;
    runtime.entityCount = 3;
    dungeon.entities = runtime.entities;
    dungeon.entityCount = runtime.entityCount;

    const MonsterID monsters[] = {
        MONSTER_SKELETON_MAGE,
        MONSTER_SKELETON,
        MONSTER_SKELETON};

    for (uint8_t i = 0; i < runtime.entityCount; i++)
    {
        runtime.entities[i] = Entity{};
        runtime.entities[i].active = true;
        runtime.entities[i].type = ENTITY_MONSTER;
        runtime.entities[i].monsterID = monsters[i];
        runtime.entities[i].character.team = TEAM_MONSTER;
        runtime.entities[i].character.state = STATE_DEAD;
    }

    runtime.entities[2].character.state = STATE_ALIVE;
    updateCurrentDungeonRoomCompletion(dungeon);
    TEST_ASSERT_FALSE(isDungeonRunComplete(dungeon));

    runtime.entities[2].character.state = STATE_DEAD;
    updateCurrentDungeonRoomCompletion(dungeon);
    TEST_ASSERT_TRUE(dungeon.finalEncounterCleared);
    TEST_ASSERT_FALSE(isDungeonRunComplete(dungeon));
    TEST_ASSERT_FALSE(dungeon.completed);

    dungeon.finalTreasureLooted = true;
    TEST_ASSERT_TRUE(isDungeonRunComplete(dungeon));
    markDungeonCompletedOnTownReturn(dungeon);
    TEST_ASSERT_TRUE(dungeon.completed);
    TEST_ASSERT_FALSE(hasResumableDungeon(dungeon));
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

void test_starting_new_run_clears_old_runtime_before_generation()
{
    configureLoadedRoom(2);
    dungeon.roomRuntime[2].entities[0].character.state = STATE_DEAD;
    dungeon.roomRuntime[2].entities[0].loot.generated = true;
    dungeon.roomRuntime[2].entities[0].loot.gold = 23;

    resetDungeonRun(dungeon);

    TEST_ASSERT_FALSE(dungeon.runActive);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[2].initialized);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.roomRuntime[2].entityCount);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[2].entities[0].active);
    TEST_ASSERT_EQUAL_UINT16(0, dungeon.roomRuntime[2].entities[0].loot.gold);

    enterDungeon();

    TEST_ASSERT_TRUE(dungeon.runActive);
    TEST_ASSERT_EQUAL_UINT8(MAX_ROOMS, generatedRoomCount);
}

void test_entrance_fountain_is_one_persistent_multi_tile_healing_object()
{
    generateDungeon(dungeon);
    DungeonRoom& entrance = dungeon.rooms[0];
    HealingFountain& fountain = entrance.fountain;

    TEST_ASSERT_TRUE(fountain.active);
    TEST_ASSERT_FALSE(fountain.used);
    for (uint8_t y = 0; y < HEALING_FOUNTAIN_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < HEALING_FOUNTAIN_WIDTH; x++)
        {
            TEST_ASSERT_EQUAL(TILE_FOUNTAIN,
                entrance.map.tiles[fountain.y + y][fountain.x + x]);
            TEST_ASSERT_EQUAL_PTR(&fountain, getHealingFountainAt(
                entrance, fountain.x + x, fountain.y + y));
        }
    }

    Character character = {};
    character.health.currentHP = 2;
    character.health.maxHP = 12;
    character.magic.currentMP = 1;
    character.magic.maxMP = 7;
    TEST_ASSERT_TRUE(drinkFromHealingFountain(fountain, character));
    TEST_ASSERT_EQUAL_INT(12, character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(7, character.magic.currentMP);
    TEST_ASSERT_TRUE(fountain.used);
    TEST_ASSERT_FALSE(drinkFromHealingFountain(fountain, character));

    dungeon.currentRoom = 1;
    loadRoom(dungeon, ENTRY_WEST);
    dungeon.currentRoom = 0;
    loadRoom(dungeon, ENTRY_START);
    TEST_ASSERT_TRUE(dungeon.rooms[0].fountain.used);

    generateDungeon(dungeon);
    TEST_ASSERT_TRUE(dungeon.rooms[0].fountain.active);
    TEST_ASSERT_FALSE(dungeon.rooms[0].fountain.used);
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
    RUN_TEST(test_chest_lock_and_open_state_survive_room_reload);
    RUN_TEST(test_trap_state_persists_across_room_reload);
    RUN_TEST(test_trap_uses_level_scaled_statistics);
    RUN_TEST(test_resume_uses_existing_layout_and_does_not_regenerate);
    RUN_TEST(test_only_unfinished_runs_are_resumable);
    RUN_TEST(test_new_run_generates_only_when_no_run_is_active);
    RUN_TEST(test_themed_encounters_spawn_only_their_theme_monsters);
    RUN_TEST(test_final_encounter_must_be_fully_defeated_before_completion);
    RUN_TEST(test_reset_discards_runtime_run_without_touching_player);
    RUN_TEST(test_starting_new_run_clears_old_runtime_before_generation);
    RUN_TEST(test_entrance_fountain_is_one_persistent_multi_tile_healing_object);
    RUN_TEST(test_abort_clears_combat_only_state_and_preserves_characters);
    UNITY_END();
}

void loop()
{
}
