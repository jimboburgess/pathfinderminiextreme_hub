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
static uint8_t inventoryCloseCount = 0;
static uint8_t menuCloseCount = 0;
static uint8_t interactionClearCount = 0;
static Entity* activeTestEntities = nullptr;
static uint8_t activeTestEntityCount = 0;

Combat combat = {};

const uint16_t chestclosed[16 * 16] = {};
const uint16_t chestopenwith[16 * 16] = {};
const uint16_t chestopenwithout[16 * 16] = {};
const uint16_t bertramCat16x16[16 * 16] = {};
const uint16_t testBertramSprite[16 * 16] = {};

void closeInventoryMenu() { inventoryCloseCount++; }
void closeMenu() { menuCloseCount++; }
void clearInteractionEntityReferences() { interactionClearCount++; }

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

    for (uint8_t y = 0; y < ROOM_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < ROOM_WIDTH; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
    }
}

uint8_t populateRubbleTerrain(DungeonRoom& room)
{
    // Terrain generation is covered by the room-generation suite. This
    // lifecycle harness records the themed room with one walkable tile.
    room.map.tiles[5][5] = TILE_RUBBLE;
    return 1;
}

uint8_t populateBossRubbleTerrain(DungeonRoom& room)
{
    room.map.tiles[6][6] = TILE_RUBBLE;
    return 1;
}

uint8_t populatePillarTerrain(DungeonRoom&, uint8_t, uint8_t)
{
    return 0;
}

uint8_t populateDungeonFurniture(
    DungeonRoom&, uint8_t, uint8_t, uint8_t, uint8_t)
{
    // Furniture placement has focused coverage in test_furniture. The room
    // lifecycle harness only needs the generator hook to remain linkable.
    return 0;
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
    x = ROOM_WIDTH / 2;
    y = ROOM_HEIGHT / 2;
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
        initializeMonsterDefinitionState(*entity, monsterID);

    return entity;
}

bool initializeMonsterDefinitionState(Entity& entity, MonsterID monsterID)
{
    if (monsterID == MONSTER_NONE || monsterID >= MONSTER_COUNT) return false;
    entity.monsterID = monsterID;
    entity.character.team = TEAM_MONSTER;
    entity.character.state = STATE_ALIVE;
    entity.character.creatureType = CREATURE_MONSTER;
    if (monsterID == MONSTER_SKELETON_MAGE)
    {
        entity.character.level = 3;
        entity.character.magic.maxMP = 8;
        entity.character.magic.currentMP = 8;
    }
    return true;
}

const NPCDefinition* getNPCDefinition(NPCID id)
{
    static const NPCDefinition bertram = {
        NPC_BERTRAM_RIDDLEMAN, "Bertram, Door Enthusiast", TEAM_NEUTRAL,
        testBertramSprite, "Bertram watches you expectantly."};
    return id == NPC_BERTRAM_RIDDLEMAN ? &bertram : nullptr;
}

bool initializeNPCDefinitionState(Entity& entity, NPCID npcID)
{
    const NPCDefinition* definition = getNPCDefinition(npcID);
    if (definition == nullptr) return false;
    entity.npcID = npcID;
    entity.sprite = definition->sprite;
    entity.character.team = definition->team;
    entity.character.state = STATE_ALIVE;
    return true;
}

Entity* spawnNPC(
    Entity* entities,
    uint8_t& entityCount,
    NPCID npcID,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = spawnEntity(
        entities, entityCount, ENTITY_NPC, x, y);
    if (entity != nullptr)
        initializeNPCDefinitionState(*entity, npcID);
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
#include "../../src/dungeon/dungeongraph.cpp"
#include "../../src/dungeon/riddles.cpp"
#include "../../src/dungeon/entitypersistence.cpp"

bool configureRiddlemanPuzzleRoom(
    DungeonRoom& room, Direction direction, RiddleID id, const uint8_t rolls[3])
{
    room.npcSpawn.id = NPC_BERTRAM_RIDDLEMAN;
    room.npcSpawn.x = 7;
    room.npcSpawn.y = 7;
    room.npcSpawn.keyX = 8;
    room.npcSpawn.keyY = 7;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_UNSOLVED;
    room.npcSpawn.lockedExitDirection = direction;
    initializeRiddleState(room.npcSpawn.riddle, id, rolls);
    return true;
}

bool isRiddlemanPuzzleRoom(const DungeonRoom& room)
{
    return room.npcSpawn.puzzleState != RIDDLE_ROOM_NONE;
}

bool configureBellPuzzleRoom(
    DungeonRoom& room, Direction direction, uint8_t level,
    const uint8_t* rolls, uint8_t rollCount)
{
    (void)level;
    (void)rolls;
    (void)rollCount;
    room.puzzleType = PUZZLE_BELLS;
    room.bellPuzzle.progress = BELL_PUZZLE_UNSOLVED;
    room.bellPuzzle.lockedExitDirection = direction;
    room.bellPuzzle.sequenceLength = 3;
    return true;
}

bool isBellPuzzleRoom(const DungeonRoom& room)
{
    return room.puzzleType == PUZZLE_BELLS &&
        room.bellPuzzle.progress != BELL_PUZZLE_NONE;
}

bool configureNumberTilePuzzleRoom(
    DungeonRoom& room, Direction direction, uint8_t level,
    const uint8_t* rolls, uint16_t rollCount)
{
    (void)level;
    (void)rolls;
    (void)rollCount;
    room.puzzleType = PUZZLE_NUMBER_TILES;
    room.numberPuzzle.progress = NUMBER_PUZZLE_UNSOLVED;
    room.numberPuzzle.lockedExitDirection = direction;
    return true;
}

bool isNumberTilePuzzleRoom(const DungeonRoom& room)
{
    return room.puzzleType == PUZZLE_NUMBER_TILES &&
        room.numberPuzzle.progress != NUMBER_PUZZLE_NONE;
}

#include "../../src/dungeon/dungeon.cpp"

static void configureLoadedRoom(uint8_t roomIndex)
{
    resetDungeonRun(dungeon);
    dungeon.runActive = true;
    dungeon.roomCount = MIN_DUNGEON_ROOMS;
    dungeon.bossRoom = MIN_DUNGEON_ROOMS - 2;
    dungeon.treasureRoom = MIN_DUNGEON_ROOMS - 1;
    dungeon.currentRoom = roomIndex;
    dungeon.loadedRoom = roomIndex;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];
    runtime.initialized = true;
    Entity* entities = dungeon.activeDungeonEntities;
    entities[0] = Entity{};
    entities[0].active = true;
    entities[0].type = ENTITY_MONSTER;
    initializeMonsterDefinitionState(
        entities[0], MONSTER_GOBLIN_SCIMITAR);
    entities[0].x = 3;
    entities[0].y = 3;
    entities[0].character.health.currentHP = 3;
    entities[0].character.health.maxHP = 8;
    entities[0].turn.standardActionUsed = true;

    entities[1] = Entity{};
    entities[1].active = true;
    entities[1].type = ENTITY_PLAYER;
    entities[1].x = ROOM_WIDTH / 2;
    entities[1].y = ROOM_HEIGHT / 2;
    entities[1].character.state = STATE_ALIVE;
    entities[1].character.health.currentHP = 7;
    entities[1].character.health.maxHP = 12;
    entities[1].character.magic.currentMP = 2;
    entities[1].character.magic.maxMP = 6;
    entities[1].character.inventory.gold = 41;
    entities[1].character.conditions.count = 1;
    entities[1].character.conditions.conditions[0].type =
        CONDITION_POISONED;

    dungeon.entities = dungeon.activeDungeonEntities;
    dungeon.entityCount = 2;
    dungeon.rooms[roomIndex].discovered = true;
}

void setUp()
{
    generatedRoomCount = 0;
    mapEffectClearCount = 0;
    inventoryCloseCount = 0;
    menuCloseCount = 0;
    interactionClearCount = 0;
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
    TEST_ASSERT_NULL(dungeon.entities);
    TEST_ASSERT_TRUE(runtime.persistenceReady);
    TEST_ASSERT_EQUAL_UINT8(1, runtime.persistentEntityCount);
    TEST_ASSERT_EQUAL_INT(3,
        runtime.entityStorage.compact.persistentEntities[0]
            .payload.monster.currentHP);
    TEST_ASSERT_FALSE(dungeon.rooms[2].completed);
}

void test_dead_unlooted_and_looted_state_survive_room_reload()
{
    configureLoadedRoom(1);
    dungeon.entities[0].character.state = STATE_DEAD;
    dungeon.entities[0].loot.generated = true;
    dungeon.entities[0].loot.gold = 9;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 1;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_DEAD,
                      dungeon.entities[0].character.state);
    TEST_ASSERT_TRUE(dungeon.entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(9, dungeon.entities[0].loot.gold);

    dungeon.entities[0].character.state = STATE_LOOTED;
    dungeon.entities[0].active = false;
    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 1;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_FALSE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_LOOTED,
                      dungeon.entities[0].character.state);
}

void test_living_monster_hp_and_conditions_survive_room_reload()
{
    configureLoadedRoom(2);
    dungeon.entities[0].character.health.currentHP = 2;
    dungeon.entities[0].character.conditions.count = 1;
    dungeon.entities[0].character.conditions.conditions[0].type =
        CONDITION_BLINDED;
    dungeon.entities[0].character.conditions.conditions[0].roundsRemaining = 3;
    dungeon.entities[0].awareOfPlayer = true;
    dungeon.entities[0].revealedToPlayer = true;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 2;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL(STATE_ALIVE,
                      dungeon.entities[0].character.state);
    TEST_ASSERT_EQUAL_INT(2,
                          dungeon.entities[0].character.health.currentHP);
    TEST_ASSERT_EQUAL_UINT8(1,
                            dungeon.entities[0].character.conditions.count);
    TEST_ASSERT_EQUAL(CONDITION_BLINDED,
        dungeon.entities[0].character.conditions.conditions[0].type);
    TEST_ASSERT_EQUAL_INT(3,
        dungeon.entities[0].character.conditions.conditions[0].roundsRemaining);
    TEST_ASSERT_TRUE(dungeon.entities[0].awareOfPlayer);
    TEST_ASSERT_TRUE(dungeon.entities[0].revealedToPlayer);
}

void test_chest_lock_and_open_state_survive_room_reload()
{
    configureLoadedRoom(2);
    Entity& chest = dungeon.entities[0];
    chest = Entity{};
    chest.type = ENTITY_CHEST;
    chest.active = true;
    chest.x = 3;
    chest.y = 3;
    chest.locked = false;
    chest.opened = true;
    chest.loot.generated = true;
    chest.loot.gold = 7;
    chest.loot.itemCount = 1;
    chest.loot.slots[0].item = makeItemInstance(ITEM_MANA_POTION);
    chest.loot.slots[0].quantity = 2;

    suspendDungeonRun(dungeon);
    dungeon.currentRoom = 2;
    loadRoom(dungeon, ENTRY_START);

    TEST_ASSERT_TRUE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL(ENTITY_CHEST, dungeon.entities[0].type);
    TEST_ASSERT_FALSE(dungeon.entities[0].locked);
    TEST_ASSERT_TRUE(dungeon.entities[0].opened);
    TEST_ASSERT_TRUE(dungeon.entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(7, dungeon.entities[0].loot.gold);
    TEST_ASSERT_EQUAL_UINT8(1, dungeon.entities[0].loot.itemCount);
    TEST_ASSERT_EQUAL(ITEM_MANA_POTION,
        dungeon.entities[0].loot.slots[0].item.itemID);
    TEST_ASSERT_EQUAL_UINT8(2, dungeon.entities[0].loot.slots[0].quantity);
}

void test_damaged_caster_mp_and_condition_survive_room_reload()
{
    configureLoadedRoom(2);
    Entity& caster = dungeon.entities[0];
    caster = Entity{};
    caster.type = ENTITY_MONSTER;
    caster.active = true;
    caster.x = 5;
    caster.y = 5;
    TEST_ASSERT_TRUE(initializeMonsterDefinitionState(
        caster, MONSTER_SKELETON_MAGE));
    caster.character.health.currentHP = 6;
    caster.character.health.maxHP = 19;
    caster.character.magic.currentMP = 2;
    caster.character.conditions.count = 1;
    caster.character.conditions.conditions[0].type = CONDITION_STUNNED;
    caster.character.conditions.conditions[0].roundsRemaining = 2;

    TEST_ASSERT_TRUE(suspendDungeonRun(dungeon));
    TEST_ASSERT_TRUE(loadRoom(dungeon, ENTRY_START));
    const Entity& restored = dungeon.entities[0];
    TEST_ASSERT_EQUAL(MONSTER_SKELETON_MAGE, restored.monsterID);
    TEST_ASSERT_EQUAL_INT(6, restored.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(19, restored.character.health.maxHP);
    TEST_ASSERT_EQUAL_INT(2, restored.character.magic.currentMP);
    TEST_ASSERT_EQUAL_INT(8, restored.character.magic.maxMP);
    TEST_ASSERT_EQUAL(CONDITION_STUNNED,
        restored.character.conditions.conditions[0].type);
    TEST_ASSERT_EQUAL_INT(2,
        restored.character.conditions.conditions[0].roundsRemaining);
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
    dungeon.hasRubbleTheme = true;
    dungeon.rooms[0].dungeonX = 0;
    dungeon.rooms[0].dungeonY = 0;
    dungeon.rooms[0].east = 3;
    dungeon.rooms[3].dungeonX = 1;
    dungeon.rooms[3].dungeonY = 0;
    dungeon.rooms[3].west = 0;
    dungeon.rooms[0].map.tiles[4][5] = TILE_WALL;
    dungeon.rooms[3].map.tiles[7][8] = TILE_WALL;
    dungeon.entities[0].character.state = STATE_DEAD;
    dungeon.entities[0].loot.generated = true;
    dungeon.entities[0].loot.gold = 17;

    suspendDungeonRun(dungeon);
    enterDungeon();

    TEST_ASSERT_EQUAL_UINT8(3, dungeon.currentRoom);
    TEST_ASSERT_EQUAL_UINT8(0, generatedRoomCount);
    TEST_ASSERT_TRUE(dungeon.hasRubbleTheme);
    TEST_ASSERT_EQUAL_UINT8(3, dungeon.rooms[0].east);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.rooms[3].west);
    TEST_ASSERT_EQUAL_INT8(1, dungeon.rooms[3].dungeonX);
    TEST_ASSERT_EQUAL(TILE_WALL, dungeon.rooms[0].map.tiles[4][5]);
    TEST_ASSERT_EQUAL(TILE_WALL, dungeon.rooms[3].map.tiles[7][8]);
    TEST_ASSERT_EQUAL(STATE_DEAD,
        dungeon.entities[0].character.state);
    TEST_ASSERT_TRUE(dungeon.entities[0].loot.generated);
    TEST_ASSERT_EQUAL_UINT16(17,
        dungeon.entities[0].loot.gold);
    TEST_ASSERT_NOT_NULL(getPlayerEntity(
        dungeon.entities, dungeon.entityCount));
    TEST_ASSERT_FALSE(combat.active);
}

void test_neutral_npc_spawn_persists_when_room_is_resumed()
{
    resetDungeonRun(dungeon);
    dungeon.runActive = true;
    dungeon.roomCount = MIN_DUNGEON_ROOMS;
    dungeon.currentRoom = 2;
    DungeonRoom& room = dungeon.rooms[2];
    for (uint8_t y = 0; y < ROOM_HEIGHT; y++)
        for (uint8_t x = 0; x < ROOM_WIDTH; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
    room.npcSpawn.id = NPC_BERTRAM_RIDDLEMAN;
    room.npcSpawn.x = 9;
    room.npcSpawn.y = 4;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_KEY_COLLECTED;
    room.npcSpawn.lockedExitDirection = DIR_EAST;

    loadRoom(dungeon, ENTRY_START);
    Entity* bertram = getEntityAt(
        dungeon.entities, dungeon.entityCount, 9, 4);
    TEST_ASSERT_NOT_NULL(bertram);
    TEST_ASSERT_EQUAL(ENTITY_NPC, bertram->type);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, bertram->npcID);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, bertram->character.team);
    TEST_ASSERT_NOT_EQUAL(RIDDLE_NONE, room.npcSpawn.riddle.id);
    TEST_ASSERT_TRUE(isValidRiddleAnswerOrder(room.npcSpawn.riddle));
    const RiddleID assignedRiddle = room.npcSpawn.riddle.id;
    room.npcSpawn.riddle.result = RIDDLE_ANSWERED_INCORRECT;

    suspendDungeonRun(dungeon);
    loadRoom(dungeon, ENTRY_START);
    Entity* resumed = getEntityAt(
        dungeon.entities, dungeon.entityCount, 9, 4);
    TEST_ASSERT_EQUAL_PTR(bertram, resumed);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, resumed->npcID);
    TEST_ASSERT_EQUAL(assignedRiddle, room.npcSpawn.riddle.id);
    TEST_ASSERT_EQUAL(RIDDLE_ANSWERED_INCORRECT, room.npcSpawn.riddle.result);
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_KEY_COLLECTED, room.npcSpawn.puzzleState);
    TEST_ASSERT_EQUAL(DIR_EAST, room.npcSpawn.lockedExitDirection);
}

void test_rubble_plan_is_dungeon_scoped_and_selects_some_middle_rooms()
{
    const DungeonRubblePlan clean = createDungeonRubblePlan(
        DUNGEON_RUBBLE_THEME_CHANCE_PERCENT, 0, 0, 0);
    TEST_ASSERT_FALSE(clean.enabled);
    for (bool selected : clean.middleRooms)
        TEST_ASSERT_FALSE(selected);

    const DungeonRubblePlan oneRoom = createDungeonRubblePlan(
        DUNGEON_RUBBLE_THEME_CHANCE_PERCENT - 1, 1, 50, 0);
    TEST_ASSERT_TRUE(oneRoom.enabled);
    TEST_ASSERT_FALSE(oneRoom.middleRooms[0]);
    TEST_ASSERT_TRUE(oneRoom.middleRooms[1]);
    TEST_ASSERT_FALSE(oneRoom.middleRooms[2]);

    const DungeonRubblePlan twoRooms = createDungeonRubblePlan(0, 0, 0, 0);
    TEST_ASSERT_TRUE(twoRooms.enabled);
    uint8_t selectedCount = 0;
    for (bool selected : twoRooms.middleRooms)
        selectedCount += selected ? 1 : 0;
    TEST_ASSERT_EQUAL_UINT8(2, selectedCount);
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
    TEST_ASSERT_TRUE(dungeon.roomCount >= MIN_DUNGEON_ROOMS);
    TEST_ASSERT_TRUE(dungeon.roomCount <= MAX_DUNGEON_ROOMS);
    TEST_ASSERT_EQUAL_UINT8(dungeon.roomCount, generatedRoomCount);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.currentRoom);
    TEST_ASSERT_TRUE(dungeon.riddleRoom < dungeon.roomCount);
    const DungeonRoom& riddleRoom = dungeon.rooms[dungeon.riddleRoom];
    TEST_ASSERT_EQUAL(ROOM_PUZZLE, riddleRoom.type);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, riddleRoom.npcSpawn.id);
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_UNSOLVED, riddleRoom.npcSpawn.puzzleState);
    TEST_ASSERT_TRUE(isValidRiddleAnswerOrder(riddleRoom.npcSpawn.riddle));
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

        for (uint8_t y = 0; y < ROOM_HEIGHT; y++)
            for (uint8_t x = 0; x < ROOM_WIDTH; x++)
                room.map.tiles[y][x] = TILE_FLOOR;

        room.map.tiles[3][3] = TILE_ENEMY_START;
        room.map.tiles[3][5] = TILE_ENEMY_START;
        dungeon.entities = dungeon.activeDungeonEntities;

        initializeRoomEntities(dungeon, room, runtime);

        TEST_ASSERT_EQUAL_UINT8(2, dungeon.entityCount);
        for (uint8_t i = 0; i < dungeon.entityCount; i++)
        {
            const MonsterID id = dungeon.entities[i].monsterID;
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
    dungeon.roomCount = MIN_DUNGEON_ROOMS;
    dungeon.bossRoom = MIN_DUNGEON_ROOMS - 2;
    dungeon.treasureRoom = MIN_DUNGEON_ROOMS - 1;
    dungeon.currentRoom = dungeon.bossRoom;
    dungeon.loadedRoom = dungeon.bossRoom;
    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.bossRoom];
    runtime.initialized = true;
    dungeon.entities = dungeon.activeDungeonEntities;
    dungeon.entityCount = 3;

    const MonsterID monsters[] = {
        MONSTER_SKELETON_MAGE,
        MONSTER_SKELETON,
        MONSTER_SKELETON};

    for (uint8_t i = 0; i < dungeon.entityCount; i++)
    {
        dungeon.entities[i] = Entity{};
        dungeon.entities[i].active = true;
        dungeon.entities[i].type = ENTITY_MONSTER;
        initializeMonsterDefinitionState(dungeon.entities[i], monsters[i]);
        dungeon.entities[i].character.state = STATE_DEAD;
    }

    dungeon.entities[2].character.state = STATE_ALIVE;
    updateCurrentDungeonRoomCompletion(dungeon);
    TEST_ASSERT_FALSE(isDungeonRunComplete(dungeon));

    // Legacy persisted Turn Undead state is also a defeated monster and must
    // not keep the boss encounter or room open.
    dungeon.entities[2].character.state = STATE_TURNED;
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
    dungeon.entities[0].character.state = STATE_DEAD;
    dungeon.entities[0].loot.generated = true;
    dungeon.entities[0].loot.gold = 23;

    resetDungeonRun(dungeon);

    TEST_ASSERT_FALSE(dungeon.runActive);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[2].initialized);
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.roomRuntime[2].persistentEntityCount);
    TEST_ASSERT_FALSE(dungeon.roomRuntime[2].persistenceReady);
    TEST_ASSERT_FALSE(dungeon.activeDungeonEntities[0].active);

    enterDungeon();

    TEST_ASSERT_TRUE(dungeon.runActive);
    TEST_ASSERT_TRUE(generatedRoomCount >= MIN_DUNGEON_ROOMS);
    TEST_ASSERT_TRUE(generatedRoomCount <= MAX_DUNGEON_ROOMS);
    TEST_ASSERT_EQUAL_UINT8(dungeon.roomCount, generatedRoomCount);
}

void test_shared_buffer_round_trip_between_two_rooms_preserves_monster_state()
{
    configureLoadedRoom(0);
    Entity& monster = dungeon.entities[0];
    monster.x = 4;
    monster.y = 5;
    monster.character.health.currentHP = 2;
    monster.character.health.maxHP = 11;
    monster.character.magic.currentMP = 1;
    monster.character.inventory.itemCount = 1;
    monster.character.inventory.slots[0].item =
        makeItemInstance(ITEM_MANA_POTION);
    monster.character.inventory.slots[0].quantity = 2;
    monster.loot.generated = true;
    monster.loot.gold = 13;
    monster.awareOfPlayer = true;
    monster.revealedToPlayer = true;
    monster.hasLastKnownPosition = true;
    monster.lastKnownX = 8;
    monster.lastKnownY = 9;
    monster.idleDirection = DIR_SOUTH;
    monster.idleStepsRemaining = 4;
    monster.nextIdleActionTime = 9876;

    DungeonRoom& secondRoom = dungeon.rooms[1];
    for (uint8_t y = 0; y < ROOM_HEIGHT; ++y)
        for (uint8_t x = 0; x < ROOM_WIDTH; ++x)
            secondRoom.map.tiles[y][x] = TILE_FLOOR;
    dungeon.currentRoom = 1;
    TEST_ASSERT_TRUE(loadRoom(dungeon, ENTRY_START));
    TEST_ASSERT_EQUAL_PTR(dungeon.activeDungeonEntities, dungeon.entities);

    dungeon.currentRoom = 0;
    TEST_ASSERT_TRUE(loadRoom(dungeon, ENTRY_START));
    const Entity& restored = dungeon.entities[0];
    TEST_ASSERT_EQUAL_UINT8(4, restored.x);
    TEST_ASSERT_EQUAL_UINT8(5, restored.y);
    TEST_ASSERT_EQUAL_INT(2, restored.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(11, restored.character.health.maxHP);
    TEST_ASSERT_EQUAL_INT(1, restored.character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(1, restored.character.inventory.itemCount);
    TEST_ASSERT_EQUAL(ITEM_MANA_POTION,
        restored.character.inventory.slots[0].item.itemID);
    TEST_ASSERT_EQUAL_UINT8(2,
        restored.character.inventory.slots[0].quantity);
    TEST_ASSERT_EQUAL_UINT16(13, restored.loot.gold);
    TEST_ASSERT_TRUE(restored.awareOfPlayer);
    TEST_ASSERT_TRUE(restored.revealedToPlayer);
    TEST_ASSERT_TRUE(restored.hasLastKnownPosition);
    TEST_ASSERT_EQUAL_UINT8(8, restored.lastKnownX);
    TEST_ASSERT_EQUAL_UINT8(9, restored.lastKnownY);
    TEST_ASSERT_EQUAL(DIR_SOUTH, restored.idleDirection);
    TEST_ASSERT_EQUAL_UINT8(4, restored.idleStepsRemaining);
    TEST_ASSERT_EQUAL_UINT32(9876, restored.nextIdleActionTime);
}

void test_transactional_pack_failure_preserves_room_and_active_buffer()
{
    configureLoadedRoom(2);
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[2];
    PersistentEntity& previous =
        runtime.entityStorage.compact.persistentEntities[0];
    Entity previousChest{};
    previousChest.type = ENTITY_CHEST;
    previousChest.active = true;
    previousChest.x = 1;
    previousChest.y = 2;
    previousChest.loot.gold = 77;
    TEST_ASSERT_TRUE(packPersistentEntity(previousChest, previous));
    runtime.persistentEntityCount = 1;
    runtime.persistenceReady = true;

    Entity& unsupported = dungeon.entities[0];
    unsupported.character.inventory.itemCount =
        MAX_PERSISTENT_MONSTER_ITEMS + 1;
    for (uint8_t i = 0; i < unsupported.character.inventory.itemCount; ++i)
    {
        unsupported.character.inventory.slots[i].item =
            makeItemInstance(ITEM_MANA_POTION);
        unsupported.character.inventory.slots[i].quantity = 1;
    }
    const int activeHP = unsupported.character.health.currentHP;
    player.health.currentHP = 99;
    combat.active = true;
    combat.pendingAttackTarget = &unsupported;

    TEST_ASSERT_FALSE(persistActiveDungeonRoom(dungeon));
    TEST_ASSERT_EQUAL_PTR(dungeon.activeDungeonEntities, dungeon.entities);
    TEST_ASSERT_EQUAL_UINT8(2, dungeon.entityCount);
    TEST_ASSERT_TRUE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL_INT(activeHP,
        dungeon.entities[0].character.health.currentHP);
    TEST_ASSERT_TRUE(combat.active);
    TEST_ASSERT_EQUAL_PTR(&dungeon.entities[0], combat.pendingAttackTarget);
    TEST_ASSERT_EQUAL_INT(99, player.health.currentHP);
    TEST_ASSERT_EQUAL_UINT8(1, runtime.persistentEntityCount);
    TEST_ASSERT_EQUAL(ENTITY_CHEST, previous.type);
    TEST_ASSERT_EQUAL_UINT16(77, previous.payload.chest.loot.gold);
    TEST_ASSERT_EQUAL_UINT8(0, inventoryCloseCount);
    TEST_ASSERT_EQUAL_UINT8(0, menuCloseCount);
    TEST_ASSERT_EQUAL_UINT8(0, interactionClearCount);
}

void test_successful_pack_clears_active_entity_pointer_holders()
{
    configureLoadedRoom(2);
    activeTestEntities = dungeon.entities;
    activeTestEntityCount = dungeon.entityCount;
    combat.active = true;
    combat.combatantCount = 2;
    combat.initiativeOrder[0] = &dungeon.entities[0];
    combat.initiativeOrder[1] = &dungeon.entities[1];
    combat.pendingAttackTarget = &dungeon.entities[0];
    combat.abilityCaster = &dungeon.entities[1];

    TEST_ASSERT_TRUE(suspendDungeonRun(dungeon));
    TEST_ASSERT_FALSE(combat.active);
    TEST_ASSERT_NULL(combat.initiativeOrder[0]);
    TEST_ASSERT_NULL(combat.pendingAttackTarget);
    TEST_ASSERT_NULL(combat.abilityCaster);
    TEST_ASSERT_EQUAL_UINT8(1, inventoryCloseCount);
    TEST_ASSERT_EQUAL_UINT8(1, menuCloseCount);
    TEST_ASSERT_EQUAL_UINT8(1, interactionClearCount);
    TEST_ASSERT_NULL(dungeon.entities);
}

void test_puzzle_npc_cat_and_key_use_compact_room_persistence()
{
    configureLoadedRoom(2);
    DungeonRoom& room = dungeon.rooms[2];
    room.puzzleType = PUZZLE_RIDDLEMAN;
    room.npcSpawn.id = NPC_BERTRAM_RIDDLEMAN;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_KEY_PRESENTED;
    room.npcSpawn.riddle.id = RIDDLE_MOUNTAIN;
    const RiddleRoomState originalPuzzleState = room.npcSpawn.puzzleState;

    Entity savedPlayer = dungeon.entities[1];
    Entity& bertram = dungeon.entities[0];
    bertram = Entity{};
    bertram.type = ENTITY_NPC;
    bertram.active = true;
    bertram.x = 4;
    bertram.y = 4;
    TEST_ASSERT_TRUE(initializeNPCDefinitionState(
        bertram, NPC_BERTRAM_RIDDLEMAN));

    Entity& cat = dungeon.entities[1];
    cat = Entity{};
    cat.type = ENTITY_RIDDLE_CAT;
    cat.active = true;
    cat.x = 6;
    cat.y = 5;
    cat.character.team = TEAM_NEUTRAL;
    cat.character.state = STATE_ALIVE;

    Entity& key = dungeon.entities[2];
    key = Entity{};
    key.type = ENTITY_PUZZLE_KEY;
    key.active = false;
    key.x = 5;
    key.y = 4;
    dungeon.entities[3] = savedPlayer;
    dungeon.entityCount = 4;

    TEST_ASSERT_TRUE(suspendDungeonRun(dungeon));
    const DungeonRoomRuntime& stored = dungeon.roomRuntime[2];
    TEST_ASSERT_EQUAL_UINT8(3, stored.persistentEntityCount);
    TEST_ASSERT_TRUE(loadRoom(dungeon, ENTRY_START));
    TEST_ASSERT_EQUAL(originalPuzzleState, room.npcSpawn.puzzleState);
    TEST_ASSERT_EQUAL(RIDDLE_MOUNTAIN, room.npcSpawn.riddle.id);
    TEST_ASSERT_EQUAL(ENTITY_NPC, dungeon.entities[0].type);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, dungeon.entities[0].npcID);
    TEST_ASSERT_EQUAL(ENTITY_RIDDLE_CAT, dungeon.entities[1].type);
    TEST_ASSERT_TRUE(dungeon.entities[1].active);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, dungeon.entities[1].character.team);
    TEST_ASSERT_EQUAL(ENTITY_PUZZLE_KEY, dungeon.entities[2].type);
    TEST_ASSERT_FALSE(dungeon.entities[2].active);
}

void test_entrance_fountain_is_one_persistent_multi_tile_healing_object()
{
    generateDungeon(dungeon);
    DungeonRoom& entrance = dungeon.rooms[0];
    HealingFountain& fountain = entrance.fountain;

    TEST_ASSERT_TRUE(fountain.active);
    TEST_ASSERT_FALSE(fountain.used);
    TEST_ASSERT_EQUAL_INT8(ENTRANCE_FOUNTAIN_X, fountain.x);
    TEST_ASSERT_EQUAL_INT8(ENTRANCE_FOUNTAIN_Y, fountain.y);
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
    RUN_TEST(test_damaged_caster_mp_and_condition_survive_room_reload);
    RUN_TEST(test_trap_state_persists_across_room_reload);
    RUN_TEST(test_trap_uses_level_scaled_statistics);
    RUN_TEST(test_resume_uses_existing_layout_and_does_not_regenerate);
    RUN_TEST(test_neutral_npc_spawn_persists_when_room_is_resumed);
    RUN_TEST(test_rubble_plan_is_dungeon_scoped_and_selects_some_middle_rooms);
    RUN_TEST(test_only_unfinished_runs_are_resumable);
    RUN_TEST(test_new_run_generates_only_when_no_run_is_active);
    RUN_TEST(test_themed_encounters_spawn_only_their_theme_monsters);
    RUN_TEST(test_final_encounter_must_be_fully_defeated_before_completion);
    RUN_TEST(test_reset_discards_runtime_run_without_touching_player);
    RUN_TEST(test_starting_new_run_clears_old_runtime_before_generation);
    RUN_TEST(test_shared_buffer_round_trip_between_two_rooms_preserves_monster_state);
    RUN_TEST(test_transactional_pack_failure_preserves_room_and_active_buffer);
    RUN_TEST(test_successful_pack_clears_active_entity_pointer_holders);
    RUN_TEST(test_puzzle_npc_cat_and_key_use_compact_room_persistence);
    RUN_TEST(test_entrance_fountain_is_one_persistent_multi_tile_healing_object);
    RUN_TEST(test_abort_clears_combat_only_state_and_preserves_characters);
    UNITY_END();
}

void loop()
{
}
