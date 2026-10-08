#include "multiplayer_activity.h"

#include <Arduino.h>
#include <string.h>

#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/furniture.h"
#include "dungeon/traps.h"
#include "forest/forest.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "graphics/npcsprites.h"
#include "graphics/sprites.h"
#include "graphics/tiles.h"
#include "input/menu.h"
#include "map/activemap.h"
#include "map/playermovement.h"
#include "multiplayer_session.h"

#ifndef MULTIPLAYER_DEBUG
#define MULTIPLAYER_DEBUG 0
#endif

#if MULTIPLAYER_DEBUG
#define MP_DEBUG(message) Serial.println(message)
#define MP_DEBUGF(...) Serial.printf(__VA_ARGS__)
#else
#define MP_DEBUG(message) do {} while (false)
#define MP_DEBUGF(...) do {} while (false)
#endif

namespace
{
constexpr uint32_t INVITE_RETRY_MS = 750;
constexpr uint32_t INVITE_TIMEOUT_MS = 8000;
constexpr uint32_t SNAPSHOT_RETRY_MS = 600;
constexpr uint8_t SNAPSHOT_RETRY_LIMIT = 8;
constexpr uint32_t READY_RETRY_MS = 500;
constexpr uint32_t START_REPEAT_MS = 700;
constexpr uint8_t START_REPEAT_COUNT = 4;
constexpr uint8_t WORLD_SNAPSHOT_VERSION = 1;
constexpr uint8_t DUNGEON_GRAPH_VERSION = 1;
constexpr uint8_t WORLD_ENTITY_RECORD_SIZE = 10;

enum class ActivityPhase : uint8_t
{
    NONE,
    WAITING_FOR_RESPONSES,
    LOADING,
    ACTIVE
};

struct Participant
{
    bool invited = false;
    bool responded = false;
    bool participating = false;
    bool ready = false;
    bool characterReceived = false;
    bool spawnReceived = false;
    bool hasMoveSequence = false;
    uint16_t lastMoveSequence = 0;
    NetworkCharacterState character{};
    PlayerSpawnPayload spawn{};
    uint8_t syncAttempts = 0;
    uint32_t lastSyncAt = 0;
};

struct ActivityRuntime
{
    ActivityPhase phase = ActivityPhase::NONE;
    MultiplayerActivityType type = MultiplayerActivityType::NONE;
    uint32_t activityID = 0;
    uint16_t snapshotEpoch = 0;
    uint8_t participantMask = 0;
    PlayerID localPlayerID = INVALID_PLAYER_ID;
    uint8_t currentRoom = NETWORK_NO_ROOM;
    uint8_t transitionEntry = 0;
    uint16_t localMovementSequence = 0;
    uint32_t phaseStartedAt = 0;
    uint32_t lastInviteAt = 0;
    uint32_t lastReadyAt = 0;
    uint32_t lastStartAt = 0;
    uint8_t startRepeatsRemaining = 0;
    bool pendingInvite = false;
    bool pendingInviteAccepted = false;
    bool pendingInviteDeclined = false;
    bool graphApplied = false;
    bool detailApplied = false;
    bool worldApplied = false;
    uint8_t expectedGraphChunks = 0;
    uint8_t expectedDetailChunks = 0;
    uint8_t expectedWorldChunks = 0;
    char pendingHostName[NETWORK_PLAYER_NAME_SIZE] = {};
    Participant participants[MAX_MULTIPLAYER_PLAYERS] = {};
    BoundedSnapshotReceiver snapshotReceiver{};
};

ActivityRuntime runtime;

class BufferWriter
{
public:
    BufferWriter(uint8_t* destination, size_t capacity)
        : destination(destination), capacity(capacity) {}

    bool u8(uint8_t value)
    {
        if (position >= capacity) return false;
        destination[position++] = value;
        return true;
    }

    bool u16(uint16_t value)
    {
        return u8(static_cast<uint8_t>(value)) &&
               u8(static_cast<uint8_t>(value >> 8));
    }

    size_t size() const { return position; }

private:
    uint8_t* destination;
    size_t capacity;
    size_t position = 0;
};

class BufferReader
{
public:
    BufferReader(const uint8_t* source, size_t size)
        : source(source), sizeValue(size) {}

    bool u8(uint8_t& value)
    {
        if (position >= sizeValue) return false;
        value = source[position++];
        return true;
    }

    bool u16(uint16_t& value)
    {
        uint8_t low = 0;
        uint8_t high = 0;
        if (!u8(low) || !u8(high)) return false;
        value = static_cast<uint16_t>(low) |
                (static_cast<uint16_t>(high) << 8);
        return true;
    }

    bool finished() const { return position == sizeValue; }

private:
    const uint8_t* source;
    size_t sizeValue;
    size_t position = 0;
};

uint8_t playerBit(PlayerID playerID)
{
    return isValidPlayerID(playerID)
        ? static_cast<uint8_t>(1u << playerID) : 0;
}

bool elapsed(uint32_t now, uint32_t then, uint32_t interval)
{
    return static_cast<uint32_t>(now - then) >= interval;
}

bool isParticipant(PlayerID playerID)
{
    return isValidPlayerID(playerID) &&
           (runtime.participantMask & playerBit(playerID)) != 0;
}

uint8_t getCurrentRoomID()
{
    return runtime.type == MultiplayerActivityType::DUNGEON
        ? dungeon.currentRoom : NETWORK_NO_ROOM;
}

void resetActivityState()
{
    runtime = ActivityRuntime{};
}

void setFullMapRedraw()
{
    backgroundNeedsRedraw = true;
    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void copyCharacterToNetwork(
    PlayerID playerID,
    const Character& source,
    NetworkCharacterState& destination)
{
    destination = NetworkCharacterState{};
    destination.playerID = playerID;
    strncpy(destination.displayName,
            source.name.length() > 0 ? source.name.c_str() : "Adventurer",
            NETWORK_PLAYER_NAME_SIZE - 1);
    destination.characterClass = static_cast<uint8_t>(source.characterClass);
    destination.level = source.level;
    destination.currentHP = static_cast<int16_t>(source.health.currentHP);
    destination.maxHP = static_cast<int16_t>(source.health.maxHP);
    destination.abilityScores[0] = source.abilities.strength;
    destination.abilityScores[1] = source.abilities.dexterity;
    destination.abilityScores[2] = source.abilities.constitution;
    destination.abilityScores[3] = source.abilities.intelligence;
    destination.abilityScores[4] = source.abilities.wisdom;
    destination.abilityScores[5] = source.abilities.charisma;
    destination.speed = source.speed;
}

void applyNetworkCharacter(
    const NetworkCharacterState& source,
    Character& destination)
{
    destination = Character{};
    destination.name = source.displayName;
    destination.characterClass =
        static_cast<CharacterClass>(source.characterClass);
    destination.team = TEAM_PLAYER;
    destination.state = source.currentHP > 0 ? STATE_ALIVE : STATE_UNCONSCIOUS;
    destination.level = source.level;
    destination.health.currentHP = source.currentHP;
    destination.health.maxHP = source.maxHP;
    destination.abilities.strength = source.abilityScores[0];
    destination.abilities.dexterity = source.abilityScores[1];
    destination.abilities.constitution = source.abilityScores[2];
    destination.abilities.intelligence = source.abilityScores[3];
    destination.abilities.wisdom = source.abilityScores[4];
    destination.abilities.charisma = source.abilityScores[5];
    destination.speed = source.speed;
}

bool sendToPlayer(
    PlayerID destination,
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags = NETWORK_FLAG_CONTROL_EVENT)
{
    return multiplayerSession.sendGameplayPacketToPlayer(
        destination, packetType, payload, payloadSize, flags);
}

bool sendToHost(
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags = NETWORK_FLAG_CONTROL_EVENT)
{
    return multiplayerSession.sendGameplayPacketToHost(
        packetType, payload, payloadSize, flags);
}

bool sendActivityReady()
{
    ActivityReadyPayload ready{};
    ready.activityID = runtime.activityID;
    ready.snapshotEpoch = runtime.snapshotEpoch;
    uint8_t bytes[16] = {};
    size_t size = 0;
    return encodeActivityReady(ready, bytes, sizeof(bytes), size) &&
           sendToHost(NetworkPacketType::ACTIVITY_READY, bytes, size);
}

bool tileIsValidSpawn(int x, int y)
{
    if (!isInsideActiveMap(x, y)) return false;
    const TileType tile = getActiveMapTile(x, y);
    if (gameState == GAME_FOREST)
    {
        if (tile == TILE_TREE || tile == TILE_WATER || tile == TILE_VOID)
            return false;
    }
    else if (gameState == GAME_DUNGEON)
    {
        if (!isDungeonFloorTerrain(tile)) return false;
        const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
        const TrapInstance* trap = getTrapAt(room, x, y);
        if (trap != nullptr && isTrapActive(*trap)) return false;
        if (getDungeonFurnitureAt(room, x, y) != nullptr) return false;
    }
    else
        return false;

    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    return entities != nullptr && getEntityAt(
        entities, count, static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == nullptr;
}

bool activeMapSpawnPredicate(int x, int y, void*)
{
    return tileIsValidSpawn(x, y);
}

bool findSpawnNear(int originX, int originY, uint8_t& resultX, uint8_t& resultY)
{
    return findDeterministicMultiplayerSpawn(
        static_cast<uint8_t>(getActiveMapWidth()),
        static_cast<uint8_t>(getActiveMapHeight()),
        originX, originY, activeMapSpawnPredicate, nullptr, resultX, resultY);
}

Entity* spawnPlayerFromState(
    const NetworkCharacterState& character,
    uint8_t x,
    uint8_t y)
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    if (entities == nullptr) return nullptr;

    Entity* existing = getPlayerEntityByOwner(
        entities, entityCount, character.playerID);
    if (existing != nullptr)
    {
        existing->x = x;
        existing->y = y;
        if (character.playerID == runtime.localPlayerID)
            existing->character = player;
        else
            applyNetworkCharacter(character, existing->character);
        existing->character.team = TEAM_PLAYER;
        existing->sprite = getPlayerSprite(existing->character.characterClass);
        return existing;
    }

    uint8_t* count = gameState == GAME_FOREST
        ? &forestEntityCount : &dungeon.entityCount;
    Entity* result = spawnEntity(entities, *count, ENTITY_PLAYER, x, y);
    if (result == nullptr) return nullptr;
    result->ownerPlayerID = character.playerID;
    if (character.playerID == runtime.localPlayerID)
        result->character = player;
    else
        applyNetworkCharacter(character, result->character);
    result->character.team = TEAM_PLAYER;
    result->sprite = getPlayerSprite(result->character.characterClass);
    return result;
}

void removePlayerEntity(PlayerID playerID)
{
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    if (entities == nullptr) return;
    Entity* entity = getPlayerEntityByOwner(entities, count, playerID);
    if (entity == nullptr) return;
    markEntityFootprintDirty(*entity);
    removeEntity(*entity);
}

void refreshHostParticipantCharacters()
{
    if (!multiplayerSession.isHost()) return;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    if (entities == nullptr) return;
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        Entity* entity = getPlayerEntityByOwner(entities, count, playerID);
        if (entity != nullptr)
            copyCharacterToNetwork(
                playerID, entity->character,
                runtime.participants[playerID].character);
    }
}

bool spawnHostParticipants()
{
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    if (entities == nullptr) return false;
    Entity* host = getPlayerEntity(entities, count);
    if (host == nullptr) return false;

    host->ownerPlayerID = HOST_PLAYER_ID;
    copyCharacterToNetwork(HOST_PLAYER_ID, host->character,
                           runtime.participants[HOST_PLAYER_ID].character);
    runtime.participants[HOST_PLAYER_ID].characterReceived = true;
    runtime.participants[HOST_PLAYER_ID].participating = true;
    runtime.participants[HOST_PLAYER_ID].ready = true;
    PlayerSpawnPayload& hostSpawn =
        runtime.participants[HOST_PLAYER_ID].spawn;
    hostSpawn.activityID = runtime.activityID;
    hostSpawn.playerID = HOST_PLAYER_ID;
    hostSpawn.roomID = getCurrentRoomID();
    hostSpawn.x = host->x;
    hostSpawn.y = host->y;
    runtime.participants[HOST_PLAYER_ID].spawnReceived = true;

    const int originX = host->x;
    const int originY = host->y;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        Participant& participant = runtime.participants[playerID];
        if (!participant.participating) continue;
        uint8_t x = 0;
        uint8_t y = 0;
        if (!findSpawnNear(originX, originY, x, y) ||
            spawnPlayerFromState(participant.character, x, y) == nullptr)
        {
            participant.participating = false;
            runtime.participantMask &= ~playerBit(playerID);
            continue;
        }
        participant.spawn.activityID = runtime.activityID;
        participant.spawn.playerID = playerID;
        participant.spawn.roomID = getCurrentRoomID();
        participant.spawn.x = x;
        participant.spawn.y = y;
        participant.spawnReceived = true;
    }
    return true;
}

bool encodeDungeonGraph(uint8_t* destination, size_t capacity, size_t& size)
{
    BufferWriter writer(destination, capacity);
    if (!writer.u8(DUNGEON_GRAPH_VERSION) || !writer.u8(dungeon.roomCount) ||
        !writer.u8(dungeon.bossRoom) || !writer.u8(dungeon.treasureRoom) ||
        !writer.u8(dungeon.riddleRoom) ||
        !writer.u8(static_cast<uint8_t>(dungeon.encounterTheme)) ||
        !writer.u8(dungeon.hasRubbleTheme ? 1 : 0)) return false;

    for (uint8_t roomID = 0; roomID < dungeon.roomCount; ++roomID)
    {
        const DungeonRoom& room = dungeon.rooms[roomID];
        if (!writer.u8(roomID) ||
            !writer.u8(static_cast<uint8_t>(room.type)) ||
            !writer.u8(static_cast<uint8_t>(room.puzzleType)) ||
            !writer.u8(static_cast<uint8_t>(room.encounterTheme)) ||
            !writer.u8(static_cast<uint8_t>(room.shape)) ||
            !writer.u8((room.discovered ? 1 : 0) |
                       (room.completed ? 2 : 0)) ||
            !writer.u8(room.north) || !writer.u8(room.east) ||
            !writer.u8(room.south) || !writer.u8(room.west) ||
            !writer.u8(static_cast<uint8_t>(room.dungeonX)) ||
            !writer.u8(static_cast<uint8_t>(room.dungeonY)) ||
            !writer.u8(room.connectionCount)) return false;
        for (uint8_t index = 0; index < MAX_ROOM_CONNECTIONS; ++index)
        {
            const RoomConnection& connection = room.connections[index];
            if (!writer.u8(static_cast<uint8_t>(connection.direction)) ||
                !writer.u8(connection.x) || !writer.u8(connection.y))
                return false;
        }
    }
    size = writer.size();
    return true;
}

bool applyDungeonGraph(const uint8_t* source, size_t size)
{
    BufferReader reader(source, size);
    uint8_t version = 0;
    uint8_t roomCount = 0;
    uint8_t encounterTheme = 0;
    uint8_t rubble = 0;
    if (!reader.u8(version) || version != DUNGEON_GRAPH_VERSION ||
        !reader.u8(roomCount) || roomCount < MIN_DUNGEON_ROOMS ||
        roomCount > MAX_DUNGEON_ROOMS)
        return false;

    resetDungeonRun(dungeon);
    dungeon.roomCount = roomCount;
    if (!reader.u8(dungeon.bossRoom) ||
        !reader.u8(dungeon.treasureRoom) || !reader.u8(dungeon.riddleRoom) ||
        !reader.u8(encounterTheme) || !reader.u8(rubble)) return false;
    if (dungeon.bossRoom >= roomCount || dungeon.treasureRoom >= roomCount ||
        dungeon.riddleRoom >= roomCount ||
        encounterTheme > static_cast<uint8_t>(ENCOUNTER_SPIDER) || rubble > 1)
        return false;
    dungeon.encounterTheme = static_cast<EncounterTheme>(encounterTheme);
    dungeon.hasRubbleTheme = rubble != 0;
    dungeon.runActive = true;

    for (uint8_t expectedRoom = 0; expectedRoom < roomCount; ++expectedRoom)
    {
        uint8_t roomID = 0;
        uint8_t roomType = 0;
        uint8_t puzzleType = 0;
        uint8_t theme = 0;
        uint8_t shape = 0;
        uint8_t flags = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t connectionCount = 0;
        DungeonRoom& room = dungeon.rooms[expectedRoom];
        if (!reader.u8(roomID) || roomID != expectedRoom ||
            !reader.u8(roomType) || !reader.u8(puzzleType) ||
            !reader.u8(theme) || !reader.u8(shape) || !reader.u8(flags) ||
            !reader.u8(room.north) || !reader.u8(room.east) ||
            !reader.u8(room.south) || !reader.u8(room.west) ||
            !reader.u8(x) || !reader.u8(y) ||
            !reader.u8(connectionCount) ||
            roomType > ROOM_BOSS || theme > ENCOUNTER_SPIDER ||
            connectionCount > MAX_ROOM_CONNECTIONS)
            return false;
        room.type = static_cast<RoomType>(roomType);
        room.puzzleType = static_cast<DungeonPuzzleType>(puzzleType);
        room.encounterTheme = static_cast<EncounterTheme>(theme);
        room.shape = static_cast<RoomShape>(shape);
        room.discovered = (flags & 1) != 0;
        room.completed = (flags & 2) != 0;
        room.dungeonX = static_cast<int8_t>(x);
        room.dungeonY = static_cast<int8_t>(y);
        room.connectionCount = connectionCount;
        for (uint8_t index = 0; index < MAX_ROOM_CONNECTIONS; ++index)
        {
            uint8_t direction = 0;
            if (!reader.u8(direction) ||
                !reader.u8(room.connections[index].x) ||
                !reader.u8(room.connections[index].y) || direction > DIR_WEST)
                return false;
            room.connections[index].direction = static_cast<Direction>(direction);
        }
    }
    return reader.finished();
}

bool encodeRoomDetailSnapshot(uint8_t* destination, size_t capacity, size_t& size)
{
    if (dungeon.currentRoom >= dungeon.roomCount) return false;
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    BufferWriter writer(destination, capacity);
    if (!writer.u8(1)) return false;

    uint8_t trapCount = 0;
    for (const TrapInstance& trap : room.traps)
        if (trap.id != TRAP_NONE) ++trapCount;
    if (!writer.u8(trapCount)) return false;
    for (const TrapInstance& trap : room.traps)
    {
        if (trap.id == TRAP_NONE) continue;
        const uint8_t flags =
            (trap.discovered ? 1 : 0) | (trap.disabled ? 2 : 0) |
            (trap.triggered ? 4 : 0) | (trap.charging ? 8 : 0) |
            (trap.destroyed ? 16 : 0) |
            (trap.rogueDiscoveryAttempted ? 32 : 0) |
            (trap.manualPerceptionAttempted ? 64 : 0);
        if (!writer.u8(static_cast<uint8_t>(trap.id)) ||
            !writer.u8(static_cast<uint8_t>(trap.x)) ||
            !writer.u8(static_cast<uint8_t>(trap.y)) ||
            !writer.u8(static_cast<uint8_t>(trap.sourceX)) ||
            !writer.u8(static_cast<uint8_t>(trap.sourceY)) ||
            !writer.u8(static_cast<uint8_t>(trap.direction)) ||
            !writer.u8(trap.level) ||
            !writer.u16(static_cast<uint16_t>(trap.hp)) ||
            !writer.u8(flags) || !writer.u8(trap.chargingCombatRound) ||
            !writer.u8(static_cast<uint8_t>(trap.suspicion)) ||
            !writer.u8(trap.controlGroup)) return false;
    }

    uint8_t suspicionCount = 0;
    for (const SuspicionInstance& suspicion : room.suspicions)
        if (suspicion.type != SUSPICION_NONE) ++suspicionCount;
    if (!writer.u8(suspicionCount)) return false;
    for (const SuspicionInstance& suspicion : room.suspicions)
    {
        if (suspicion.type == SUSPICION_NONE) continue;
        if (!writer.u8(static_cast<uint8_t>(suspicion.type)) ||
            !writer.u8(static_cast<uint8_t>(suspicion.x)) ||
            !writer.u8(static_cast<uint8_t>(suspicion.y))) return false;
    }

    uint8_t furnitureCount = 0;
    for (const DungeonFurnitureInstance& furniture : room.furniture)
        if (furniture.type != FURNITURE_NONE) ++furnitureCount;
    if (!writer.u8(furnitureCount)) return false;
    for (const DungeonFurnitureInstance& furniture : room.furniture)
    {
        if (furniture.type == FURNITURE_NONE) continue;
        if (!writer.u8(static_cast<uint8_t>(furniture.type)) ||
            !writer.u8(static_cast<uint8_t>(furniture.x)) ||
            !writer.u8(static_cast<uint8_t>(furniture.y)) ||
            !writer.u16(static_cast<uint16_t>(furniture.hp))) return false;
    }

    if (!writer.u8(static_cast<uint8_t>(room.fountain.x)) ||
        !writer.u8(static_cast<uint8_t>(room.fountain.y)) ||
        !writer.u8((room.fountain.active ? 1 : 0) |
                   (room.fountain.used ? 2 : 0))) return false;

    for (BellToneID tone : room.bellPuzzle.sequence)
        if (!writer.u8(static_cast<uint8_t>(tone))) return false;
    if (!writer.u8(room.bellPuzzle.sequenceLength) ||
        !writer.u8(static_cast<uint8_t>(room.bellPuzzle.progress)) ||
        !writer.u8(room.bellPuzzle.lockedExitDirection) ||
        !writer.u8(static_cast<uint8_t>(room.bellPuzzle.runeX)) ||
        !writer.u8(static_cast<uint8_t>(room.bellPuzzle.runeY)) ||
        !writer.u8(static_cast<uint8_t>(room.bellPuzzle.keyX)) ||
        !writer.u8(static_cast<uint8_t>(room.bellPuzzle.keyY))) return false;

    if (!writer.u8(room.brazierPuzzle.brazierCount) ||
        !writer.u8(room.brazierPuzzle.litMask) ||
        !writer.u8(static_cast<uint8_t>(room.brazierPuzzle.progress)) ||
        !writer.u8(room.brazierPuzzle.lockedExitDirection) ||
        !writer.u8(static_cast<uint8_t>(room.brazierPuzzle.rowX)) ||
        !writer.u8(static_cast<uint8_t>(room.brazierPuzzle.rowY)) ||
        !writer.u8(static_cast<uint8_t>(room.brazierPuzzle.rewardX)) ||
        !writer.u8(static_cast<uint8_t>(room.brazierPuzzle.rewardY)))
        return false;

    for (uint8_t digit : room.numberPuzzle.digits)
        if (!writer.u8(digit)) return false;
    if (!writer.u8(static_cast<uint8_t>(room.numberPuzzle.rule)) ||
        !writer.u8(room.numberPuzzle.clueVariant) ||
        !writer.u8(room.numberPuzzle.minimumDigit) ||
        !writer.u8(room.numberPuzzle.maximumDigit) ||
        !writer.u8(static_cast<uint8_t>(room.numberPuzzle.progress)) ||
        !writer.u8(room.numberPuzzle.lockedExitDirection) ||
        !writer.u8(room.numberPuzzle.fieldX) ||
        !writer.u8(room.numberPuzzle.fieldY) ||
        !writer.u8(room.numberPuzzle.fieldWidth) ||
        !writer.u8(room.numberPuzzle.fieldHeight) ||
        !writer.u8(static_cast<uint8_t>(room.numberPuzzle.clueX)) ||
        !writer.u8(static_cast<uint8_t>(room.numberPuzzle.clueY)) ||
        !writer.u8(static_cast<uint8_t>(room.numberPuzzle.failedX)) ||
        !writer.u8(static_cast<uint8_t>(room.numberPuzzle.failedY)) ||
        !writer.u8(room.numberPuzzle.seedA) ||
        !writer.u8(room.numberPuzzle.seedB)) return false;
    for (uint8_t digit : room.numberPuzzle.requiredSequence)
        if (!writer.u8(digit)) return false;
    if (!writer.u8(room.numberPuzzle.requiredLength)) return false;

    size = writer.size();
    return true;
}

bool applyRoomDetailSnapshot(const uint8_t* source, size_t size)
{
    if (runtime.currentRoom >= dungeon.roomCount) return false;
    DungeonRoom& room = dungeon.rooms[runtime.currentRoom];
    BufferReader reader(source, size);
    uint8_t version = 0;
    uint8_t count = 0;
    if (!reader.u8(version) || version != 1 || !reader.u8(count) ||
        count > MAX_TRAPS_PER_ROOM) return false;
    for (TrapInstance& trap : room.traps) trap = TrapInstance{};
    for (uint8_t index = 0; index < count; ++index)
    {
        TrapInstance& trap = room.traps[index];
        uint8_t id = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        uint8_t sourceX = 0;
        uint8_t sourceY = 0;
        uint8_t direction = 0;
        uint16_t hp = 0;
        uint8_t flags = 0;
        uint8_t suspicion = 0;
        if (!reader.u8(id) || !reader.u8(x) || !reader.u8(y) ||
            !reader.u8(sourceX) || !reader.u8(sourceY) ||
            !reader.u8(direction) || !reader.u8(trap.level) ||
            !reader.u16(hp) || !reader.u8(flags) ||
            !reader.u8(trap.chargingCombatRound) ||
            !reader.u8(suspicion) || !reader.u8(trap.controlGroup) ||
            id > TRAP_COLOR_SPRAY || direction >= 8 ||
            suspicion > SUSPICION_DISTURBED_DUST) return false;
        trap.id = static_cast<TrapID>(id);
        trap.x = static_cast<int8_t>(x);
        trap.y = static_cast<int8_t>(y);
        trap.sourceX = static_cast<int8_t>(sourceX);
        trap.sourceY = static_cast<int8_t>(sourceY);
        trap.direction = static_cast<Direction>(direction);
        trap.hp = static_cast<int16_t>(hp);
        trap.discovered = (flags & 1) != 0;
        trap.disabled = (flags & 2) != 0;
        trap.triggered = (flags & 4) != 0;
        trap.charging = (flags & 8) != 0;
        trap.destroyed = (flags & 16) != 0;
        trap.rogueDiscoveryAttempted = (flags & 32) != 0;
        trap.manualPerceptionAttempted = (flags & 64) != 0;
        trap.suspicion = static_cast<SuspicionType>(suspicion);
        trap.chargingUntilMillis = 0;
    }

    if (!reader.u8(count) || count > MAX_SUSPICIONS_PER_ROOM) return false;
    for (SuspicionInstance& suspicion : room.suspicions)
        suspicion = SuspicionInstance{};
    for (uint8_t index = 0; index < count; ++index)
    {
        uint8_t type = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        if (!reader.u8(type) || !reader.u8(x) || !reader.u8(y) ||
            type > SUSPICION_DISTURBED_DUST) return false;
        room.suspicions[index].type = static_cast<SuspicionType>(type);
        room.suspicions[index].x = static_cast<int8_t>(x);
        room.suspicions[index].y = static_cast<int8_t>(y);
    }

    if (!reader.u8(count) || count > MAX_FURNITURE_PER_ROOM) return false;
    for (DungeonFurnitureInstance& furniture : room.furniture)
        furniture = DungeonFurnitureInstance{};
    for (uint8_t index = 0; index < count; ++index)
    {
        uint8_t type = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        uint16_t hp = 0;
        if (!reader.u8(type) || !reader.u8(x) || !reader.u8(y) ||
            !reader.u16(hp) || type > FURNITURE_BELL_HIGH) return false;
        room.furniture[index].type = static_cast<DungeonFurnitureType>(type);
        room.furniture[index].x = static_cast<int8_t>(x);
        room.furniture[index].y = static_cast<int8_t>(y);
        room.furniture[index].hp = static_cast<int16_t>(hp);
    }

    uint8_t fountainX = 0;
    uint8_t fountainY = 0;
    uint8_t fountainFlags = 0;
    if (!reader.u8(fountainX) || !reader.u8(fountainY) ||
        !reader.u8(fountainFlags) || fountainFlags > 3) return false;
    room.fountain.x = static_cast<int8_t>(fountainX);
    room.fountain.y = static_cast<int8_t>(fountainY);
    room.fountain.active = (fountainFlags & 1) != 0;
    room.fountain.used = (fountainFlags & 2) != 0;

    for (BellToneID& tone : room.bellPuzzle.sequence)
    {
        uint8_t value = 0;
        if (!reader.u8(value) || value >= BELL_TONE_COUNT) return false;
        tone = static_cast<BellToneID>(value);
    }
    uint8_t bellProgress = 0;
    uint8_t value = 0;
    if (!reader.u8(room.bellPuzzle.sequenceLength) ||
        room.bellPuzzle.sequenceLength > MAX_BELL_SEQUENCE ||
        !reader.u8(bellProgress) || bellProgress > BELL_PUZZLE_COMPLETE ||
        !reader.u8(room.bellPuzzle.lockedExitDirection) ||
        !reader.u8(value)) return false;
    room.bellPuzzle.progress = static_cast<BellPuzzleProgress>(bellProgress);
    room.bellPuzzle.runeX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.bellPuzzle.runeY = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.bellPuzzle.keyX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.bellPuzzle.keyY = static_cast<int8_t>(value);

    uint8_t brazierProgress = 0;
    if (!reader.u8(room.brazierPuzzle.brazierCount) ||
        room.brazierPuzzle.brazierCount > MAX_BRAZIER_PUZZLE_COUNT ||
        !reader.u8(room.brazierPuzzle.litMask) ||
        !reader.u8(brazierProgress) || brazierProgress > BRAZIER_PUZZLE_COMPLETE ||
        !reader.u8(room.brazierPuzzle.lockedExitDirection) ||
        !reader.u8(value)) return false;
    room.brazierPuzzle.progress =
        static_cast<BrazierPuzzleProgress>(brazierProgress);
    room.brazierPuzzle.rowX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.brazierPuzzle.rowY = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.brazierPuzzle.rewardX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.brazierPuzzle.rewardY = static_cast<int8_t>(value);

    for (uint8_t& digit : room.numberPuzzle.digits)
        if (!reader.u8(digit)) return false;
    uint8_t numberRule = 0;
    uint8_t numberProgress = 0;
    if (!reader.u8(numberRule) || numberRule >= NUMBER_RULE_COUNT ||
        !reader.u8(room.numberPuzzle.clueVariant) ||
        !reader.u8(room.numberPuzzle.minimumDigit) ||
        !reader.u8(room.numberPuzzle.maximumDigit) ||
        !reader.u8(numberProgress) || numberProgress > NUMBER_PUZZLE_COMPLETE ||
        !reader.u8(room.numberPuzzle.lockedExitDirection) ||
        !reader.u8(room.numberPuzzle.fieldX) ||
        !reader.u8(room.numberPuzzle.fieldY) ||
        !reader.u8(room.numberPuzzle.fieldWidth) ||
        !reader.u8(room.numberPuzzle.fieldHeight) ||
        !reader.u8(value)) return false;
    room.numberPuzzle.rule = static_cast<NumberTileRule>(numberRule);
    room.numberPuzzle.progress = static_cast<NumberPuzzleProgress>(numberProgress);
    room.numberPuzzle.clueX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.numberPuzzle.clueY = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.numberPuzzle.failedX = static_cast<int8_t>(value);
    if (!reader.u8(value)) return false;
    room.numberPuzzle.failedY = static_cast<int8_t>(value);
    if (!reader.u8(room.numberPuzzle.seedA) ||
        !reader.u8(room.numberPuzzle.seedB)) return false;
    for (uint8_t& digit : room.numberPuzzle.requiredSequence)
        if (!reader.u8(digit)) return false;
    if (!reader.u8(room.numberPuzzle.requiredLength) ||
        room.numberPuzzle.requiredLength > NUMBER_STATEFUL_MAX_STEPS)
        return false;
    return reader.finished();
}

bool encodeWorldSnapshot(uint8_t* destination, size_t capacity, size_t& size)
{
    BufferWriter writer(destination, capacity);
    const uint8_t width = static_cast<uint8_t>(getActiveMapWidth());
    const uint8_t height = static_cast<uint8_t>(getActiveMapHeight());
    if (!writer.u8(WORLD_SNAPSHOT_VERSION) || !writer.u8(width) ||
        !writer.u8(height)) return false;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    uint8_t transmittedEntities = 0;
    for (uint8_t index = 0; index < entityCount; ++index)
        if (entities[index].active && entities[index].type != ENTITY_PLAYER)
            ++transmittedEntities;
    if (!writer.u8(transmittedEntities)) return false;

    for (uint8_t y = 0; y < height; ++y)
        for (uint8_t x = 0; x < width; ++x)
            if (!writer.u8(static_cast<uint8_t>(getActiveMapTile(x, y))))
                return false;

    for (uint8_t index = 0; index < entityCount; ++index)
    {
        const Entity& entity = entities[index];
        if (!entity.active || entity.type == ENTITY_PLAYER) continue;
        uint16_t subtype = 0;
        uint8_t flags = 0;
        if (entity.type == ENTITY_MONSTER)
            subtype = static_cast<uint16_t>(entity.monsterID);
        else if (entity.type == ENTITY_NPC)
            subtype = static_cast<uint16_t>(entity.npcID);
        else if (entity.type == ENTITY_CHEST)
            flags = (entity.locked ? 1 : 0) | (entity.opened ? 2 : 0) |
                ((entity.loot.itemCount > 0 || entity.loot.gold > 0) ? 4 : 0);
        if (!writer.u8(static_cast<uint8_t>(entity.type)) ||
            !writer.u8(entity.x) || !writer.u8(entity.y) ||
            !writer.u16(subtype) || !writer.u8(flags) ||
            !writer.u16(static_cast<uint16_t>(entity.character.health.currentHP)) ||
            !writer.u16(static_cast<uint16_t>(entity.character.health.maxHP)))
            return false;
    }
    size = writer.size();
    return true;
}

bool applyWorldSnapshot(const uint8_t* source, size_t size)
{
    BufferReader reader(source, size);
    uint8_t version = 0;
    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t entityCount = 0;
    if (!reader.u8(version) || version != WORLD_SNAPSHOT_VERSION ||
        !reader.u8(width) || !reader.u8(height) || !reader.u8(entityCount) ||
        entityCount > MAX_ENTITIES - 1)
        return false;

    const uint8_t expectedWidth = runtime.type == MultiplayerActivityType::FOREST
        ? FOREST_WIDTH : ROOM_WIDTH;
    const uint8_t expectedHeight = runtime.type == MultiplayerActivityType::FOREST
        ? FOREST_HEIGHT : ROOM_HEIGHT;
    if (width != expectedWidth || height != expectedHeight) return false;

    if (runtime.type == MultiplayerActivityType::FOREST)
        beginAuthoritativeForest();
    else
    {
        if (runtime.currentRoom >= dungeon.roomCount) return false;
        abortCombat();
        dungeon.currentRoom = runtime.currentRoom;
        dungeon.loadedRoom = runtime.currentRoom;
        dungeon.entities = dungeon.activeDungeonEntities;
        dungeon.entityCount = 0;
        dungeon.roomRuntime[runtime.currentRoom].initialized = true;
        dungeon.roomRuntime[runtime.currentRoom].persistenceReady = false;
        gameState = GAME_DUNGEON;
    }

    for (uint8_t y = 0; y < height; ++y)
    {
        for (uint8_t x = 0; x < width; ++x)
        {
            uint8_t tile = 0;
            if (!reader.u8(tile) || tile > TILE_NUMBER_CLUE_PLAQUE) return false;
            if (runtime.type == MultiplayerActivityType::FOREST)
                setForestTileAuthoritative(x, y, static_cast<TileType>(tile));
            else
                dungeon.rooms[runtime.currentRoom].map.tiles[y][x] =
                    static_cast<TileType>(tile);
        }
    }

    Entity* entities = runtime.type == MultiplayerActivityType::FOREST
        ? forestEntities : dungeon.activeDungeonEntities;
    uint8_t& activeCount = runtime.type == MultiplayerActivityType::FOREST
        ? forestEntityCount : dungeon.entityCount;
    for (uint8_t index = 0; index < entityCount; ++index)
    {
        uint8_t typeValue = 0;
        uint8_t x = 0;
        uint8_t y = 0;
        uint16_t subtype = 0;
        uint8_t flags = 0;
        uint16_t currentHP = 0;
        uint16_t maxHP = 0;
        if (!reader.u8(typeValue) || !reader.u8(x) || !reader.u8(y) ||
            !reader.u16(subtype) || !reader.u8(flags) ||
            !reader.u16(currentHP) || !reader.u16(maxHP) ||
            typeValue == ENTITY_PLAYER || typeValue > ENTITY_RIDDLE_CAT ||
            x >= width || y >= height)
            return false;
        const EntityType type = static_cast<EntityType>(typeValue);
        Entity* entity = spawnEntity(entities, activeCount, type, x, y);
        if (entity == nullptr) return false;
        if (type == ENTITY_MONSTER &&
            !initializeMonsterDefinitionState(
                *entity, static_cast<MonsterID>(subtype))) return false;
        if (type == ENTITY_NPC &&
            !initializeNPCDefinitionState(*entity, static_cast<NPCID>(subtype)))
            return false;
        if (type == ENTITY_CHEST)
        {
            entity->locked = (flags & 1) != 0;
            entity->opened = (flags & 2) != 0;
            entity->sprite = entity->opened
                ? ((flags & 4) != 0 ? chestopenwith : chestopenwithout)
                : chestclosed;
        }
        if (type == ENTITY_RIDDLE_CAT)
        {
            entity->character.team = TEAM_NEUTRAL;
            entity->character.state = STATE_ALIVE;
            entity->sprite = bertramCat16x16;
        }
        entity->character.health.currentHP = static_cast<int16_t>(currentHP);
        entity->character.health.maxHP = static_cast<int16_t>(maxHP);
    }
    return reader.finished();
}

uint8_t chunkCountForSize(size_t size)
{
    return static_cast<uint8_t>(
        (size + SNAPSHOT_CHUNK_DATA_SIZE - 1) / SNAPSHOT_CHUNK_DATA_SIZE);
}

bool sendSnapshot(
    PlayerID destination,
    SnapshotType snapshotType,
    uint8_t roomID,
    uint16_t epoch,
    const uint8_t* data,
    size_t dataSize)
{
    const uint8_t totalChunks = chunkCountForSize(dataSize);
    if (totalChunks == 0 || totalChunks > MAX_SNAPSHOT_CHUNKS) return false;
    for (uint8_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex)
    {
        SnapshotChunkPayload chunk{};
        chunk.activityID = runtime.activityID;
        chunk.snapshotType = snapshotType;
        chunk.roomID = roomID;
        chunk.snapshotEpoch = epoch;
        chunk.chunkIndex = chunkIndex;
        chunk.totalChunks = totalChunks;
        const size_t offset = chunkIndex * SNAPSHOT_CHUNK_DATA_SIZE;
        const size_t remaining = dataSize - offset;
        chunk.payloadLength = static_cast<uint8_t>(
            remaining > SNAPSHOT_CHUNK_DATA_SIZE
                ? SNAPSHOT_CHUNK_DATA_SIZE : remaining);
        memcpy(chunk.payload, data + offset, chunk.payloadLength);
        uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
        size_t size = 0;
        uint8_t flags = NETWORK_FLAG_CONTROL_EVENT;
        if (chunkIndex == 0) flags |= NETWORK_FLAG_SNAPSHOT_BEGIN;
        if (chunkIndex + 1 == totalChunks) flags |= NETWORK_FLAG_SNAPSHOT_END;
        if (!encodeSnapshotChunk(chunk, bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination, NetworkPacketType::SNAPSHOT_CHUNK,
                          bytes, size, flags))
            return false;
    }
    return true;
}

bool buildGraphSnapshot(size_t& size)
{
    size = 0;
    return encodeDungeonGraph(runtime.snapshotReceiver.data,
                              sizeof(runtime.snapshotReceiver.data), size);
}

bool buildWorldSnapshot(size_t& size)
{
    size = 0;
    return encodeWorldSnapshot(runtime.snapshotReceiver.data,
                               sizeof(runtime.snapshotReceiver.data), size);
}

bool buildRoomDetailSnapshot(size_t& size)
{
    size = 0;
    return encodeRoomDetailSnapshot(runtime.snapshotReceiver.data,
                                    sizeof(runtime.snapshotReceiver.data), size);
}

bool sendSyncBundle(PlayerID destination)
{
    size_t graphSize = 0;
    size_t detailSize = 0;
    size_t worldSize = 0;
    const bool needsGraph = runtime.type == MultiplayerActivityType::DUNGEON &&
                            runtime.snapshotEpoch == 1;
    if (needsGraph && !buildGraphSnapshot(graphSize)) return false;
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        !buildRoomDetailSnapshot(detailSize)) return false;
    if (!buildWorldSnapshot(worldSize)) return false;

    ActivityPreparePayload prepare{};
    prepare.activityID = runtime.activityID;
    prepare.activityType = runtime.type;
    prepare.roomID = getCurrentRoomID();
    prepare.snapshotEpoch = runtime.snapshotEpoch;
    prepare.participantMask = runtime.participantMask;
    prepare.graphChunkCount = needsGraph ? chunkCountForSize(graphSize) : 0;
    prepare.detailChunkCount = runtime.type == MultiplayerActivityType::DUNGEON
        ? chunkCountForSize(detailSize) : 0;
    prepare.worldChunkCount = chunkCountForSize(worldSize);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (!encodeActivityPrepare(prepare, bytes, sizeof(bytes), size) ||
        !sendToPlayer(destination, NetworkPacketType::ACTIVITY_PREPARE,
                      bytes, size)) return false;

    if (runtime.type == MultiplayerActivityType::FOREST)
    {
        ActivityReadyPayload begin{};
        begin.activityID = runtime.activityID;
        begin.snapshotEpoch = runtime.snapshotEpoch;
        if (!encodeActivityReady(begin, bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination, NetworkPacketType::FOREST_BEGIN,
                          bytes, size)) return false;
    }
    else if (needsGraph)
    {
        DungeonBeginPayload begin{};
        begin.activityID = runtime.activityID;
        begin.roomCount = dungeon.roomCount;
        begin.entranceRoom = dungeon.currentRoom;
        begin.snapshotEpoch = runtime.snapshotEpoch;
        if (!encodeDungeonBegin(begin, bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination, NetworkPacketType::DUNGEON_BEGIN,
                          bytes, size)) return false;
    }
    else
    {
        RoomTransitionPayload transition{};
        transition.activityID = runtime.activityID;
        transition.roomID = dungeon.currentRoom;
        transition.entryDirection = runtime.transitionEntry;
        transition.snapshotEpoch = runtime.snapshotEpoch;
        if (!encodeRoomTransition(transition, bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination, NetworkPacketType::ROOM_TRANSITION,
                          bytes, size)) return false;
    }

    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        PlayerCharacterStatePayload character{};
        character.activityID = runtime.activityID;
        character.character = runtime.participants[playerID].character;
        if (!encodePlayerCharacterState(
                character, bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination,
                          NetworkPacketType::PLAYER_CHARACTER_STATE,
                          bytes, size)) return false;
    }

    if (needsGraph && (!buildGraphSnapshot(graphSize) ||
        !sendSnapshot(destination, SnapshotType::DUNGEON_GRAPH,
                      NETWORK_NO_ROOM, runtime.snapshotEpoch,
                      runtime.snapshotReceiver.data, graphSize))) return false;

    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        (!buildRoomDetailSnapshot(detailSize) ||
         !sendSnapshot(destination, SnapshotType::DUNGEON_ROOM_DETAIL,
                       getCurrentRoomID(), runtime.snapshotEpoch,
                       runtime.snapshotReceiver.data, detailSize))) return false;

    const SnapshotType worldType =
        runtime.type == MultiplayerActivityType::FOREST
            ? SnapshotType::FOREST_STATE : SnapshotType::DUNGEON_ROOM;
    if (!buildWorldSnapshot(worldSize) ||
        !sendSnapshot(destination, worldType, getCurrentRoomID(),
                      runtime.snapshotEpoch, runtime.snapshotReceiver.data,
                      worldSize)) return false;

    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        if (!encodePlayerSpawn(runtime.participants[playerID].spawn,
                               bytes, sizeof(bytes), size) ||
            !sendToPlayer(destination, NetworkPacketType::PLAYER_SPAWN,
                          bytes, size)) return false;
    }
    MP_DEBUGF("Player %u snapshot bundle sent, epoch %u\n",
              destination, runtime.snapshotEpoch);
    return true;
}

void enterTownLocally()
{
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* localEntity = entities != nullptr && isValidPlayerID(runtime.localPlayerID)
        ? getPlayerEntityByOwner(entities, count, runtime.localPlayerID)
        : getActiveMapPlayer();
    if (localEntity != nullptr) player = localEntity->character;
    abortCombat();
    if (multiplayerSession.isHost() && gameState == GAME_DUNGEON &&
        dungeon.runActive)
        suspendDungeonRun(dungeon);
    gameState = GAME_TOWN;
    townSelection = TOWN_STAY_HOME;
    setFullMapRedraw();
}

void endActivityLocally(const char* message)
{
    enterTownLocally();
    resetActivityState();
    if (message != nullptr) setGameMessage(message);
}

void sendActivityEndToParticipants()
{
    ActivityMemberPayload payload{};
    payload.activityID = runtime.activityID;
    payload.playerID = HOST_PLAYER_ID;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (!encodeActivityMember(payload, bytes, sizeof(bytes), size)) return;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (isParticipant(playerID))
            sendToPlayer(playerID, NetworkPacketType::ACTIVITY_END, bytes, size);
}

void sendActivityEndTo(PlayerID playerID)
{
    ActivityMemberPayload payload{};
    payload.activityID = runtime.activityID;
    payload.playerID = HOST_PLAYER_ID;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (encodeActivityMember(payload, bytes, sizeof(bytes), size))
        sendToPlayer(playerID, NetworkPacketType::ACTIVITY_END, bytes, size);
}

void broadcastPlayerDespawn(PlayerID playerID)
{
    ActivityMemberPayload payload{};
    payload.activityID = runtime.activityID;
    payload.playerID = playerID;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (encodeActivityMember(payload, bytes, sizeof(bytes), size))
        multiplayerSession.broadcastAuthoritativeGameplayPacket(
            NetworkPacketType::PLAYER_DESPAWN, bytes, size);
}

bool allInvitedPlayersResponded()
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        const Participant& participant = runtime.participants[playerID];
        if (participant.invited && !participant.responded) return false;
    }
    return true;
}

bool allParticipantsReady()
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (isParticipant(playerID) && !runtime.participants[playerID].ready)
            return false;
    return true;
}

void sendTravelInvites()
{
    TravelInvitePayload invite{};
    invite.activityID = runtime.activityID;
    invite.activityType = runtime.type;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (!encodeTravelInvite(invite, bytes, sizeof(bytes), size)) return;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        Participant& participant = runtime.participants[playerID];
        if (participant.invited && !participant.responded)
            sendToPlayer(playerID, NetworkPacketType::TRAVEL_INVITE,
                         bytes, size);
    }
    runtime.lastInviteAt = millis();
}

void beginHostLoading(uint32_t now)
{
    runtime.phase = ActivityPhase::LOADING;
    runtime.phaseStartedAt = now;
    runtime.snapshotEpoch = 1;
    runtime.participantMask |= playerBit(HOST_PLAYER_ID);
    if (runtime.type == MultiplayerActivityType::FOREST)
        enterForest();
    else
    {
        resetDungeonRun(dungeon);
        enterDungeon();
    }
    runtime.currentRoom = getCurrentRoomID();
    if (!spawnHostParticipants())
    {
        sendActivityEndToParticipants();
        endActivityLocally("Could not prepare multiplayer area.");
        return;
    }
    MP_DEBUG("Multiplayer area prepared");
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        Participant& participant = runtime.participants[playerID];
        if (!participant.participating) continue;
        participant.ready = false;
        participant.syncAttempts = 0;
        participant.lastSyncAt = 0;
    }
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Preparing shared dungeon..." : "Preparing shared forest...");
}

void broadcastActivityStart(uint32_t now)
{
    ActivityReadyPayload start{};
    start.activityID = runtime.activityID;
    start.snapshotEpoch = runtime.snapshotEpoch;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (encodeActivityReady(start, bytes, sizeof(bytes), size))
    {
        for (PlayerID playerID = 1;
             playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
            if (isParticipant(playerID))
                sendToPlayer(playerID, NetworkPacketType::ACTIVITY_START,
                             bytes, size);
    }
    runtime.lastStartAt = now;
}

void startHostActivity(uint32_t now)
{
    runtime.phase = ActivityPhase::ACTIVE;
    runtime.startRepeatsRemaining = START_REPEAT_COUNT;
    broadcastActivityStart(now);
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Party entered dungeon." : "Party entered forest.");
    MP_DEBUG("Activity started");
}

bool clientLoadComplete()
{
    if (!runtime.worldApplied &&
        !isSnapshotComplete(runtime.snapshotReceiver)) return false;
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        runtime.snapshotEpoch == 1 && !runtime.graphApplied) return false;
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        !runtime.detailApplied) return false;
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        const Participant& participant = runtime.participants[playerID];
        if (!participant.characterReceived || !participant.spawnReceived)
            return false;
    }
    return true;
}

void finishClientLoading()
{
    if (!clientLoadComplete()) return;
    if (runtime.worldApplied) return;
    if (!applyWorldSnapshot(runtime.snapshotReceiver.data,
                            runtime.snapshotReceiver.dataSize))
        return;
    runtime.worldApplied = true;
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        const Participant& participant = runtime.participants[playerID];
        if (spawnPlayerFromState(participant.character,
                                 participant.spawn.x,
                                 participant.spawn.y) == nullptr)
            return;
    }
    Entity* local = getPlayerEntityByOwner(
        runtime.type == MultiplayerActivityType::FOREST
            ? forestEntities : dungeon.activeDungeonEntities,
        runtime.type == MultiplayerActivityType::FOREST
            ? forestEntityCount : dungeon.entityCount,
        multiplayerSession.getLocalPlayerID());
    if (local != nullptr)
    {
        previousPlayerPosition.x = local->x;
        previousPlayerPosition.y = local->y;
    }
    setFullMapRedraw();
    sendActivityReady();
    runtime.lastReadyAt = millis();
    setGameMessage("Area ready. Waiting for host...");
}

void sendAuthoritativePosition(
    PlayerID playerID,
    uint16_t movementSequence,
    Direction facing)
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    Entity* entity = entities != nullptr
        ? getPlayerEntityByOwner(entities, entityCount, playerID) : nullptr;
    if (entity == nullptr) return;
    PlayerPositionPayload position{};
    position.activityID = runtime.activityID;
    position.playerID = playerID;
    position.roomID = getCurrentRoomID();
    position.x = entity->x;
    position.y = entity->y;
    position.facing = static_cast<uint8_t>(facing);
    position.movementSequence = movementSequence;
    uint8_t bytes[24] = {};
    size_t size = 0;
    if (encodePlayerPosition(position, bytes, sizeof(bytes), size))
        multiplayerSession.broadcastAuthoritativeGameplayPacket(
            NetworkPacketType::PLAYER_POSITION, bytes, size);
}

void beginRoomTransition(uint8_t entryDirection, uint32_t now)
{
    runtime.phase = ActivityPhase::LOADING;
    runtime.snapshotEpoch++;
    if (runtime.snapshotEpoch == 0) runtime.snapshotEpoch = 1;
    runtime.currentRoom = dungeon.currentRoom;
    runtime.transitionEntry = entryDirection;
    if (!spawnHostParticipants())
    {
        sendActivityEndToParticipants();
        endActivityLocally("Room transition failed.");
        return;
    }
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        Participant& participant = runtime.participants[playerID];
        participant.ready = false;
        participant.syncAttempts = 0;
        participant.lastSyncAt = 0;
    }
    runtime.phaseStartedAt = now;
    setGameMessage("Party entering next room...");
}

void applyPositionUpdate(const PlayerPositionPayload& position)
{
    if (position.activityID != runtime.activityID ||
        !isParticipant(position.playerID) ||
        position.x >= getActiveMapWidth() || position.y >= getActiveMapHeight() ||
        (runtime.type == MultiplayerActivityType::DUNGEON &&
         position.roomID != dungeon.currentRoom)) return;
    Participant& participant = runtime.participants[position.playerID];
    if (participant.hasMoveSequence &&
        !isNewerMovementSequence(position.movementSequence,
                                 participant.lastMoveSequence)) return;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* entity = entities != nullptr
        ? getPlayerEntityByOwner(entities, count, position.playerID) : nullptr;
    if (entity == nullptr) return;
    markEntityFootprintDirty(*entity);
    entity->x = position.x;
    entity->y = position.y;
    markEntityFootprintDirty(*entity);
    participant.lastMoveSequence = position.movementSequence;
    participant.hasMoveSequence = true;
    if (position.playerID == multiplayerSession.getLocalPlayerID())
        moveDirection = static_cast<Direction>(position.facing);
    needsRedraw = true;
}

void processTravelInvite(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::NONE || gameState != GAME_TOWN)
        return;
    TravelInvitePayload invite{};
    if (!decodeTravelInvite(payload, payloadSize, invite)) return;
    if (runtime.pendingInviteAccepted &&
        invite.activityID == runtime.activityID)
    {
        TravelResponsePayload response{};
        response.activityID = runtime.activityID;
        copyCharacterToNetwork(multiplayerSession.getLocalPlayerID(),
                               player, response.character);
        uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
        size_t size = 0;
        if (encodeTravelResponse(response, bytes, sizeof(bytes), size))
            sendToHost(NetworkPacketType::TRAVEL_ACCEPT, bytes, size);
        return;
    }
    if (runtime.pendingInviteDeclined &&
        invite.activityID == runtime.activityID)
    {
        TravelResponsePayload response{};
        response.activityID = runtime.activityID;
        copyCharacterToNetwork(multiplayerSession.getLocalPlayerID(),
                               player, response.character);
        uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
        size_t size = 0;
        if (encodeTravelResponse(response, bytes, sizeof(bytes), size))
            sendToHost(NetworkPacketType::TRAVEL_DECLINE, bytes, size);
        return;
    }
    if (invite.activityID != runtime.activityID)
    {
        runtime.pendingInviteAccepted = false;
        runtime.pendingInviteDeclined = false;
    }
    runtime.pendingInvite = true;
    runtime.pendingInviteAccepted = false;
    runtime.activityID = invite.activityID;
    runtime.type = invite.activityType;
    const SessionMember* host = multiplayerSession.getMember(HOST_PLAYER_ID);
    strncpy(runtime.pendingHostName,
            host != nullptr ? host->profile.displayName : "Host",
            sizeof(runtime.pendingHostName) - 1);
    openMultiplayerTravelInviteMenu();
    MP_DEBUG("Travel invitation received");
}

void processTravelResponse(
    PlayerID sender,
    NetworkPacketType packetType,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() ||
        runtime.phase != ActivityPhase::WAITING_FOR_RESPONSES ||
        !isValidPlayerID(sender) || sender == HOST_PLAYER_ID)
        return;
    TravelResponsePayload response{};
    if (!decodeTravelResponse(payload, payloadSize, response) ||
        response.activityID != runtime.activityID ||
        response.character.playerID != sender) return;
    Participant& participant = runtime.participants[sender];
    if (!participant.invited) return;
    participant.responded = true;
    participant.participating = packetType == NetworkPacketType::TRAVEL_ACCEPT;
    if (participant.participating)
    {
        participant.character = response.character;
        participant.characterReceived = true;
        runtime.participantMask |= playerBit(sender);
        MP_DEBUGF("Player %u accepted travel\n", sender);
    }
    else
        runtime.participantMask &= ~playerBit(sender);
}

void processActivityPrepare(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    ActivityPreparePayload prepare{};
    if (!decodeActivityPrepare(payload, payloadSize, prepare) ||
        prepare.activityID != runtime.activityID ||
        (prepare.participantMask &
         playerBit(multiplayerSession.getLocalPlayerID())) == 0 ||
        (runtime.snapshotEpoch != 0 &&
         !isNewerMovementSequence(prepare.snapshotEpoch,
                                  runtime.snapshotEpoch) &&
         prepare.snapshotEpoch != runtime.snapshotEpoch)) return;

    if (prepare.snapshotEpoch != runtime.snapshotEpoch)
    {
        resetSnapshotReceiver(runtime.snapshotReceiver);
        runtime.graphApplied = prepare.graphChunkCount == 0;
        runtime.detailApplied = prepare.detailChunkCount == 0;
        runtime.worldApplied = false;
        for (Participant& participant : runtime.participants)
        {
            participant.characterReceived = false;
            participant.spawnReceived = false;
        }
    }
    runtime.phase = ActivityPhase::LOADING;
    runtime.type = prepare.activityType;
    runtime.snapshotEpoch = prepare.snapshotEpoch;
    runtime.participantMask = prepare.participantMask;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        runtime.participants[playerID].participating =
            (prepare.participantMask & playerBit(playerID)) != 0;
    runtime.currentRoom = prepare.roomID;
    runtime.expectedGraphChunks = prepare.graphChunkCount;
    runtime.expectedDetailChunks = prepare.detailChunkCount;
    runtime.expectedWorldChunks = prepare.worldChunkCount;
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Joining Dungeon..." : "Joining Forest...");
}

void processSnapshotChunk(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING) return;
    SnapshotChunkPayload chunk{};
    if (!decodeSnapshotChunk(payload, payloadSize, chunk)) return;
    uint8_t expectedCount = 0;
    uint8_t expectedRoom = NETWORK_NO_ROOM;
    if (chunk.snapshotType == SnapshotType::DUNGEON_GRAPH)
    {
        if (runtime.graphApplied) return;
        expectedCount = runtime.expectedGraphChunks;
    }
    else if (chunk.snapshotType == SnapshotType::DUNGEON_ROOM_DETAIL)
    {
        if (!runtime.graphApplied || runtime.detailApplied) return;
        expectedCount = runtime.expectedDetailChunks;
        expectedRoom = runtime.currentRoom;
    }
    else
    {
        if (runtime.worldApplied) return;
        if (runtime.type == MultiplayerActivityType::DUNGEON &&
            (!runtime.graphApplied || !runtime.detailApplied)) return;
        const SnapshotType expectedType =
            runtime.type == MultiplayerActivityType::FOREST
                ? SnapshotType::FOREST_STATE : SnapshotType::DUNGEON_ROOM;
        if (chunk.snapshotType != expectedType) return;
        expectedCount = runtime.expectedWorldChunks;
        expectedRoom = runtime.currentRoom;
    }
    if (!acceptSnapshotChunk(runtime.snapshotReceiver, chunk, runtime.activityID,
                             expectedRoom, runtime.snapshotEpoch,
                             expectedCount)) return;
    if (chunk.snapshotType == SnapshotType::DUNGEON_GRAPH &&
        isSnapshotComplete(runtime.snapshotReceiver))
    {
        if (!applyDungeonGraph(runtime.snapshotReceiver.data,
                               runtime.snapshotReceiver.dataSize)) return;
        runtime.graphApplied = true;
        MP_DEBUG("Dungeon graph received");
        resetSnapshotReceiver(runtime.snapshotReceiver);
    }
    else if (chunk.snapshotType == SnapshotType::DUNGEON_ROOM_DETAIL &&
             isSnapshotComplete(runtime.snapshotReceiver))
    {
        if (!applyRoomDetailSnapshot(runtime.snapshotReceiver.data,
                                     runtime.snapshotReceiver.dataSize)) return;
        runtime.detailApplied = true;
        MP_DEBUG("Dungeon room detail received");
        resetSnapshotReceiver(runtime.snapshotReceiver);
    }
    finishClientLoading();
}

void processCharacterState(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    PlayerCharacterStatePayload state{};
    if (!decodePlayerCharacterState(payload, payloadSize, state) ||
        state.activityID != runtime.activityID ||
        !isParticipant(state.character.playerID)) return;
    Participant& participant = runtime.participants[state.character.playerID];
    participant.character = state.character;
    participant.characterReceived = true;
    finishClientLoading();
}

void processPlayerSpawn(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    PlayerSpawnPayload spawn{};
    if (!decodePlayerSpawn(payload, payloadSize, spawn) ||
        spawn.activityID != runtime.activityID || !isParticipant(spawn.playerID) ||
        spawn.roomID != runtime.currentRoom) return;
    Participant& participant = runtime.participants[spawn.playerID];
    participant.spawn = spawn;
    participant.spawnReceived = true;
    finishClientLoading();
}

void processActivityReady(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || runtime.phase != ActivityPhase::LOADING ||
        !isParticipant(sender)) return;
    ActivityReadyPayload ready{};
    if (!decodeActivityReady(payload, payloadSize, ready) ||
        ready.activityID != runtime.activityID ||
        ready.snapshotEpoch != runtime.snapshotEpoch) return;
    runtime.participants[sender].ready = true;
    MP_DEBUGF("Player %u ready, epoch %u\n", sender, runtime.snapshotEpoch);
}

void processActivityStart(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING || !clientLoadComplete()) return;
    ActivityReadyPayload start{};
    if (!decodeActivityReady(payload, payloadSize, start) ||
        start.activityID != runtime.activityID ||
        start.snapshotEpoch != runtime.snapshotEpoch) return;
    runtime.phase = ActivityPhase::ACTIVE;
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = false;
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Party entered dungeon." : "Party entered forest.");
}

void processActivityLeave(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || !isParticipant(sender) ||
        sender == HOST_PLAYER_ID) return;
    ActivityMemberPayload leave{};
    if (!decodeActivityMember(payload, payloadSize, leave) ||
        leave.activityID != runtime.activityID || leave.playerID != sender)
        return;
    removePlayerEntity(sender);
    runtime.participantMask &= ~playerBit(sender);
    runtime.participants[sender].participating = false;
    broadcastPlayerDespawn(sender);
    setGameMessage("An adventurer returned to Town.");
}

void processPlayerDespawn(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    ActivityMemberPayload despawn{};
    if (!decodeActivityMember(payload, payloadSize, despawn) ||
        despawn.activityID != runtime.activityID) return;
    removePlayerEntity(despawn.playerID);
    runtime.participantMask &= ~playerBit(despawn.playerID);
    runtime.participants[despawn.playerID].participating = false;
}

void processActivityEnd(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    ActivityMemberPayload end{};
    if (!decodeActivityMember(payload, payloadSize, end) ||
        end.activityID != runtime.activityID) return;
    endActivityLocally("The host ended the activity.");
}

void processMoveRequest(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || runtime.phase != ActivityPhase::ACTIVE ||
        !isParticipant(sender) || sender == HOST_PLAYER_ID) return;
    PlayerMoveRequestPayload request{};
    if (!decodePlayerMoveRequest(payload, payloadSize, request) ||
        request.activityID != runtime.activityID) return;
    MP_DEBUGF("Player %u move request seq %u\n",
              sender, request.clientMovementSequence);
    Participant& participant = runtime.participants[sender];
    if (participant.hasMoveSequence &&
        !isNewerMovementSequence(request.clientMovementSequence,
                                 participant.lastMoveSequence)) return;
    bool roomChanged = false;
    tryMovePlayerAuthoritative(
        dungeon, sender, static_cast<Direction>(request.direction),
        false, roomChanged);
    participant.lastMoveSequence = request.clientMovementSequence;
    participant.hasMoveSequence = true;
    sendAuthoritativePosition(
        sender, request.clientMovementSequence,
        static_cast<Direction>(request.direction));
    MP_DEBUGF("Player %u position broadcast seq %u\n",
              sender, request.clientMovementSequence);
}

void processPlayerPosition(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::ACTIVE) return;
    PlayerPositionPayload position{};
    if (decodePlayerPosition(payload, payloadSize, position))
        applyPositionUpdate(position);
}

void handleGameplayPacket(
    PlayerID sender,
    NetworkPacketType packetType,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    switch (packetType)
    {
        case NetworkPacketType::TRAVEL_INVITE:
            processTravelInvite(sender, payload, payloadSize);
            break;
        case NetworkPacketType::TRAVEL_ACCEPT:
        case NetworkPacketType::TRAVEL_DECLINE:
            processTravelResponse(sender, packetType, payload, payloadSize);
            break;
        case NetworkPacketType::ACTIVITY_PREPARE:
            processActivityPrepare(sender, payload, payloadSize);
            break;
        case NetworkPacketType::SNAPSHOT_CHUNK:
            processSnapshotChunk(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_CHARACTER_STATE:
            processCharacterState(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_SPAWN:
            processPlayerSpawn(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_DESPAWN:
            processPlayerDespawn(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ACTIVITY_READY:
        case NetworkPacketType::ROOM_READY:
            processActivityReady(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ACTIVITY_START:
            processActivityStart(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ACTIVITY_LEAVE:
            processActivityLeave(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ACTIVITY_END:
            processActivityEnd(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_MOVE_REQUEST:
            processMoveRequest(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_POSITION:
            processPlayerPosition(sender, payload, payloadSize);
            break;
        default:
            break;
    }
}

void removeDisconnectedParticipants()
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        Participant& participant = runtime.participants[playerID];
        if (!participant.participating ||
            multiplayerSession.getMember(playerID) != nullptr) continue;
        char message[48] = {};
        snprintf(message, sizeof(message), "%s disconnected.",
                 participant.character.displayName[0] != '\0'
                    ? participant.character.displayName : "Adventurer");
        removePlayerEntity(playerID);
        runtime.participantMask &= ~playerBit(playerID);
        participant.participating = false;
        if (multiplayerSession.isHost()) broadcastPlayerDespawn(playerID);
        setGameMessage(message);
    }
}
}

void initializeMultiplayerActivity()
{
    resetActivityState();
    multiplayerSession.setGameplayPacketHandler(handleGameplayPacket);
}

void updateMultiplayerActivity(uint32_t now)
{
    if (runtime.phase != ActivityPhase::NONE &&
        !multiplayerSession.isActive())
    {
        endActivityLocally("Host connection lost. Returned to Town.");
        return;
    }

    removeDisconnectedParticipants();

    if (multiplayerSession.isHost() &&
        runtime.phase == ActivityPhase::WAITING_FOR_RESPONSES)
    {
        if (allInvitedPlayersResponded() ||
            elapsed(now, runtime.phaseStartedAt, INVITE_TIMEOUT_MS))
        {
            for (PlayerID playerID = 1;
                 playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
            {
                Participant& participant = runtime.participants[playerID];
                if (participant.invited && !participant.responded)
                {
                    participant.responded = true;
                    participant.participating = false;
                }
            }
            beginHostLoading(now);
        }
        else if (runtime.lastInviteAt == 0 ||
                 elapsed(now, runtime.lastInviteAt, INVITE_RETRY_MS))
            sendTravelInvites();
    }

    if (multiplayerSession.isHost() && runtime.phase == ActivityPhase::LOADING)
    {
        for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        {
            Participant& participant = runtime.participants[playerID];
            if (!participant.participating || participant.ready) continue;
            if (participant.syncAttempts >= SNAPSHOT_RETRY_LIMIT)
            {
                removePlayerEntity(playerID);
                runtime.participantMask &= ~playerBit(playerID);
                participant.participating = false;
                sendActivityEndTo(playerID);
                broadcastPlayerDespawn(playerID);
                setGameMessage("A player failed to load the area.");
                continue;
            }
            if (participant.lastSyncAt == 0 ||
                elapsed(now, participant.lastSyncAt, SNAPSHOT_RETRY_MS))
            {
                sendSyncBundle(playerID);
                participant.lastSyncAt = now;
                participant.syncAttempts++;
            }
        }
        if (allParticipantsReady()) startHostActivity(now);
    }

    if (multiplayerSession.isClient() && runtime.phase == ActivityPhase::LOADING &&
        clientLoadComplete() &&
        (runtime.lastReadyAt == 0 ||
         elapsed(now, runtime.lastReadyAt, READY_RETRY_MS)))
    {
        sendActivityReady();
        runtime.lastReadyAt = now;
    }

    if (multiplayerSession.isHost() && runtime.phase == ActivityPhase::ACTIVE &&
        runtime.startRepeatsRemaining > 0 &&
        elapsed(now, runtime.lastStartAt, START_REPEAT_MS))
    {
        broadcastActivityStart(now);
        runtime.startRepeatsRemaining--;
    }
}

bool requestMultiplayerTravel(MultiplayerActivityType activityType)
{
    if (!multiplayerSession.isActive()) return false;
    if (runtime.phase != ActivityPhase::NONE)
    {
        setGameMessage("A multiplayer activity is already active.");
        return true;
    }
    if (!multiplayerSession.isHost())
    {
        setGameMessage("Only the party host can lead travel.");
        return true;
    }
    if (multiplayerSession.getConnectedPlayerCount() <= 1)
        return false;

    resetActivityState();
    runtime.phase = ActivityPhase::WAITING_FOR_RESPONSES;
    runtime.localPlayerID = HOST_PLAYER_ID;
    runtime.type = activityType;
    runtime.activityID = multiplayerSession.getSessionID() ^ millis() ^ esp_random();
    if (runtime.activityID == 0) runtime.activityID = 1;
    runtime.phaseStartedAt = millis();
    runtime.participantMask = playerBit(HOST_PLAYER_ID);
    runtime.participants[HOST_PLAYER_ID].participating = true;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (multiplayerSession.getMember(playerID) == nullptr) continue;
        runtime.participants[playerID].invited = true;
    }
    sendTravelInvites();
    MP_DEBUGF("Travel invite sent: %s\n",
              activityType == MultiplayerActivityType::DUNGEON
                  ? "Dungeon" : "Forest");
    setGameMessage(activityType == MultiplayerActivityType::DUNGEON
        ? "Dungeon invitation sent." : "Forest invitation sent.");
    return true;
}

bool hasPendingMultiplayerTravelInvite()
{
    return runtime.pendingInvite;
}

MultiplayerActivityType getPendingMultiplayerTravelType()
{
    return runtime.pendingInvite ? runtime.type : MultiplayerActivityType::NONE;
}

const char* getPendingMultiplayerTravelHostName()
{
    return runtime.pendingHostName;
}

void acceptPendingMultiplayerTravel()
{
    if (!runtime.pendingInvite || !multiplayerSession.isClient()) return;
    TravelResponsePayload response{};
    response.activityID = runtime.activityID;
    copyCharacterToNetwork(multiplayerSession.getLocalPlayerID(),
                           player, response.character);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodeTravelResponse(response, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::TRAVEL_ACCEPT, bytes, size);
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = true;
    runtime.pendingInviteDeclined = false;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    runtime.participantMask = playerBit(multiplayerSession.getLocalPlayerID());
    setGameMessage("Joining party activity...");
}

void declinePendingMultiplayerTravel()
{
    if (!runtime.pendingInvite || !multiplayerSession.isClient()) return;
    TravelResponsePayload response{};
    response.activityID = runtime.activityID;
    copyCharacterToNetwork(multiplayerSession.getLocalPlayerID(),
                           player, response.character);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodeTravelResponse(response, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::TRAVEL_DECLINE, bytes, size);
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = false;
    runtime.pendingInviteDeclined = true;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    runtime.participantMask = 0;
    runtime.type = MultiplayerActivityType::NONE;
    setGameMessage("Staying in Town. Party remains connected.");
}

bool isMultiplayerExplorationActive()
{
    return runtime.phase == ActivityPhase::ACTIVE;
}

bool isMultiplayerActivityLoading()
{
    return runtime.phase == ActivityPhase::LOADING ||
           runtime.phase == ActivityPhase::WAITING_FOR_RESPONSES;
}

bool isLocalActivityParticipant()
{
    return isParticipant(runtime.localPlayerID);
}

MultiplayerActivityType getMultiplayerActivityType()
{
    return runtime.type;
}

MultiplayerMemberLocation getMultiplayerMemberLocation(PlayerID playerID)
{
    if (multiplayerSession.getMember(playerID) == nullptr)
        return MultiplayerMemberLocation::DISCONNECTED;
    if (runtime.phase == ActivityPhase::LOADING && isParticipant(playerID))
        return MultiplayerMemberLocation::CONNECTING;
    if (runtime.phase == ActivityPhase::ACTIVE && isParticipant(playerID))
        return runtime.type == MultiplayerActivityType::FOREST
            ? MultiplayerMemberLocation::FOREST
            : MultiplayerMemberLocation::DUNGEON;
    return MultiplayerMemberLocation::TOWN;
}

const char* multiplayerMemberLocationName(MultiplayerMemberLocation location)
{
    switch (location)
    {
        case MultiplayerMemberLocation::TOWN: return "Town";
        case MultiplayerMemberLocation::CONNECTING: return "Joining";
        case MultiplayerMemberLocation::FOREST: return "Forest";
        case MultiplayerMemberLocation::DUNGEON: return "Dungeon";
        case MultiplayerMemberLocation::DISCONNECTED: return "Offline";
    }
    return "Unknown";
}

bool handleMultiplayerLocalMove(uint8_t direction, bool& moved)
{
    moved = false;
    if (!isMultiplayerExplorationActive() || !isLocalActivityParticipant())
        return false;
    if (direction >= 8) return true;
    const PlayerID localPlayerID = runtime.localPlayerID;
    Participant& participant = runtime.participants[localPlayerID];
    const uint16_t sequence = ++runtime.localMovementSequence;

    if (multiplayerSession.isHost())
    {
        refreshHostParticipantCharacters();
        bool roomChanged = false;
        moved = tryMovePlayerAuthoritative(
            dungeon, localPlayerID, static_cast<Direction>(direction),
            true, roomChanged);
        participant.lastMoveSequence = sequence;
        participant.hasMoveSequence = true;
        if (!roomChanged) refreshHostParticipantCharacters();
        if (roomChanged)
            beginRoomTransition(static_cast<uint8_t>(ENTRY_START), millis());
        else
            sendAuthoritativePosition(
                localPlayerID, sequence, static_cast<Direction>(direction));
        return true;
    }

    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* local = entities != nullptr
        ? getPlayerEntityByOwner(entities, count, localPlayerID) : nullptr;
    if (local == nullptr) return true;
    const int targetX = local->x + directionOffsets[direction].dx;
    const int targetY = local->y + directionOffsets[direction].dy;
    if (isInsideActiveMap(targetX, targetY))
    {
        const TileType tile = getActiveMapTile(targetX, targetY);
        const bool simpleTile = runtime.type == MultiplayerActivityType::FOREST
            ? tile != TILE_TREE && tile != TILE_WATER && tile != TILE_VOID
            : (tile == TILE_FLOOR || tile == TILE_RUBBLE);
        Entity* occupant = getEntityAt(
            entities, count, static_cast<uint8_t>(targetX),
            static_cast<uint8_t>(targetY));
        if (simpleTile && occupant == nullptr)
        {
            markEntityFootprintDirty(*local);
            local->x = static_cast<uint8_t>(targetX);
            local->y = static_cast<uint8_t>(targetY);
            markEntityFootprintDirty(*local);
            moved = true;
            needsRedraw = true;
        }
    }
    PlayerMoveRequestPayload request{};
    request.activityID = runtime.activityID;
    request.direction = direction;
    request.clientMovementSequence = sequence;
    uint8_t bytes[16] = {};
    size_t size = 0;
    if (encodePlayerMoveRequest(request, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::PLAYER_MOVE_REQUEST, bytes, size,
                   NETWORK_FLAG_NONE);
    return true;
}

void leaveMultiplayerActivity()
{
    if (runtime.phase == ActivityPhase::NONE) return;
    if (multiplayerSession.isHost())
    {
        sendActivityEndToParticipants();
        endActivityLocally("Party returned to Town.");
        return;
    }
    if (multiplayerSession.isClient() && isLocalActivityParticipant())
    {
        ActivityMemberPayload leave{};
        leave.activityID = runtime.activityID;
        leave.playerID = multiplayerSession.getLocalPlayerID();
        uint8_t bytes[16] = {};
        size_t size = 0;
        if (encodeActivityMember(leave, bytes, sizeof(bytes), size))
            sendToHost(NetworkPacketType::ACTIVITY_LEAVE, bytes, size);
    }
    endActivityLocally("Returned to Town. Party remains connected.");
}
