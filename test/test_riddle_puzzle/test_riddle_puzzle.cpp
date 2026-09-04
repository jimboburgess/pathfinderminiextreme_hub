#include <Arduino.h>
#include <unity.h>

#include "dungeon/dungeon.h"
#include "dungeon/riddlepuzzle.h"

Dungeon dungeon;
namespace { const char* messageText = ""; }
extern const uint16_t bertramCat16x16[16 * 16] = {};

const RoomConnection* getRoomConnection(const DungeonRoom& room, Direction direction)
{
    for (uint8_t i = 0; i < room.connectionCount; ++i)
        if (room.connections[i].direction == direction) return &room.connections[i];
    return nullptr;
}

bool placeDungeonNPC(DungeonRoom& room, NPCID id, int x, int y)
{
    if (room.map.tiles[y][x] != TILE_FLOOR || room.npcSpawn.id != NPC_NONE) return false;
    room.npcSpawn.id = id;
    room.npcSpawn.x = x;
    room.npcSpawn.y = y;
    return true;
}

const TrapInstance* getTrapAt(const DungeonRoom&, int, int) { return nullptr; }
const DungeonFurnitureInstance* getDungeonFurnitureAt(const DungeonRoom&, int, int) { return nullptr; }
bool isHealingFountainTile(const DungeonRoom&, int, int) { return false; }
void markTileDirty(int, int) {}
void setGameMessage(const char* message) { messageText = message; }

Entity* getEntityAt(Entity entities[], uint8_t count, uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < count; ++i)
        if (entities[i].active && entities[i].x == x && entities[i].y == y) return &entities[i];
    return nullptr;
}

Entity* getPlayerEntity(Entity entities[], uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
        if (entities[i].active && entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    return nullptr;
}

bool isBertramRiddleman(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_NPC &&
        entity.npcID == NPC_BERTRAM_RIDDLEMAN;
}

Entity* spawnEntity(Entity entities[], uint8_t& count, EntityType type, uint8_t x, uint8_t y)
{
    if (count >= MAX_ENTITIES) return nullptr;
    Entity* entity = &entities[count++];
    *entity = Entity{};
    entity->active = true;
    entity->type = type;
    entity->x = x;
    entity->y = y;
    return entity;
}

#include "../../src/dungeon/riddles.cpp"
#include "../../src/dungeon/riddlepuzzle.cpp"

namespace
{
void prepareRoom(DungeonRoom& room)
{
    room = DungeonRoom{};
    room.type = ROOM_PUZZLE;
    for (uint8_t y = 0; y < ROOM_SIZE; ++y)
        for (uint8_t x = 0; x < ROOM_SIZE; ++x)
            room.map.tiles[y][x] = (x == 0 || y == 0 || x == ROOM_SIZE - 1 || y == ROOM_SIZE - 1)
                ? TILE_WALL : TILE_FLOOR;
    room.map.tiles[7][ROOM_SIZE - 1] = TILE_DOOR;
    room.connections[0] = {DIR_EAST, ROOM_SIZE - 1, 7};
    room.connectionCount = 1;
}
}

void setUp()
{
    dungeon = Dungeon{};
    dungeon.roomCount = 1;
    dungeon.currentRoom = 0;
    dungeon.entities = dungeon.roomRuntime[0].entities;
    messageText = "";
}
void tearDown() {}

void test_room_configuration_assigns_one_persistent_gate()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 0};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_MOUNTAIN, rolls));
    TEST_ASSERT_TRUE(isRiddlemanPuzzleRoom(room));
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, room.npcSpawn.id);
    TEST_ASSERT_EQUAL(RIDDLE_MOUNTAIN, room.npcSpawn.riddle.id);
    TEST_ASSERT_TRUE(isValidRiddleAnswerOrder(room.npcSpawn.riddle));
    TEST_ASSERT_TRUE(isRiddlemanExitLocked(room, DIR_EAST));
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[room.npcSpawn.keyY][room.npcSpawn.keyX]);
}

void test_correct_answer_key_collection_and_unlock_are_single_use()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 0};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_FIRE, rolls));
    spawnEntity(dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    spawnEntity(dungeon.entities, dungeon.entityCount, ENTITY_NPC,
                room.npcSpawn.x, room.npcSpawn.y);

    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(true));
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_KEY_PRESENTED, room.npcSpawn.puzzleState);
    const uint8_t countAfterReward = dungeon.entityCount;
    TEST_ASSERT_FALSE(handleCurrentBertramRiddleResult(true));
    TEST_ASSERT_EQUAL_UINT8(countAfterReward, dungeon.entityCount);

    Entity* key = getEntityAt(dungeon.entities, dungeon.entityCount,
                              room.npcSpawn.keyX, room.npcSpawn.keyY);
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_TRUE(collectCurrentRiddleKey(*key));
    TEST_ASSERT_FALSE(key->active);
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_KEY_COLLECTED, room.npcSpawn.puzzleState);
    TEST_ASSERT_TRUE(tryUnlockCurrentRiddleExit(DIR_EAST));
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_COMPLETE, room.npcSpawn.puzzleState);
    TEST_ASSERT_TRUE(room.completed);
    TEST_ASSERT_FALSE(isRiddlemanExitLocked(room, DIR_EAST));
}

void test_incorrect_answer_allows_retry_without_reward()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {2, 1, 3};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_RIVER, rolls));
    room.npcSpawn.riddle.result = RIDDLE_ANSWERED_INCORRECT;
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_UNSOLVED, room.npcSpawn.puzzleState);
    TEST_ASSERT_EQUAL(RIDDLE_UNANSWERED, room.npcSpawn.riddle.result);
    TEST_ASSERT_EQUAL_UINT8(1, room.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_TRUE(isRiddleRetryRequired(room));
    TEST_ASSERT_EQUAL_UINT8(0, dungeon.entityCount);
    TEST_ASSERT_FALSE(tryUnlockCurrentRiddleExit(DIR_EAST));
    TEST_ASSERT_EQUAL_STRING("The exit is locked.", messageText);
}

void test_retry_costs_scale_by_tier_and_level()
{
    TEST_ASSERT_EQUAL_UINT16(50, getRiddleRetryCost(1, 1));
    TEST_ASSERT_EQUAL_UINT16(250, getRiddleRetryCost(2, 1));
    TEST_ASSERT_EQUAL_UINT16(1000, getRiddleRetryCost(3, 1));
    TEST_ASSERT_EQUAL_UINT16(60, getRiddleRetryCost(1, 5));
    TEST_ASSERT_EQUAL_UINT16(300, getRiddleRetryCost(2, 5));
    TEST_ASSERT_EQUAL_UINT16(1200, getRiddleRetryCost(3, 5));
    TEST_ASSERT_EQUAL_UINT16(0, getRiddleRetryCost(0, 5));
    TEST_ASSERT_EQUAL_UINT16(0, getRiddleRetryCost(4, 5));
}

void test_paid_retry_requires_gold_and_grants_exactly_one_guess()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_ECHO, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    player->character.level = 5;
    player->character.inventory.gold = 59;
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));

    TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_INSUFFICIENT_GOLD,
        payForCurrentRiddleRetry(*player));
    TEST_ASSERT_EQUAL_UINT32(59, player->character.inventory.gold);
    TEST_ASSERT_TRUE(isRiddleRetryRequired(room));

    player->character.inventory.gold = 100;
    TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_GRANTED,
        payForCurrentRiddleRetry(*player));
    TEST_ASSERT_EQUAL_UINT32(40, player->character.inventory.gold);
    TEST_ASSERT_FALSE(isRiddleRetryRequired(room));
    TEST_ASSERT_EQUAL_UINT8(1, room.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_INVALID,
        payForCurrentRiddleRetry(*player));
    TEST_ASSERT_EQUAL_UINT32(40, player->character.inventory.gold);
}

void test_cat_threshold_and_catch_chance_progression()
{
    TEST_ASSERT_EQUAL_UINT8(3, selectCatForcedEscapeThreshold(0));
    TEST_ASSERT_EQUAL_UINT8(4, selectCatForcedEscapeThreshold(1));
    TEST_ASSERT_EQUAL_UINT8(5, selectCatForcedEscapeThreshold(2));
    TEST_ASSERT_EQUAL_UINT8(3, selectCatForcedEscapeThreshold(3));
    for (uint8_t attempt = 0; attempt < 4; ++attempt)
        TEST_ASSERT_EQUAL_UINT8(0, getCatCatchChance(attempt, 4));
    TEST_ASSERT_EQUAL_UINT8(25, getCatCatchChance(4, 4));
    TEST_ASSERT_EQUAL_UINT8(40, getCatCatchChance(5, 4));
    TEST_ASSERT_EQUAL_UINT8(60, getCatCatchChance(6, 4));
    TEST_ASSERT_EQUAL_UINT8(80, getCatCatchChance(7, 4));
    TEST_ASSERT_EQUAL_UINT8(100, getCatCatchChance(8, 4));
    TEST_ASSERT_EQUAL_UINT8(100, getCatCatchChance(20, 4));
}

void test_cat_chase_forces_escapes_then_grants_one_retry()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_LEAVES, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_TRUE(startCurrentRiddleCatChase(1));
    TEST_ASSERT_EQUAL_UINT8(4, room.npcSpawn.catForcedEscapeThreshold);

    Entity* cat = nullptr;
    for (uint8_t i = 0; i < dungeon.entityCount; ++i)
        if (isBertramRiddleCat(dungeon.entities[i])) cat = &dungeon.entities[i];
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, cat->character.team);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[cat->y][cat->x]);

    for (uint8_t attempt = 0; attempt < 4; ++attempt)
    {
        const RiddleCatCatchResult result =
            attemptCatchCurrentRiddleCat(*player, *cat, 0);
        TEST_ASSERT_NOT_EQUAL(RIDDLE_CAT_CAUGHT, result);
        TEST_ASSERT_TRUE(cat->active);
    }
    TEST_ASSERT_EQUAL(RIDDLE_CAT_CAUGHT,
        attemptCatchCurrentRiddleCat(*player, *cat, 0));
    TEST_ASSERT_FALSE(cat->active);
    TEST_ASSERT_FALSE(isRiddleRetryRequired(room));
    TEST_ASSERT_TRUE((room.npcSpawn.riddleFlags &
        RIDDLE_FLAG_CAT_JUST_CAUGHT) != 0);
    TEST_ASSERT_EQUAL_STRING("You caught the cat.", messageText);
}

void test_new_wrong_answer_can_start_fresh_cat_chase()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {1, 2, 0};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_SKULL, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_TRUE(startCurrentRiddleCatChase(0));
    Entity* cat = getEntityAt(dungeon.entities, dungeon.entityCount,
        dungeon.entities[1].x, dungeon.entities[1].y);
    TEST_ASSERT_NOT_NULL(cat);
    room.npcSpawn.catCatchAttempts = room.npcSpawn.catForcedEscapeThreshold + 4;
    TEST_ASSERT_EQUAL(RIDDLE_CAT_CAUGHT,
        attemptCatchCurrentRiddleCat(*player, *cat, 99));

    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_EQUAL_UINT8(2, room.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_TRUE(startCurrentRiddleCatChase(2));
    TEST_ASSERT_EQUAL_UINT8(5, room.npcSpawn.catForcedEscapeThreshold);
    TEST_ASSERT_EQUAL_UINT8(0, room.npcSpawn.catCatchAttempts);
}

void test_bypass_retires_active_cat_without_solving_or_key()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 2, 1};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_FIRE, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_TRUE(startCurrentRiddleCatChase(0));
    Entity* cat = &dungeon.entities[1];
    TEST_ASSERT_TRUE(isBertramRiddleCat(*cat));

    TEST_ASSERT_EQUAL(RIDDLEMAN_BYPASS_SUCCEEDED,
        attemptCurrentRiddlemanDoorBypass(DIR_EAST, 99));
    TEST_ASSERT_FALSE(cat->active);
    TEST_ASSERT_TRUE(room.completed);
    TEST_ASSERT_EQUAL(RIDDLE_UNANSWERED, room.npcSpawn.riddle.result);
    TEST_ASSERT_EQUAL_UINT8(2, dungeon.entityCount);
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_COMPLETE, room.npcSpawn.puzzleState);
}

void test_correct_answer_after_paid_retry_uses_normal_key_path()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {2, 0, 1};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_MOUNTAIN, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    player->character.level = 1;
    player->character.inventory.gold = 50;
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_GRANTED,
        payForCurrentRiddleRetry(*player));

    room.npcSpawn.riddle.result = RIDDLE_ANSWERED_CORRECT;
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(true));
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_KEY_PRESENTED, room.npcSpawn.puzzleState);
    TEST_ASSERT_EQUAL_UINT8(2, room.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_NOT_NULL(getEntityAt(dungeon.entities, dungeon.entityCount,
        room.npcSpawn.keyX, room.npcSpawn.keyY));
}

void test_retry_and_cat_state_survive_room_state_copy()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {2, 1, 0};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_FOOTSTEPS, rolls));
    spawnEntity(dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_TRUE(startCurrentRiddleCatChase(2));
    room.npcSpawn.catCatchAttempts = 2;

    const DungeonRoom restored = room;
    TEST_ASSERT_EQUAL(RIDDLE_FOOTSTEPS, restored.npcSpawn.riddle.id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(room.npcSpawn.riddle.answerOrder,
        restored.npcSpawn.riddle.answerOrder, 4);
    TEST_ASSERT_EQUAL_UINT8(1, restored.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_EQUAL_UINT8(5, restored.npcSpawn.catForcedEscapeThreshold);
    TEST_ASSERT_EQUAL_UINT8(2, restored.npcSpawn.catCatchAttempts);
    TEST_ASSERT_TRUE((restored.npcSpawn.riddleFlags &
        RIDDLE_FLAG_CAT_CHASE_ACTIVE) != 0);
}

void test_fourth_wrong_answer_does_not_offer_fifth_retry()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 2};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_FIRE, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    player->character.inventory.gold = 10000;

    for (uint8_t attempt = 1; attempt <= MAX_RIDDLE_ATTEMPTS; ++attempt)
    {
        TEST_ASSERT_TRUE(handleCurrentBertramRiddleResult(false));
        if (attempt < MAX_RIDDLE_ATTEMPTS)
            TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_GRANTED,
                payForCurrentRiddleRetry(*player));
    }
    TEST_ASSERT_EQUAL_UINT8(MAX_RIDDLE_ATTEMPTS,
        room.npcSpawn.riddleAttemptsMade);
    TEST_ASSERT_FALSE(isRiddleRetryRequired(room));
    TEST_ASSERT_EQUAL_UINT16(0, getRiddleRetryCost(
        room.npcSpawn.riddleAttemptsMade, 1));
    TEST_ASSERT_EQUAL(RIDDLE_RETRY_PAYMENT_INVALID,
        payForCurrentRiddleRetry(*player));
    TEST_ASSERT_FALSE(handleCurrentBertramRiddleResult(false));
    TEST_ASSERT_EQUAL_UINT8(MAX_RIDDLE_ATTEMPTS,
        room.npcSpawn.riddleAttemptsMade);
}

void test_bypass_dc_is_scalable_and_harder_than_normal_lock()
{
    const int levelOneDC = getRiddlemanLockDisableDC(1);
    const int levelTwentyDC = getRiddlemanLockDisableDC(20);
    TEST_ASSERT_GREATER_THAN(CHEST_LOCK_DC, levelOneDC);
    TEST_ASSERT_GREATER_THAN(levelOneDC, levelTwentyDC);
    TEST_ASSERT_LESS_OR_EQUAL(30, levelTwentyDC);
}

void test_bypass_attempt_warns_once_and_does_not_stop_check()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {0, 1, 0};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_MOUNTAIN, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    player->character.level = 5;

    TEST_ASSERT_TRUE(noteCurrentRiddlemanBypassAttempt());
    TEST_ASSERT_FALSE(noteCurrentRiddlemanBypassAttempt());
    TEST_ASSERT_EQUAL(RIDDLEMAN_BYPASS_SUCCEEDED,
        attemptCurrentRiddlemanDoorBypass(DIR_EAST, 99));
    TEST_ASSERT_TRUE(room.completed);
    TEST_ASSERT_EQUAL(RIDDLE_UNANSWERED, room.npcSpawn.riddle.result);
    TEST_ASSERT_EQUAL_UINT8(1, dungeon.entityCount);
}

void test_failed_bypass_leaves_unsolved_room_unchanged()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {2, 0, 1};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_FIRE, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    player->character.level = 4;
    const int hpBefore = player->character.health.currentHP;

    TEST_ASSERT_EQUAL(RIDDLEMAN_BYPASS_FAILED,
        attemptCurrentRiddlemanDoorBypass(DIR_EAST, 0));
    TEST_ASSERT_TRUE(isRiddlemanExitLocked(room, DIR_EAST));
    TEST_ASSERT_EQUAL(RIDDLE_ROOM_UNSOLVED, room.npcSpawn.puzzleState);
    TEST_ASSERT_EQUAL(RIDDLE_UNANSWERED, room.npcSpawn.riddle.result);
    TEST_ASSERT_EQUAL(hpBefore, player->character.health.currentHP);
}

void test_bertram_refuses_hostile_actions_without_spending_resources()
{
    DungeonRoom& room = dungeon.rooms[0];
    prepareRoom(room);
    const uint8_t rolls[3] = {1, 0, 2};
    TEST_ASSERT_TRUE(configureRiddlemanPuzzleRoom(
        room, DIR_EAST, RIDDLE_TIME, rolls));
    Entity* player = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER, 2, 2);
    Entity* bertram = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_NPC,
        room.npcSpawn.x, room.npcSpawn.y);
    bertram->npcID = NPC_BERTRAM_RIDDLEMAN;
    const int mpBefore = player->character.magic.currentMP;
    const TurnState turnBefore = player->turn;

    TEST_ASSERT_TRUE(refuseCurrentBertramHostileAction(*bertram));
    TEST_ASSERT_EQUAL_STRING("The Riddleman refuses your attack.", messageText);
    TEST_ASSERT_EQUAL(mpBefore, player->character.magic.currentMP);
    TEST_ASSERT_EQUAL_MEMORY(&turnBefore, &player->turn, sizeof(TurnState));

    TEST_ASSERT_TRUE(refuseCurrentBertramHostileAction(*bertram));
    TEST_ASSERT_EQUAL_STRING(
        "I am A Riddleman and I refuse your attack.", messageText);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_room_configuration_assigns_one_persistent_gate);
    RUN_TEST(test_correct_answer_key_collection_and_unlock_are_single_use);
    RUN_TEST(test_incorrect_answer_allows_retry_without_reward);
    RUN_TEST(test_retry_costs_scale_by_tier_and_level);
    RUN_TEST(test_paid_retry_requires_gold_and_grants_exactly_one_guess);
    RUN_TEST(test_cat_threshold_and_catch_chance_progression);
    RUN_TEST(test_cat_chase_forces_escapes_then_grants_one_retry);
    RUN_TEST(test_new_wrong_answer_can_start_fresh_cat_chase);
    RUN_TEST(test_bypass_retires_active_cat_without_solving_or_key);
    RUN_TEST(test_correct_answer_after_paid_retry_uses_normal_key_path);
    RUN_TEST(test_retry_and_cat_state_survive_room_state_copy);
    RUN_TEST(test_fourth_wrong_answer_does_not_offer_fifth_retry);
    RUN_TEST(test_bypass_dc_is_scalable_and_harder_than_normal_lock);
    RUN_TEST(test_bypass_attempt_warns_once_and_does_not_stop_check);
    RUN_TEST(test_failed_bypass_leaves_unsolved_room_unchanged);
    RUN_TEST(test_bertram_refuses_hostile_actions_without_spending_resources);
    UNITY_END();
}

void loop() {}
