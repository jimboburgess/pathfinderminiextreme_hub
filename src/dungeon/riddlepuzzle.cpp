#include "dungeon/riddlepuzzle.h"

#include <stdlib.h>

#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/furniture.h"
#include "dungeon/roomgen.h"
#include "dungeon/traps.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "graphics/npcsprites.h"
#include "map/interaction.h"

namespace
{
constexpr int8_t CARDINAL_X[4] = {1, 0, -1, 0};
constexpr int8_t CARDINAL_Y[4] = {0, 1, 0, -1};

bool isKeyFloorAvailable(const DungeonRoom& room, int x, int y)
{
    return x > 0 && x < ROOM_SIZE - 1 && y > 0 && y < ROOM_SIZE - 1 &&
        room.map.tiles[y][x] == TILE_FLOOR &&
        getTrapAt(room, x, y) == nullptr &&
        getDungeonFurnitureAt(room, x, y) == nullptr &&
        !isHealingFountainTile(room, x, y) &&
        !(room.npcSpawn.id != NPC_NONE && room.npcSpawn.x == x && room.npcSpawn.y == y);
}

bool findKeyTileNear(const DungeonRoom& room, int originX, int originY,
                     int8_t& resultX, int8_t& resultY, bool checkEntities)
{
    for (int distance = 1; distance < ROOM_SIZE * 2; ++distance)
    {
        for (int y = 1; y < ROOM_SIZE - 1; ++y)
        {
            for (int x = 1; x < ROOM_SIZE - 1; ++x)
            {
                if (abs(x - originX) + abs(y - originY) != distance ||
                    !isKeyFloorAvailable(room, x, y)) continue;
                if (checkEntities && getEntityAt(
                        dungeon.entities, dungeon.entityCount, x, y) != nullptr) continue;
                resultX = static_cast<int8_t>(x);
                resultY = static_cast<int8_t>(y);
                return true;
            }
        }
    }
    return false;
}

bool isCatFloorAvailable(const DungeonRoom& room, int x, int y)
{
    return x > 0 && x < ROOM_SIZE - 1 && y > 0 && y < ROOM_SIZE - 1 &&
        room.map.tiles[y][x] == TILE_FLOOR &&
        getTrapAt(room, x, y) == nullptr &&
        getDungeonFurnitureAt(room, x, y) == nullptr &&
        !isHealingFountainTile(room, x, y) &&
        !(room.npcSpawn.id != NPC_NONE &&
          room.npcSpawn.x == x && room.npcSpawn.y == y) &&
        getEntityAt(dungeon.entities, dungeon.entityCount, x, y) == nullptr;
}

Entity* getCurrentRiddleCat()
{
    for (uint8_t index = 0; dungeon.entities != nullptr &&
         index < dungeon.entityCount; ++index)
    {
        if (isBertramRiddleCat(dungeon.entities[index]))
            return &dungeon.entities[index];
    }
    return nullptr;
}

bool findCatSpawnTile(const DungeonRoom& room, const Entity& player,
                      int8_t& resultX, int8_t& resultY)
{
    int bestDistance = -1;
    bool found = false;
    for (int y = 1; y < ROOM_SIZE - 1; ++y)
    {
        for (int x = 1; x < ROOM_SIZE - 1; ++x)
        {
            if (!isCatFloorAvailable(room, x, y)) continue;
            const int distance = abs(x - player.x) + abs(y - player.y);
            if (distance <= bestDistance) continue;
            bestDistance = distance;
            resultX = static_cast<int8_t>(x);
            resultY = static_cast<int8_t>(y);
            found = true;
        }
    }
    return found;
}

bool fleeCatFromPlayer(
    const DungeonRoom& room, Entity& player, Entity& cat, uint8_t catchChance)
{
    const int maximumStep = catchChance >= 80 ? 1 :
        (catchChance >= 60 ? 2 : 3);
    int8_t bestX = -1;
    int8_t bestY = -1;
    int bestPlayerDistance = -1;
    int bestStepDistance = ROOM_SIZE * 2;

    for (int y = 1; y < ROOM_SIZE - 1; ++y)
    {
        for (int x = 1; x < ROOM_SIZE - 1; ++x)
        {
            const int stepDistance = abs(x - cat.x) + abs(y - cat.y);
            if (stepDistance < 1 || stepDistance > maximumStep ||
                (x != cat.x && y != cat.y))
                continue;
            const int stepX = x == cat.x ? 0 : (x > cat.x ? 1 : -1);
            const int stepY = y == cat.y ? 0 : (y > cat.y ? 1 : -1);
            bool clearPath = true;
            int pathX = cat.x;
            int pathY = cat.y;
            for (int step = 0; step < stepDistance; ++step)
            {
                pathX += stepX;
                pathY += stepY;
                if (!isCatFloorAvailable(room, pathX, pathY))
                {
                    clearPath = false;
                    break;
                }
            }
            if (!clearPath) continue;
            const int playerDistance = abs(x - player.x) + abs(y - player.y);
            if (playerDistance < bestPlayerDistance ||
                (playerDistance == bestPlayerDistance &&
                 stepDistance >= bestStepDistance))
                continue;
            bestPlayerDistance = playerDistance;
            bestStepDistance = stepDistance;
            bestX = static_cast<int8_t>(x);
            bestY = static_cast<int8_t>(y);
        }
    }

    if (bestX < 0 || bestY < 0) return false;
    const int oldX = cat.x;
    const int oldY = cat.y;
    cat.x = static_cast<uint8_t>(bestX);
    cat.y = static_cast<uint8_t>(bestY);
    markTileDirty(oldX, oldY);
    markTileDirty(cat.x, cat.y);
    return true;
}

void retireCurrentRiddleCat(DungeonRoom& room)
{
    Entity* cat = getCurrentRiddleCat();
    if (cat != nullptr)
    {
        const int x = cat->x;
        const int y = cat->y;
        cat->active = false;
        markTileDirty(x, y);
    }
    room.npcSpawn.riddleFlags &= static_cast<uint8_t>(
        ~(RIDDLE_FLAG_CAT_CHASE_ACTIVE | RIDDLE_FLAG_RETRY_REQUIRED |
          RIDDLE_FLAG_CAT_JUST_CAUGHT));
}
}

bool isRiddlemanPuzzleRoom(const DungeonRoom& room)
{
    return room.type == ROOM_PUZZLE &&
        room.npcSpawn.id == NPC_BERTRAM_RIDDLEMAN &&
        room.npcSpawn.puzzleState != RIDDLE_ROOM_NONE;
}

bool configureRiddlemanPuzzleRoom(DungeonRoom& room, Direction lockedDirection,
                                  RiddleID riddleID, const uint8_t rolls[3])
{
    if (room.type != ROOM_PUZZLE || getRoomConnection(room, lockedDirection) == nullptr)
        return false;

    const RoomConnection* exit = getRoomConnection(room, lockedDirection);
    int preferredX = ROOM_SIZE / 2;
    int preferredY = ROOM_SIZE / 2;
    if (lockedDirection == DIR_NORTH) { preferredX = exit->x; preferredY = 4; }
    else if (lockedDirection == DIR_SOUTH) { preferredX = exit->x; preferredY = ROOM_SIZE - 5; }
    else if (lockedDirection == DIR_WEST) { preferredX = 4; preferredY = exit->y; }
    else if (lockedDirection == DIR_EAST) { preferredX = ROOM_SIZE - 5; preferredY = exit->y; }

    for (int distance = 0; distance < ROOM_SIZE * 2; ++distance)
    {
        for (int y = 2; y < ROOM_SIZE - 2; ++y)
        {
            for (int x = 2; x < ROOM_SIZE - 2; ++x)
            {
                if (abs(x - preferredX) + abs(y - preferredY) != distance ||
                    room.map.tiles[y][x] != TILE_FLOOR) continue;
                DungeonRoom candidate = room;
                if (!placeDungeonNPC(candidate, NPC_BERTRAM_RIDDLEMAN, x, y)) continue;
                if (!findKeyTileNear(candidate, x, y,
                                     candidate.npcSpawn.keyX,
                                     candidate.npcSpawn.keyY, false)) continue;
                candidate.npcSpawn.puzzleState = RIDDLE_ROOM_UNSOLVED;
                candidate.npcSpawn.lockedExitDirection = lockedDirection;
                candidate.npcSpawn.riddleFlags = RIDDLE_FLAG_NONE;
                candidate.npcSpawn.riddleAttemptsMade = 0;
                candidate.npcSpawn.catForcedEscapeThreshold = 0;
                candidate.npcSpawn.catCatchAttempts = 0;
                initializeRiddleState(candidate.npcSpawn.riddle, riddleID, rolls);
                room = candidate;
                return isValidRiddleAnswerOrder(room.npcSpawn.riddle);
            }
        }
    }
    return false;
}

bool isRiddlemanExitLocked(const DungeonRoom& room, Direction direction)
{
    return isRiddlemanPuzzleRoom(room) &&
        room.npcSpawn.lockedExitDirection == direction &&
        room.npcSpawn.puzzleState != RIDDLE_ROOM_COMPLETE;
}

bool handleCurrentBertramRiddleResult(bool correct)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanPuzzleRoom(room) ||
        room.npcSpawn.puzzleState != RIDDLE_ROOM_UNSOLVED ||
        isRiddleRetryRequired(room) ||
        room.npcSpawn.riddleAttemptsMade >= MAX_RIDDLE_ATTEMPTS) return false;

    if (room.npcSpawn.riddleAttemptsMade < MAX_RIDDLE_ATTEMPTS)
        ++room.npcSpawn.riddleAttemptsMade;

    if (!correct)
    {
        room.npcSpawn.riddle.result = RIDDLE_UNANSWERED;
        room.npcSpawn.riddle.selectedAnswer = 255;
        if (room.npcSpawn.riddleAttemptsMade < MAX_RIDDLE_ATTEMPTS)
            room.npcSpawn.riddleFlags |= RIDDLE_FLAG_RETRY_REQUIRED;
        setGameMessage("Incorrect.");
        return true;
    }

    int8_t keyX = -1;
    int8_t keyY = -1;
    if (!findKeyTileNear(room, room.npcSpawn.x, room.npcSpawn.y,
                         keyX, keyY, true)) return false;
    Entity* key = spawnEntity(dungeon.entities, dungeon.entityCount,
                              ENTITY_PUZZLE_KEY, keyX, keyY);
    if (key == nullptr) return false;
    room.npcSpawn.keyX = keyX;
    room.npcSpawn.keyY = keyY;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_KEY_PRESENTED;
    dungeon.roomRuntime[dungeon.currentRoom].entityCount = dungeon.entityCount;
    markTileDirty(keyX, keyY);
    setGameMessage("Correct.");
    return true;
}

bool collectCurrentRiddleKey(Entity& keyEntity)
{
    if (dungeon.currentRoom >= dungeon.roomCount ||
        keyEntity.type != ENTITY_PUZZLE_KEY || !keyEntity.active) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanPuzzleRoom(room) ||
        room.npcSpawn.puzzleState != RIDDLE_ROOM_KEY_PRESENTED ||
        keyEntity.x != room.npcSpawn.keyX || keyEntity.y != room.npcSpawn.keyY)
        return false;
    const int x = keyEntity.x;
    const int y = keyEntity.y;
    keyEntity.active = false;
    room.npcSpawn.puzzleState = RIDDLE_ROOM_KEY_COLLECTED;
    markTileDirty(x, y);
    setGameMessage("You take the puzzle key.");
    return true;
}

bool tryUnlockCurrentRiddleExit(Direction direction)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanExitLocked(room, direction)) return true;
    if (room.npcSpawn.puzzleState != RIDDLE_ROOM_KEY_COLLECTED)
    {
        setGameMessage("The exit is locked.");
        return false;
    }
    room.npcSpawn.puzzleState = RIDDLE_ROOM_COMPLETE;
    room.completed = true;
    setGameMessage("The key unlocks the door.");
    return true;
}

int getRiddlemanLockDisableDC(uint8_t challengeLevel)
{
    if (challengeLevel < 1) challengeLevel = 1;
    if (challengeLevel > 20) challengeLevel = 20;
    return CHEST_LOCK_DC + RIDDLEMAN_LOCK_DC_SURCHARGE +
        (static_cast<int>(challengeLevel) + 1) / 2;
}

bool getCurrentRiddlemanExitDirectionAt(int x, int y, Direction& direction)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanPuzzleRoom(room)) return false;
    const Direction exitDirection = static_cast<Direction>(
        room.npcSpawn.lockedExitDirection);
    const RoomConnection* connection = getRoomConnection(room, exitDirection);
    if (connection == nullptr || connection->x != x || connection->y != y)
        return false;
    direction = exitDirection;
    return true;
}

bool noteCurrentRiddlemanBypassAttempt()
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanPuzzleRoom(room) ||
        (room.npcSpawn.riddleFlags & RIDDLE_FLAG_BYPASS_REACTION_SHOWN) != 0)
        return false;
    room.npcSpawn.riddleFlags |= RIDDLE_FLAG_BYPASS_REACTION_SHOWN;
    return true;
}

RiddlemanDoorBypassResult attemptCurrentRiddlemanDoorBypass(
    Direction direction, int disableDeviceTotal)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return RIDDLEMAN_BYPASS_INVALID;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanExitLocked(room, direction)) return RIDDLEMAN_BYPASS_INVALID;

    const Entity* player = getPlayerEntity(dungeon.entities, dungeon.entityCount);
    const uint8_t challengeLevel = player != nullptr
        ? player->character.level : 1;
    if (disableDeviceTotal < getRiddlemanLockDisableDC(challengeLevel))
        return RIDDLEMAN_BYPASS_FAILED;

    // Passage is complete, but the riddle result remains untouched. This is
    // deliberately distinct from solving the riddle and never spawns a key.
    room.npcSpawn.puzzleState = RIDDLE_ROOM_COMPLETE;
    room.completed = true;
    retireCurrentRiddleCat(room);
    return RIDDLEMAN_BYPASS_SUCCEEDED;
}

bool refuseCurrentBertramHostileAction(Entity& target)
{
    if (!isBertramRiddleman(target) ||
        dungeon.currentRoom >= dungeon.roomCount)
        return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddlemanPuzzleRoom(room)) return false;

    const bool repeated =
        (room.npcSpawn.riddleFlags & RIDDLE_FLAG_ATTACK_REFUSED) != 0;
    room.npcSpawn.riddleFlags |= RIDDLE_FLAG_ATTACK_REFUSED;
    setGameMessage(repeated
        ? "I am A Riddleman and I refuse your attack."
        : "The Riddleman refuses your attack.");
    return true;
}

uint16_t getRiddleRetryCost(uint8_t attemptsMade, uint8_t level)
{
    static const uint16_t BASE_COSTS[3] = {50, 250, 1000};
    if (attemptsMade < 1 || attemptsMade >= MAX_RIDDLE_ATTEMPTS) return 0;
    if (level < 1) level = 1;
    if (level > 20) level = 20;
    const uint32_t factor = 100U + 5U * (level - 1U);
    return static_cast<uint16_t>(
        static_cast<uint32_t>(BASE_COSTS[attemptsMade - 1]) * factor / 100U);
}

uint8_t selectCatForcedEscapeThreshold(uint8_t randomValue)
{
    return static_cast<uint8_t>(3U + randomValue % 3U);
}

uint8_t getCatCatchChance(
    uint8_t catchAttemptsMade, uint8_t forcedEscapeThreshold)
{
    if (catchAttemptsMade < forcedEscapeThreshold) return 0;
    static const uint8_t CHANCES[5] = {25, 40, 60, 80, 100};
    const uint8_t eligibleAttempt = static_cast<uint8_t>(
        catchAttemptsMade - forcedEscapeThreshold);
    return CHANCES[eligibleAttempt < 4 ? eligibleAttempt : 4];
}

bool isRiddleRetryRequired(const DungeonRoom& room)
{
    return isRiddlemanPuzzleRoom(room) &&
        (room.npcSpawn.riddleFlags & RIDDLE_FLAG_RETRY_REQUIRED) != 0;
}

RiddleRetryPaymentResult payForCurrentRiddleRetry(Entity& player)
{
    if (dungeon.currentRoom >= dungeon.roomCount ||
        player.type != ENTITY_PLAYER || !player.active)
        return RIDDLE_RETRY_PAYMENT_INVALID;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddleRetryRequired(room) ||
        room.npcSpawn.riddleAttemptsMade >= MAX_RIDDLE_ATTEMPTS)
        return RIDDLE_RETRY_PAYMENT_INVALID;
    const uint16_t cost = getRiddleRetryCost(
        room.npcSpawn.riddleAttemptsMade, player.character.level);
    if (cost == 0) return RIDDLE_RETRY_PAYMENT_INVALID;
    if (player.character.inventory.gold < cost)
        return RIDDLE_RETRY_PAYMENT_INSUFFICIENT_GOLD;
    player.character.inventory.gold -= cost;
    room.npcSpawn.riddleFlags &= static_cast<uint8_t>(
        ~(RIDDLE_FLAG_RETRY_REQUIRED | RIDDLE_FLAG_CAT_JUST_CAUGHT));
    return RIDDLE_RETRY_PAYMENT_GRANTED;
}

bool isBertramRiddleCat(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_RIDDLE_CAT;
}

bool startCurrentRiddleCatChase(uint8_t thresholdRandomValue)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if (!isRiddleRetryRequired(room) ||
        room.npcSpawn.riddleAttemptsMade >= MAX_RIDDLE_ATTEMPTS ||
        (room.npcSpawn.riddleFlags & RIDDLE_FLAG_CAT_CHASE_ACTIVE) != 0)
        return false;
    Entity* player = getPlayerEntity(dungeon.entities, dungeon.entityCount);
    if (player == nullptr) return false;
    int8_t catX = -1;
    int8_t catY = -1;
    if (!findCatSpawnTile(room, *player, catX, catY)) return false;
    Entity* cat = spawnEntity(
        dungeon.entities, dungeon.entityCount,
        ENTITY_RIDDLE_CAT, catX, catY);
    if (cat == nullptr) return false;
    cat->character.team = TEAM_NEUTRAL;
    cat->character.state = STATE_ALIVE;
    cat->sprite = bertramCat16x16;
    room.npcSpawn.catForcedEscapeThreshold =
        selectCatForcedEscapeThreshold(thresholdRandomValue);
    room.npcSpawn.catCatchAttempts = 0;
    room.npcSpawn.riddleFlags |= RIDDLE_FLAG_CAT_CHASE_ACTIVE;
    room.npcSpawn.riddleFlags &= static_cast<uint8_t>(~RIDDLE_FLAG_CAT_JUST_CAUGHT);
    dungeon.roomRuntime[dungeon.currentRoom].entityCount = dungeon.entityCount;
    markTileDirty(catX, catY);
    return true;
}

RiddleCatCatchResult attemptCatchCurrentRiddleCat(
    Entity& player, Entity& cat, uint8_t percentileRoll)
{
    if (dungeon.currentRoom >= dungeon.roomCount ||
        !isBertramRiddleCat(cat) || player.type != ENTITY_PLAYER)
        return RIDDLE_CAT_NOT_ACTIVE;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if ((room.npcSpawn.riddleFlags & RIDDLE_FLAG_CAT_CHASE_ACTIVE) == 0)
        return RIDDLE_CAT_NOT_ACTIVE;

    const uint8_t chance = getCatCatchChance(
        room.npcSpawn.catCatchAttempts,
        room.npcSpawn.catForcedEscapeThreshold);
    if (room.npcSpawn.catCatchAttempts < 255)
        ++room.npcSpawn.catCatchAttempts;
    const bool forcedEscape = chance == 0;
    bool caught = !forcedEscape && percentileRoll % 100U < chance;

    if (!caught && !fleeCatFromPlayer(room, player, cat, chance))
    {
        if (forcedEscape) return RIDDLE_CAT_CORNERED;
        caught = true;
    }

    if (!caught) return RIDDLE_CAT_ESCAPED;
    const int catX = cat.x;
    const int catY = cat.y;
    cat.active = false;
    room.npcSpawn.riddleFlags &= static_cast<uint8_t>(
        ~(RIDDLE_FLAG_CAT_CHASE_ACTIVE | RIDDLE_FLAG_RETRY_REQUIRED));
    room.npcSpawn.riddleFlags |= RIDDLE_FLAG_CAT_JUST_CAUGHT;
    markTileDirty(catX, catY);
    setGameMessage("You caught the cat.");
    return RIDDLE_CAT_CAUGHT;
}
