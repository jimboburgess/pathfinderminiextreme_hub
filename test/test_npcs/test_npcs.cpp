#include <Arduino.h>
#include <unity.h>

#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/combatpolicy.h"
#include "dungeon/dungeon.h"
#include "dungeon/npcs.h"
#include "graphics/messagelog.h"

const uint16_t bertram16x16[SPRITE_W * SPRITE_H] = {};

namespace
{
const char* testMessage = "";
bool riddleOpened = false;
}

bool openBertramRiddle(const Entity&)
{
    riddleOpened = true;
    return true;
}

TrapInstance* getTrapAt(DungeonRoom&, int, int)
{
    return nullptr;
}

const TrapInstance* getTrapAt(const DungeonRoom&, int, int)
{
    return nullptr;
}

DungeonFurnitureInstance* getDungeonFurnitureAt(DungeonRoom&, int, int)
{
    return nullptr;
}

const DungeonFurnitureInstance* getDungeonFurnitureAt(
    const DungeonRoom&, int, int)
{
    return nullptr;
}

bool isHealingFountainTile(const DungeonRoom&, int, int)
{
    return false;
}

void setGameMessage(const char* message)
{
    testMessage = message;
}

const char* getGameMessage()
{
    return testMessage;
}

void clearGameMessage()
{
    testMessage = "";
}

#include "../../src/dungeon/npcs.cpp"
#include "../../src/data/entityspawn.cpp"

namespace
{
void fillFloor(DungeonRoom& room)
{
    for (uint8_t y = 0; y < ROOM_SIZE; y++)
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
}
}

void setUp()
{
    clearGameMessage();
    riddleOpened = false;
}

void tearDown() {}

void test_bertram_definition_and_spawn_are_neutral()
{
    const NPCDefinition* definition = getNPCDefinition(
        NPC_BERTRAM_RIDDLEMAN);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_STRING(
        "Bertram, Door Enthusiast", definition->name);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, definition->team);

    Entity entities[2] = {};
    uint8_t count = 0;
    Entity* bertram = spawnNPC(
        entities, count, NPC_BERTRAM_RIDDLEMAN, 5, 6);
    TEST_ASSERT_NOT_NULL(bertram);
    TEST_ASSERT_EQUAL(ENTITY_NPC, bertram->type);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, bertram->npcID);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, bertram->character.team);
    TEST_ASSERT_EQUAL_STRING(
        "Bertram, Door Enthusiast", getEntityName(bertram));
    TEST_ASSERT_TRUE(isBlockingNeutralNPC(*bertram));
}

void test_room_placement_accepts_safe_floor_and_rejects_conflicts()
{
    DungeonRoom room{};
    fillFloor(room);
    room.map.tiles[2][2] = TILE_DOOR;

    TEST_ASSERT_FALSE(placeDungeonNPC(
        room, NPC_BERTRAM_RIDDLEMAN, 2, 2));
    TEST_ASSERT_TRUE(placeDungeonNPC(
        room, NPC_BERTRAM_RIDDLEMAN, 7, 7));
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, room.npcSpawn.id);
    TEST_ASSERT_EQUAL_INT8(7, room.npcSpawn.x);
    TEST_ASSERT_EQUAL_INT8(7, room.npcSpawn.y);
    TEST_ASSERT_FALSE(placeDungeonNPC(
        room, NPC_BERTRAM_RIDDLEMAN, 8, 7));
}

void test_bertram_is_not_a_monster_or_combatant()
{
    Entity entities[2] = {};
    uint8_t count = 0;
    Entity* player = spawnEntity(
        entities, count, ENTITY_PLAYER, 2, 2);
    player->character.team = TEAM_PLAYER;
    player->character.state = STATE_ALIVE;
    Entity* bertram = spawnNPC(
        entities, count, NPC_BERTRAM_RIDDLEMAN, 3, 2);
    Entity* roster[2] = {};

    TEST_ASSERT_FALSE(isHostileMonsterForCombat(*bertram));
    TEST_ASSERT_EQUAL_UINT8(
        1, buildCombatRoster(entities, count, player, roster, 2));
    TEST_ASSERT_EQUAL_PTR(player, roster[0]);
}

void test_bertram_interaction_routes_to_riddle_menu()
{
    Entity entities[1] = {};
    uint8_t count = 0;
    Entity* bertram = spawnNPC(
        entities, count, NPC_BERTRAM_RIDDLEMAN, 3, 2);

    TEST_ASSERT_TRUE(handleNPCInteraction(*bertram));
    TEST_ASSERT_TRUE(riddleOpened);
}

void test_runtime_entity_state_survives_room_view_changes()
{
    Dungeon dungeon{};
    DungeonRoomRuntime& room = dungeon.roomRuntime[3];
    Entity* bertram = spawnNPC(
        room.entities, room.entityCount,
        NPC_BERTRAM_RIDDLEMAN, 9, 4);
    room.initialized = true;

    dungeon.entities = dungeon.roomRuntime[1].entities;
    dungeon.entities = room.entities;
    dungeon.entityCount = room.entityCount;

    TEST_ASSERT_EQUAL_PTR(bertram, &dungeon.entities[0]);
    TEST_ASSERT_TRUE(dungeon.entities[0].active);
    TEST_ASSERT_EQUAL_UINT8(9, dungeon.entities[0].x);
    TEST_ASSERT_EQUAL_UINT8(4, dungeon.entities[0].y);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, dungeon.entities[0].npcID);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_bertram_definition_and_spawn_are_neutral);
    RUN_TEST(test_room_placement_accepts_safe_floor_and_rejects_conflicts);
    RUN_TEST(test_bertram_is_not_a_monster_or_combatant);
    RUN_TEST(test_bertram_interaction_routes_to_riddle_menu);
    RUN_TEST(test_runtime_entity_state_survives_room_view_changes);
    UNITY_END();
}

void loop() {}
