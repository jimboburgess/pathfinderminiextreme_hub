#include "multiplayer_activity.h"

#include <Arduino.h>
#include <string.h>

#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/bellpuzzle.h"
#include "dungeon/brazierpuzzle.h"
#include "dungeon/dungeon.h"
#include "dungeon/furniture.h"
#include "dungeon/monsterscripts.h"
#include "dungeon/numbertilepuzzle.h"
#include "dungeon/riddlepuzzle.h"
#include "dungeon/traps.h"
#include "forest/forest.h"
#include "characters/items.h"
#include "data/dice.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "graphics/npcsprites.h"
#include "graphics/sprites.h"
#include "graphics/tiles.h"
#include "input/menu.h"
#include "map/activemap.h"
#include "map/awareness.h"
#include "map/mapeffects.h"
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
constexpr uint32_t TRAVEL_RESPONSE_RETRY_MS = 750;
constexpr uint32_t TRAVEL_RESPONSE_TIMEOUT_MS = 10000;
constexpr uint32_t RELIABLE_ACK_TIMEOUT_MS = 180;
constexpr uint32_t RELIABLE_SEND_GAP_MS = 15;
constexpr uint32_t ACTIVITY_LOAD_TIMEOUT_MS = 30000;
constexpr uint8_t RELIABLE_RETRY_LIMIT = 12;
constexpr uint32_t READY_RETRY_MS = 500;
constexpr uint32_t START_REPEAT_MS = 700;
constexpr uint8_t START_REPEAT_COUNT = 4;
constexpr uint32_t DEBUG_SUMMARY_INTERVAL_MS = 5000;
constexpr uint8_t WORLD_SNAPSHOT_VERSION = 2;
constexpr uint8_t DUNGEON_GRAPH_VERSION = 1;
constexpr uint8_t WORLD_ENTITY_RECORD_SIZE = 10;

enum class ActivityPhase : uint8_t
{
    NONE,
    WAITING_FOR_RESPONSES,
    LOADING,
    ACTIVE
};

enum class SyncStage : uint8_t
{
    PREPARE,
    BEGIN,
    CHARACTERS,
    GRAPH,
    DETAIL,
    WORLD,
    SPAWNS,
    COMPLETE
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
    bool startAcked = false;
    bool roomLoading = false;
    bool transferIncludesGraph = false;
    bool transitionPending = false;
    bool hasTransitionSequence = false;
    uint16_t lastMoveSequence = 0;
    uint16_t lastTransitionSequence = 0;
    uint16_t transferEpoch = 0;
    uint8_t transferRoom = NETWORK_NO_ROOM;
    uint8_t transitionEntry = 0;
    uint8_t pendingTransitionDirection = 0;
    uint8_t pendingTransitionSourceRoom = NETWORK_NO_ROOM;
    uint32_t lastTransitionRequestAt = 0;
    uint32_t roomLoadStartedAt = 0;
    PlayerSpawnPayload spawn{};
    SyncStage syncStage = SyncStage::PREPARE;
    uint8_t syncItem = 0;
    uint8_t retryCount = 0;
    bool awaitingAck = false;
    uint32_t lastSendAt = 0;
    ActivityAckPayload expectedAck{};
};

struct ActivityRuntime
{
    ActivityPhase phase = ActivityPhase::NONE;
    MultiplayerActivityType type = MultiplayerActivityType::NONE;
    uint32_t activityID = 0;
    uint16_t snapshotEpoch = 0;
    uint8_t participantMask = 0;
    uint8_t visiblePlayerMask = 0;
    PlayerID localPlayerID = INVALID_PLAYER_ID;
    uint8_t currentRoom = NETWORK_NO_ROOM;
    uint8_t transitionEntry = 0;
    uint16_t localMovementSequence = 0;
    uint16_t nextSharedMessageSequence = 1;
    uint16_t lastSharedMessageSequence = 0;
    bool hasSharedMessageSequence = false;
    uint32_t phaseStartedAt = 0;
    uint32_t lastInviteAt = 0;
    uint32_t lastReadyAt = 0;
    uint32_t lastStartAt = 0;
    uint32_t lastReliableSendAt = 0;
    uint32_t travelResponseStartedAt = 0;
    uint32_t lastTravelResponseAt = 0;
    uint32_t lastLoadDebugAt = 0;
    uint32_t lastDebugSummaryAt = 0;
    uint8_t startRepeatsRemaining = 0;
    PlayerID nextTransferPlayer = 1;
    bool pendingInvite = false;
    bool pendingInviteAccepted = false;
    bool pendingInviteDeclined = false;
    bool graphApplied = false;
    bool detailApplied = false;
    bool worldApplied = false;
    bool beginReceived = false;
    bool clientRoomLoading = false;
    uint8_t expectedGraphChunks = 0;
    uint8_t expectedDetailChunks = 0;
    uint8_t expectedWorldChunks = 0;
    char pendingHostName[NETWORK_PLAYER_NAME_SIZE] = {};
    Participant participants[MAX_MULTIPLAYER_PLAYERS] = {};
    uint16_t roomRevisions[NETWORK_MAX_DUNGEON_ROOMS] = {};
    BoundedSnapshotReceiver snapshotReceiver{};
};

struct MultiplayerCombatSlot
{
    MultiplayerInitiativeEntry initiative{};
    uint8_t movementRemaining = 0;
    bool moveActionUsed = false;
    bool standardActionUsed = false;
    bool fullDefense = false;
};

struct MultiplayerCombatRuntime
{
    bool active = false;
    bool ending = false;
    uint16_t combatID = 0;
    uint16_t nextEventSequence = 1;
    uint16_t lastEventSequence = 0;
    bool hasEventSequence = false;
    uint8_t combatantCount = 0;
    uint8_t currentTurnIndex = 0;
    uint16_t round = 1;
    uint16_t localActionSequence = 0;
    uint16_t lastActionSequence[MAX_MULTIPLAYER_PLAYERS] = {};
    bool hasActionSequence[MAX_MULTIPLAYER_PLAYERS] = {};
    MultiplayerCombatSlot slots[MAX_COMBATANTS] = {};
    NetworkPacketType pendingPacketType = NetworkPacketType::ERROR_PACKET;
    uint16_t pendingEventSequence = 0;
    uint8_t pendingPlayerMask = 0;
    uint8_t retryCount = 0;
    uint32_t lastSendAt = 0;
    uint16_t pendingSize = 0;
    uint8_t pendingBytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
#if MULTIPLAYER_DEBUG
    uint8_t snapshotRetries = 0;
    uint8_t snapshotChunkDuplicates = 0;
    uint8_t snapshotChunkRejects = 0;
#endif
};

struct MultiplayerRoomEffectState
{
    bool initialized = false;
    MapEffect effects[MAX_MAP_EFFECTS] = {};
};

ActivityRuntime runtime;
NetworkCharacterState* participantCharacters = nullptr;
MultiplayerCombatRuntime* multiplayerCombat = nullptr;
MultiplayerRoomEffectState* multiplayerRoomEffects = nullptr;

const MultiplayerCombatantRef* currentCombatRef();
Entity* resolveVisibleCombatant(const MultiplayerCombatantRef& reference);

NetworkCharacterState& participantCharacter(PlayerID playerID)
{
    return participantCharacters[playerID];
}

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

uint8_t getPlayersInRoomMask(uint8_t roomID)
{
    uint8_t result = 0;
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (isParticipant(playerID) &&
            runtime.participants[playerID].spawn.roomID == roomID)
            result |= playerBit(playerID);
    return result;
}

uint8_t getCurrentRoomID()
{
    return runtime.type == MultiplayerActivityType::DUNGEON
        ? dungeon.currentRoom : NETWORK_NO_ROOM;
}

uint8_t getCombatRoomID()
{
    return runtime.type == MultiplayerActivityType::FOREST
        ? 0 : getCurrentRoomID();
}

void resetActivityState()
{
    runtime = ActivityRuntime{};
    if (multiplayerCombat != nullptr)
        *multiplayerCombat = MultiplayerCombatRuntime{};
    if (multiplayerRoomEffects != nullptr)
        memset(multiplayerRoomEffects, 0,
               sizeof(MultiplayerRoomEffectState) *
               NETWORK_MAX_DUNGEON_ROOMS);
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
    destination.characterState = static_cast<uint8_t>(source.state);
    destination.currentMP = static_cast<int16_t>(source.magic.currentMP);
    destination.maxMP = static_cast<int16_t>(source.magic.maxMP);
    destination.initiative = source.initiative;
    destination.trainedWeaponGroup =
        static_cast<uint8_t>(source.trainedWeaponGroup);
    destination.channelEnergyCurrent =
        source.classAbilities.channelEnergyCurrent;
    destination.channelEnergyMax = source.classAbilities.channelEnergyMax;
    const EquipmentSlot networkSlots[NETWORK_COMBAT_EQUIPMENT_SLOTS] = {
        SLOT_MELEE_WEAPON, SLOT_RANGED_WEAPON, SLOT_SHIELD, SLOT_ARMOR};
    for (uint8_t index = 0; index < NETWORK_COMBAT_EQUIPMENT_SLOTS; ++index)
    {
        const ItemInstance& item = source.equipment.equipped[networkSlots[index]];
        destination.equipment[index].itemID = static_cast<uint8_t>(item.itemID);
        destination.equipment[index].enhancementBonus = item.enhancementBonus;
        destination.equipment[index].weaponProperties = item.weaponProperties;
    }
    destination.knownAbilityCount = source.magic.knownAbilityCount <
        NETWORK_KNOWN_ABILITY_CAPACITY ? source.magic.knownAbilityCount :
        NETWORK_KNOWN_ABILITY_CAPACITY;
    for (uint8_t index = 0; index < destination.knownAbilityCount; ++index)
        destination.knownAbilities[index] =
            static_cast<uint16_t>(source.magic.knownAbilities[index]);
    destination.conditionCount = source.conditions.count <
        NETWORK_CONDITION_CAPACITY ? source.conditions.count :
        NETWORK_CONDITION_CAPACITY;
    for (uint8_t index = 0; index < destination.conditionCount; ++index)
    {
        const Condition& condition = source.conditions.conditions[index];
        destination.conditions[index].type =
            static_cast<uint8_t>(condition.type);
        destination.conditions[index].value =
            static_cast<int16_t>(condition.value);
        destination.conditions[index].roundsRemaining =
            static_cast<int16_t>(condition.roundsRemaining);
    }
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
    destination.state = source.characterState <= STATE_TURNED
        ? static_cast<CharacterState>(source.characterState)
        : (source.currentHP > 0 ? STATE_ALIVE : STATE_UNCONSCIOUS);
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
    destination.magic.currentMP = source.currentMP;
    destination.magic.maxMP = source.maxMP;
    destination.initiative = source.initiative;
    destination.trainedWeaponGroup =
        static_cast<WeaponGroup>(source.trainedWeaponGroup);
    destination.classAbilities.channelEnergyCurrent =
        source.channelEnergyCurrent;
    destination.classAbilities.channelEnergyMax = source.channelEnergyMax;
    const EquipmentSlot networkSlots[NETWORK_COMBAT_EQUIPMENT_SLOTS] = {
        SLOT_MELEE_WEAPON, SLOT_RANGED_WEAPON, SLOT_SHIELD, SLOT_ARMOR};
    for (uint8_t index = 0; index < NETWORK_COMBAT_EQUIPMENT_SLOTS; ++index)
    {
        const NetworkCharacterState::EquipmentItem& item =
            source.equipment[index];
        if (item.itemID >= ITEM_COUNT) continue;
        destination.equipment.equipped[networkSlots[index]] = {
            static_cast<ItemID>(item.itemID), item.enhancementBonus,
            item.weaponProperties};
    }
    destination.magic.knownAbilityCount = source.knownAbilityCount <
        MAX_KNOWN_ABILITIES ? source.knownAbilityCount : MAX_KNOWN_ABILITIES;
    for (uint8_t index = 0;
         index < destination.magic.knownAbilityCount; ++index)
        if (source.knownAbilities[index] < ABILITY_MAX)
            destination.magic.knownAbilities[index] =
                static_cast<AbilityID>(source.knownAbilities[index]);
    for (uint8_t index = 0; index < source.conditionCount; ++index)
    {
        const NetworkCharacterState::ConditionState& condition =
            source.conditions[index];
        if (condition.type == CONDITION_NONE ||
            condition.type >= CONDITION_MAX) continue;
        addCondition(destination, static_cast<ConditionType>(condition.type),
                     condition.value, condition.roundsRemaining);
    }
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

bool sendActivityReady(bool roomReady = false)
{
    ActivityReadyPayload ready{};
    ready.activityID = runtime.activityID;
    ready.snapshotEpoch = runtime.snapshotEpoch;
    uint8_t bytes[16] = {};
    size_t size = 0;
    return encodeActivityReady(ready, bytes, sizeof(bytes), size) &&
           sendToHost(roomReady ? NetworkPacketType::ROOM_READY
                                : NetworkPacketType::ACTIVITY_READY,
                      bytes, size);
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
    if (*count >= MAX_ENTITIES) return nullptr;
    Entity* result = &entities[(*count)++];
    *result = Entity{};
    result->active = true;
    result->type = ENTITY_PLAYER;
    result->x = x;
    result->y = y;
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
        if (!isParticipant(playerID) ||
            runtime.participants[playerID].spawn.roomID != getCurrentRoomID())
            continue;
        Entity* entity = getPlayerEntityByOwner(entities, count, playerID);
        if (entity != nullptr)
            copyCharacterToNetwork(
                playerID, entity->character,
                participantCharacter(playerID));
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
                           participantCharacter(HOST_PLAYER_ID));
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
            spawnPlayerFromState(participantCharacter(playerID), x, y) == nullptr)
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

void capturePlayersInLoadedDungeonRoom()
{
    if (!multiplayerSession.isHost() ||
        runtime.type != MultiplayerActivityType::DUNGEON ||
        dungeon.loadedRoom >= dungeon.roomCount) return;
    for (uint8_t index = 0; index < dungeon.entityCount; ++index)
    {
        Entity& entity = dungeon.activeDungeonEntities[index];
        if (!entity.active || entity.type != ENTITY_PLAYER ||
            !isParticipant(entity.ownerPlayerID)) continue;
        Participant& participant = runtime.participants[entity.ownerPlayerID];
        participant.spawn.roomID = dungeon.loadedRoom;
        participant.spawn.x = entity.x;
        participant.spawn.y = entity.y;
        copyCharacterToNetwork(entity.ownerPlayerID, entity.character,
                               participantCharacter(entity.ownerPlayerID));
        if (entity.ownerPlayerID == HOST_PLAYER_ID)
            player = entity.character;
    }
}

void checkAuthoritativeRoomCombat(PlayerID playerID)
{
    if (!multiplayerSession.isHost()) return;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* movingPlayer = entities != nullptr
        ? getPlayerEntityByOwner(entities, count, playerID) : nullptr;
    if (movingPlayer == nullptr || movingPlayer->character.state != STATE_ALIVE)
        return;
    for (uint8_t index = 0; index < count; ++index)
    {
        Entity& monster = entities[index];
        if (!monster.active || monster.type != ENTITY_MONSTER ||
            monster.character.state != STATE_ALIVE || monster.monster == nullptr)
            continue;
        const int distance = getEntityGridDistance(*movingPlayer, monster);
        const bool wasAware = monster.awareOfPlayer;
        if (distance > COMBAT_DETECTION_RANGE ||
            !tryMonsterDetectPlayer(monster, *movingPlayer)) continue;
        if (!wasAware)
        {
            char message[SHARED_MESSAGE_TEXT_SIZE] = {};
            snprintf(message, sizeof(message), "%s looks your way!",
                     getEntityName(&monster));
            publishSharedGameplayMessage(message, SharedMessageCategory::COMBAT);
        }
        startMultiplayerCombatFromCurrentRoom();
        return;
    }
}

void removeAllLoadedDungeonPlayers()
{
    for (uint8_t index = 0; index < dungeon.entityCount; ++index)
        if (dungeon.activeDungeonEntities[index].type == ENTITY_PLAYER)
            dungeon.activeDungeonEntities[index].active = false;
}

bool spawnPlayersForLoadedDungeonRoom(uint8_t roomID)
{
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID)) continue;
        const Participant& participant = runtime.participants[playerID];
        if (participant.spawn.roomID != roomID) continue;
        if (spawnPlayerFromState(participantCharacter(playerID),
                                 participant.spawn.x,
                                 participant.spawn.y) == nullptr)
            return false;
    }
    return true;
}

void captureMultiplayerRoomEffects(uint8_t roomID)
{
    if (multiplayerRoomEffects == nullptr ||
        roomID >= NETWORK_MAX_DUNGEON_ROOMS) return;
    MultiplayerRoomEffectState& state = multiplayerRoomEffects[roomID];
    memcpy(state.effects, activeMapEffects, sizeof(state.effects));
    state.initialized = true;
}

void restoreMultiplayerRoomEffects(uint8_t roomID)
{
    clearMapEffects();
    if (multiplayerRoomEffects == nullptr ||
        roomID >= NETWORK_MAX_DUNGEON_ROOMS ||
        !multiplayerRoomEffects[roomID].initialized) return;
    memcpy(activeMapEffects, multiplayerRoomEffects[roomID].effects,
           sizeof(multiplayerRoomEffects[roomID].effects));
}

bool activateAuthoritativeDungeonRoom(
    uint8_t roomID,
    RoomEntry entry,
    PlayerID enteringPlayer = INVALID_PLAYER_ID)
{
    if (!multiplayerSession.isHost() || roomID >= dungeon.roomCount)
        return false;
    if (dungeon.loadedRoom == roomID)
        return true;

    capturePlayersInLoadedDungeonRoom();
    if (dungeon.loadedRoom < NETWORK_MAX_DUNGEON_ROOMS)
        captureMultiplayerRoomEffects(dungeon.loadedRoom);
    const bool hostWasInLoadedRoom = isParticipant(HOST_PLAYER_ID) &&
        runtime.participants[HOST_PLAYER_ID].spawn.roomID == dungeon.loadedRoom;
    if (!hostWasInLoadedRoom) removeAllLoadedDungeonPlayers();

    dungeon.currentRoom = roomID;
    if (!loadRoom(dungeon, entry)) return false;
    restoreMultiplayerRoomEffects(roomID);
    Entity* attached = getPlayerEntity(
        dungeon.activeDungeonEntities, dungeon.entityCount);
    uint8_t entryX = attached != nullptr ? attached->x : ROOM_WIDTH / 2;
    uint8_t entryY = attached != nullptr ? attached->y : ROOM_HEIGHT / 2;
    removeAllLoadedDungeonPlayers();

    if (isValidPlayerID(enteringPlayer))
    {
        Participant& participant = runtime.participants[enteringPlayer];
        participant.spawn.roomID = roomID;
        participant.spawn.x = entryX;
        participant.spawn.y = entryY;
    }
    return spawnPlayersForLoadedDungeonRoom(roomID);
}

bool restoreHostDungeonRoom()
{
    const uint8_t hostRoom = runtime.participants[HOST_PLAYER_ID].spawn.roomID;
    if (hostRoom >= dungeon.roomCount) return false;
    return activateAuthoritativeDungeonRoom(hostRoom, ENTRY_START);
}

bool resolvePlayerDoorTransition(
    PlayerID playerID,
    Direction direction,
    uint8_t& destinationRoom,
    RoomEntry& entry)
{
    if (dungeon.currentRoom >= dungeon.roomCount || direction >= 8)
        return false;
    Entity* movingPlayer = getPlayerEntityByOwner(
        dungeon.activeDungeonEntities, dungeon.entityCount, playerID);
    if (movingPlayer == nullptr) return false;
    const int targetX = movingPlayer->x + directionOffsets[direction].dx;
    const int targetY = movingPlayer->y + directionOffsets[direction].dy;
    if (targetX < 0 || targetX >= ROOM_WIDTH ||
        targetY < 0 || targetY >= ROOM_HEIGHT ||
        dungeon.rooms[dungeon.currentRoom].map.tiles[targetY][targetX] !=
            TILE_DOOR) return false;

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    Direction doorDirection = DIR_NORTH;
    destinationRoom = NETWORK_NO_ROOM;
    if (targetY == 0)
    {
        destinationRoom = room.north;
        doorDirection = DIR_NORTH;
        entry = ENTRY_SOUTH;
    }
    else if (targetY == ROOM_HEIGHT - 1)
    {
        destinationRoom = room.south;
        doorDirection = DIR_SOUTH;
        entry = ENTRY_NORTH;
    }
    else if (targetX == 0)
    {
        destinationRoom = room.west;
        doorDirection = DIR_WEST;
        entry = ENTRY_EAST;
    }
    else if (targetX == ROOM_WIDTH - 1)
    {
        destinationRoom = room.east;
        doorDirection = DIR_EAST;
        entry = ENTRY_WEST;
    }
    if (destinationRoom >= dungeon.roomCount ||
        !tryUnlockCurrentRiddleExit(doorDirection) ||
        !tryUnlockCurrentBellExit(doorDirection) ||
        !tryUnlockCurrentNumberPuzzleExit(doorDirection) ||
        !tryUnlockCurrentBrazierPuzzleExit(doorDirection)) return false;
    if (dungeon.currentRoom == dungeon.bossRoom &&
        destinationRoom == dungeon.treasureRoom &&
        !dungeon.rooms[dungeon.bossRoom].completed) return false;
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

bool writeMapEffectSnapshot(BufferWriter& writer, const MapEffect& effect)
{
    if (effect.tileCount > MAX_MAP_EFFECT_TILES ||
        !writer.u8(static_cast<uint8_t>(effect.type)) ||
        !writer.u16(static_cast<uint16_t>(effect.sourceAbility)) ||
        !writer.u8(static_cast<uint8_t>(effect.x)) ||
        !writer.u8(static_cast<uint8_t>(effect.y)) ||
        !writer.u8(effect.radius) || !writer.u8(effect.roundsRemaining) ||
        !writer.u8(effect.expiresWithCombat ? 1 : 0) ||
        !writer.u8(static_cast<uint8_t>(effect.saveType)) ||
        !writer.u16(static_cast<uint16_t>(effect.saveDC)) ||
        !writer.u8(static_cast<uint8_t>(effect.conditionType)) ||
        !writer.u16(static_cast<uint16_t>(effect.conditionValue)) ||
        !writer.u8(effect.conditionDuration) ||
        !writer.u8(effect.tileCount)) return false;
    for (uint8_t index = 0; index < effect.tileCount; ++index)
        if (!writer.u8(static_cast<uint8_t>(effect.tiles[index].x)) ||
            !writer.u8(static_cast<uint8_t>(effect.tiles[index].y)))
            return false;
    return writer.u8(static_cast<uint8_t>(effect.damageType)) &&
           writer.u8(effect.damageDiceCount) &&
           writer.u8(effect.damageDiceSides) &&
           writer.u16(static_cast<uint16_t>(effect.flatDamage)) &&
           writer.u8(static_cast<uint8_t>(effect.damageSaveEffect));
}

bool readMapEffectSnapshot(BufferReader& reader, MapEffect& effect)
{
    uint8_t type = 0;
    uint16_t ability = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t expires = 0;
    uint8_t saveType = 0;
    uint16_t saveDC = 0;
    uint8_t conditionType = 0;
    uint16_t conditionValue = 0;
    uint8_t damageType = 0;
    uint16_t flatDamage = 0;
    uint8_t saveEffect = 0;
    if (!reader.u8(type) || !reader.u16(ability) ||
        !reader.u8(x) || !reader.u8(y) || !reader.u8(effect.radius) ||
        !reader.u8(effect.roundsRemaining) || !reader.u8(expires) ||
        !reader.u8(saveType) || !reader.u16(saveDC) ||
        !reader.u8(conditionType) || !reader.u16(conditionValue) ||
        !reader.u8(effect.conditionDuration) || !reader.u8(effect.tileCount) ||
        effect.tileCount > MAX_MAP_EFFECT_TILES) return false;
    effect.active = true;
    effect.type = static_cast<MapEffectType>(type);
    effect.sourceAbility = static_cast<AbilityID>(ability);
    effect.x = static_cast<int8_t>(x);
    effect.y = static_cast<int8_t>(y);
    effect.expiresWithCombat = expires != 0;
    effect.saveType = static_cast<SaveType>(saveType);
    effect.saveDC = static_cast<int16_t>(saveDC);
    effect.conditionType = static_cast<ConditionType>(conditionType);
    effect.conditionValue = static_cast<int16_t>(conditionValue);
    for (uint8_t index = 0; index < effect.tileCount; ++index)
    {
        if (!reader.u8(x) || !reader.u8(y)) return false;
        effect.tiles[index].x = static_cast<int8_t>(x);
        effect.tiles[index].y = static_cast<int8_t>(y);
    }
    if (!reader.u8(damageType) || !reader.u8(effect.damageDiceCount) ||
        !reader.u8(effect.damageDiceSides) || !reader.u16(flatDamage) ||
        !reader.u8(saveEffect)) return false;
    effect.damageType = static_cast<DamageType>(damageType);
    effect.flatDamage = static_cast<int16_t>(flatDamage);
    effect.damageSaveEffect = static_cast<SaveEffect>(saveEffect);
    return true;
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
    uint8_t effectCount = 0;
    for (const MapEffect& effect : activeMapEffects)
        if (effect.active) ++effectCount;
    if (!writer.u8(effectCount)) return false;
    for (const MapEffect& effect : activeMapEffects)
        if (effect.active && !writeMapEffectSnapshot(writer, effect))
            return false;
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
        if (!isMultiplayerCombatActive())
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
    uint8_t effectCount = 0;
    if (!reader.u8(effectCount) || effectCount > MAX_MAP_EFFECTS) return false;
    clearMapEffects();
    for (uint8_t index = 0; index < effectCount; ++index)
    {
        MapEffect effect{};
        if (!readMapEffectSnapshot(reader, effect) || addMapEffect(effect) == nullptr)
            return false;
    }
    return reader.finished();
}

uint8_t chunkCountForSize(size_t size)
{
    return static_cast<uint8_t>(
        (size + SNAPSHOT_CHUNK_DATA_SIZE - 1) / SNAPSHOT_CHUNK_DATA_SIZE);
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

ActivityAckPayload controlAck(NetworkPacketType packetType, uint8_t itemIndex = 0)
{
    ActivityAckPayload ack{};
    ack.activityID = runtime.activityID;
    ack.snapshotEpoch = runtime.snapshotEpoch;
    ack.packetType = packetType;
    ack.itemIndex = itemIndex;
    return ack;
}

bool findParticipantAtOrAfter(PlayerID start, PlayerID& result)
{
    for (PlayerID playerID = start; playerID < MAX_MULTIPLAYER_PLAYERS;
         ++playerID)
    {
        if (isParticipant(playerID))
        {
            result = playerID;
            return true;
        }
    }
    return false;
}

bool findRoomParticipantAtOrAfter(
    uint8_t roomID,
    PlayerID start,
    PlayerID& result)
{
    for (PlayerID playerID = start; playerID < MAX_MULTIPLAYER_PLAYERS;
         ++playerID)
    {
        if (isParticipant(playerID) &&
            runtime.participants[playerID].spawn.roomID == roomID)
        {
            result = playerID;
            return true;
        }
    }
    return false;
}

void normalizeSyncStage(Participant& participant)
{
    for (;;)
    {
        if (participant.syncStage == SyncStage::CHARACTERS ||
            participant.syncStage == SyncStage::SPAWNS)
        {
            PlayerID playerID = INVALID_PLAYER_ID;
            const bool found = participant.syncStage == SyncStage::CHARACTERS
                ? findParticipantAtOrAfter(participant.syncItem, playerID)
                : findRoomParticipantAtOrAfter(
                    participant.transferRoom, participant.syncItem, playerID);
            if (found)
            {
                participant.syncItem = playerID;
                return;
            }
            participant.syncStage = participant.syncStage == SyncStage::CHARACTERS
                ? SyncStage::GRAPH : SyncStage::COMPLETE;
            participant.syncItem = 0;
            continue;
        }
        if (participant.syncStage == SyncStage::GRAPH &&
            (runtime.type != MultiplayerActivityType::DUNGEON ||
             !participant.transferIncludesGraph))
        {
            participant.syncStage = SyncStage::DETAIL;
            participant.syncItem = 0;
            continue;
        }
        if (participant.syncStage == SyncStage::DETAIL &&
            runtime.type != MultiplayerActivityType::DUNGEON)
        {
            participant.syncStage = SyncStage::WORLD;
            participant.syncItem = 0;
            continue;
        }
        return;
    }
}

bool snapshotForStage(
    SyncStage stage,
    SnapshotType& type,
    uint8_t& roomID,
    size_t& size)
{
    roomID = NETWORK_NO_ROOM;
    if (stage == SyncStage::GRAPH)
    {
        type = SnapshotType::DUNGEON_GRAPH;
        return buildGraphSnapshot(size);
    }
    if (stage == SyncStage::DETAIL)
    {
        type = SnapshotType::DUNGEON_ROOM_DETAIL;
        roomID = getCurrentRoomID();
        return buildRoomDetailSnapshot(size);
    }
    type = runtime.type == MultiplayerActivityType::FOREST
        ? SnapshotType::FOREST_STATE : SnapshotType::DUNGEON_ROOM;
    roomID = getCurrentRoomID();
    return buildWorldSnapshot(size);
}

bool sendReliablePacket(
    PlayerID destination,
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    const ActivityAckPayload& expectedAck,
    uint8_t flags = NETWORK_FLAG_CONTROL_EVENT)
{
    if (!sendToPlayer(destination, packetType, payload, payloadSize, flags))
        return false;
    Participant& participant = runtime.participants[destination];
    participant.expectedAck = expectedAck;
    participant.awaitingAck = true;
    participant.lastSendAt = millis();
    runtime.lastReliableSendAt = participant.lastSendAt;
    return true;
}

bool sendTargetedActivityStart(PlayerID destination)
{
    Participant& participant = runtime.participants[destination];
    ActivityReadyPayload start{};
    start.activityID = runtime.activityID;
    start.snapshotEpoch = participant.transferEpoch;
    uint8_t bytes[16] = {};
    size_t size = 0;
    ActivityAckPayload ack = controlAck(NetworkPacketType::ACTIVITY_START);
    ack.snapshotEpoch = participant.transferEpoch;
    return encodeActivityReady(start, bytes, sizeof(bytes), size) &&
        sendReliablePacket(destination, NetworkPacketType::ACTIVITY_START,
                           bytes, size, ack);
}

bool sendReliableSnapshotChunk(PlayerID destination, Participant& participant)
{
    SnapshotType type = SnapshotType::FOREST_STATE;
    uint8_t roomID = NETWORK_NO_ROOM;
    size_t dataSize = 0;
    if (!snapshotForStage(participant.syncStage, type, roomID, dataSize))
        return false;
    const uint8_t totalChunks = chunkCountForSize(dataSize);
    if (totalChunks == 0 || totalChunks > MAX_SNAPSHOT_CHUNKS ||
        participant.syncItem >= totalChunks) return false;

    SnapshotChunkPayload chunk{};
    chunk.activityID = runtime.activityID;
    chunk.snapshotType = type;
    chunk.roomID = roomID;
    chunk.snapshotEpoch = participant.transferEpoch;
    chunk.chunkIndex = participant.syncItem;
    chunk.totalChunks = totalChunks;
    const size_t offset = chunk.chunkIndex * SNAPSHOT_CHUNK_DATA_SIZE;
    const size_t remaining = dataSize - offset;
    chunk.payloadLength = static_cast<uint8_t>(
        remaining > SNAPSHOT_CHUNK_DATA_SIZE
            ? SNAPSHOT_CHUNK_DATA_SIZE : remaining);
    memcpy(chunk.payload, runtime.snapshotReceiver.data + offset,
           chunk.payloadLength);

    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    uint8_t flags = NETWORK_FLAG_CONTROL_EVENT;
    if (chunk.chunkIndex == 0) flags |= NETWORK_FLAG_SNAPSHOT_BEGIN;
    if (chunk.chunkIndex + 1 == totalChunks) flags |= NETWORK_FLAG_SNAPSHOT_END;
    ActivityAckPayload ack = controlAck(NetworkPacketType::SNAPSHOT_CHUNK);
    ack.snapshotEpoch = participant.transferEpoch;
    ack.snapshotType = type;
    ack.roomID = roomID;
    ack.chunkIndex = chunk.chunkIndex;
    if (!encodeSnapshotChunk(chunk, bytes, sizeof(bytes), size) ||
        !sendReliablePacket(destination, NetworkPacketType::SNAPSHOT_CHUNK,
                            bytes, size, ack, flags)) return false;
    MP_DEBUGF("Snapshot sent type=%u epoch=%u chunk=%u/%u player=%u\n",
              static_cast<unsigned>(type), participant.transferEpoch,
              chunk.chunkIndex + 1, totalChunks, destination);
    return true;
}

bool sendCurrentReliableItemInActiveRoom(PlayerID destination)
{
    Participant& participant = runtime.participants[destination];
    normalizeSyncStage(participant);
    if (participant.syncStage == SyncStage::COMPLETE) return true;

    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (participant.syncStage == SyncStage::PREPARE)
    {
        size_t graphSize = 0;
        size_t detailSize = 0;
        size_t worldSize = 0;
        const bool needsGraph = runtime.type == MultiplayerActivityType::DUNGEON &&
                                participant.transferIncludesGraph;
        if ((needsGraph && !buildGraphSnapshot(graphSize)) ||
            (runtime.type == MultiplayerActivityType::DUNGEON &&
             !buildRoomDetailSnapshot(detailSize)) ||
            !buildWorldSnapshot(worldSize)) return false;
        ActivityPreparePayload prepare{};
        prepare.activityID = runtime.activityID;
        prepare.activityType = runtime.type;
        prepare.roomID = participant.transferRoom;
        prepare.snapshotEpoch = participant.transferEpoch;
        prepare.participantMask = runtime.participantMask;
        prepare.roomPlayerMask = getPlayersInRoomMask(prepare.roomID);
        prepare.graphChunkCount = needsGraph ? chunkCountForSize(graphSize) : 0;
        prepare.detailChunkCount = runtime.type == MultiplayerActivityType::DUNGEON
            ? chunkCountForSize(detailSize) : 0;
        prepare.worldChunkCount = chunkCountForSize(worldSize);
        if (!encodeActivityPrepare(prepare, bytes, sizeof(bytes), size))
            return false;
        ActivityAckPayload ack = controlAck(NetworkPacketType::ACTIVITY_PREPARE);
        ack.snapshotEpoch = participant.transferEpoch;
        return sendReliablePacket(destination,
            NetworkPacketType::ACTIVITY_PREPARE, bytes, size, ack);
    }

    if (participant.syncStage == SyncStage::BEGIN)
    {
        if (runtime.type == MultiplayerActivityType::FOREST)
        {
            ActivityReadyPayload begin{};
            begin.activityID = runtime.activityID;
            begin.snapshotEpoch = participant.transferEpoch;
            ActivityAckPayload ack = controlAck(NetworkPacketType::FOREST_BEGIN);
            ack.snapshotEpoch = participant.transferEpoch;
            return encodeActivityReady(begin, bytes, sizeof(bytes), size) &&
                sendReliablePacket(destination, NetworkPacketType::FOREST_BEGIN,
                    bytes, size, ack);
        }
        if (participant.transferIncludesGraph)
        {
            DungeonBeginPayload begin{};
            begin.activityID = runtime.activityID;
            begin.roomCount = dungeon.roomCount;
            begin.entranceRoom = participant.transferRoom;
            begin.snapshotEpoch = participant.transferEpoch;
            ActivityAckPayload ack = controlAck(NetworkPacketType::DUNGEON_BEGIN);
            ack.snapshotEpoch = participant.transferEpoch;
            return encodeDungeonBegin(begin, bytes, sizeof(bytes), size) &&
                sendReliablePacket(destination, NetworkPacketType::DUNGEON_BEGIN,
                    bytes, size, ack);
        }
        RoomTransitionPayload transition{};
        transition.activityID = runtime.activityID;
        transition.roomID = participant.transferRoom;
        transition.entryDirection = participant.transitionEntry;
        transition.snapshotEpoch = participant.transferEpoch;
        ActivityAckPayload ack = controlAck(NetworkPacketType::ROOM_TRANSITION);
        ack.snapshotEpoch = participant.transferEpoch;
        return encodeRoomTransition(transition, bytes, sizeof(bytes), size) &&
            sendReliablePacket(destination, NetworkPacketType::ROOM_TRANSITION,
                bytes, size, ack);
    }

    if (participant.syncStage == SyncStage::CHARACTERS)
    {
        const PlayerID playerID = participant.syncItem;
        PlayerCharacterStatePayload character{};
        character.activityID = runtime.activityID;
        character.character = participantCharacter(playerID);
        if (!encodePlayerCharacterState(character, bytes, sizeof(bytes), size))
            return false;
        ActivityAckPayload ack = controlAck(
            NetworkPacketType::PLAYER_CHARACTER_STATE, playerID);
        ack.snapshotEpoch = participant.transferEpoch;
        return sendReliablePacket(destination,
            NetworkPacketType::PLAYER_CHARACTER_STATE, bytes, size, ack);
    }

    if (participant.syncStage == SyncStage::GRAPH ||
        participant.syncStage == SyncStage::DETAIL ||
        participant.syncStage == SyncStage::WORLD)
        return sendReliableSnapshotChunk(destination, participant);

    const PlayerID playerID = participant.syncItem;
    ActivityAckPayload ack = controlAck(NetworkPacketType::PLAYER_SPAWN, playerID);
    ack.snapshotEpoch = participant.transferEpoch;
    return encodePlayerSpawn(runtime.participants[playerID].spawn,
                             bytes, sizeof(bytes), size) &&
        sendReliablePacket(destination, NetworkPacketType::PLAYER_SPAWN,
            bytes, size, ack);
}

bool sendCurrentReliableItem(PlayerID destination)
{
    Participant& participant = runtime.participants[destination];
    if (runtime.type != MultiplayerActivityType::DUNGEON ||
        participant.transferRoom == dungeon.loadedRoom)
        return sendCurrentReliableItemInActiveRoom(destination);

    const uint8_t restoreRoom =
        runtime.participants[HOST_PLAYER_ID].spawn.roomID;
    if (!activateAuthoritativeDungeonRoom(
            participant.transferRoom, ENTRY_START)) return false;
    const bool sent = sendCurrentReliableItemInActiveRoom(destination);
    const bool restored = activateAuthoritativeDungeonRoom(
        restoreRoom, ENTRY_START);
    return sent && restored;
}

void advanceReliableTransfer(Participant& participant)
{
    switch (participant.syncStage)
    {
        case SyncStage::PREPARE:
            participant.syncStage = SyncStage::BEGIN;
            break;
        case SyncStage::BEGIN:
            participant.syncStage = SyncStage::CHARACTERS;
            participant.syncItem = 0;
            break;
        case SyncStage::CHARACTERS:
            ++participant.syncItem;
            break;
        case SyncStage::GRAPH:
        case SyncStage::DETAIL:
        case SyncStage::WORLD:
        {
            SnapshotType type = SnapshotType::FOREST_STATE;
            uint8_t roomID = NETWORK_NO_ROOM;
            size_t dataSize = 0;
            if (!snapshotForStage(participant.syncStage, type, roomID, dataSize))
            {
                participant.syncStage = SyncStage::COMPLETE;
                break;
            }
            ++participant.syncItem;
            if (participant.syncItem >= chunkCountForSize(dataSize))
            {
                participant.syncStage = participant.syncStage == SyncStage::GRAPH
                    ? SyncStage::DETAIL
                    : participant.syncStage == SyncStage::DETAIL
                        ? SyncStage::WORLD : SyncStage::SPAWNS;
                participant.syncItem = 0;
            }
            break;
        }
        case SyncStage::SPAWNS:
            ++participant.syncItem;
            break;
        case SyncStage::COMPLETE:
            break;
    }
    participant.awaitingAck = false;
    participant.retryCount = 0;
    normalizeSyncStage(participant);
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

bool hasReliablePacketInFlight()
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (runtime.participants[playerID].participating &&
            runtime.participants[playerID].awaitingAck)
            return true;
    return false;
}

bool hasParticipantRoomLoading()
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (runtime.participants[playerID].participating &&
            runtime.participants[playerID].roomLoading)
            return true;
    return false;
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
    if (runtime.currentRoom < NETWORK_MAX_DUNGEON_ROOMS)
        runtime.roomRevisions[runtime.currentRoom] = 1;
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
        participant.transferRoom = runtime.currentRoom;
        participant.transferEpoch = runtime.snapshotEpoch;
        participant.transferIncludesGraph =
            runtime.type == MultiplayerActivityType::DUNGEON;
        participant.syncStage = SyncStage::PREPARE;
        participant.syncItem = 0;
        participant.retryCount = 0;
        participant.awaitingAck = false;
        participant.lastSendAt = 0;
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
            {
                if (runtime.participants[playerID].startAcked) continue;
                sendToPlayer(playerID, NetworkPacketType::ACTIVITY_START,
                             bytes, size);
            }
    }
    runtime.lastStartAt = now;
}

void startHostActivity(uint32_t now)
{
    runtime.phase = ActivityPhase::ACTIVE;
    runtime.startRepeatsRemaining = START_REPEAT_COUNT;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        runtime.participants[playerID].startAcked = false;
    broadcastActivityStart(now);
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Party entered dungeon." : "Party entered forest.");
    MP_DEBUG("Activity started");
}

bool clientLoadComplete()
{
    if (!runtime.beginReceived) return false;
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
        if (!participant.characterReceived ||
            ((runtime.visiblePlayerMask & playerBit(playerID)) != 0 &&
             !participant.spawnReceived))
            return false;
    }
    return true;
}

void logClientLoadBlockers(uint32_t now)
{
#if MULTIPLAYER_DEBUG
    if (!elapsed(now, runtime.lastLoadDebugAt, 1000)) return;
    runtime.lastLoadDebugAt = now;
    MP_DEBUGF("Load incomplete: begin=%s graph=%s detail=%s world=%s mask=%02X/%02X\n",
              runtime.beginReceived ? "ready" : "missing",
              runtime.graphApplied ? "ready" : "missing",
              runtime.detailApplied ? "ready" : "missing",
              runtime.worldApplied ? "ready" :
                  isSnapshotComplete(runtime.snapshotReceiver)
                      ? "complete-not-applied" : "missing",
              runtime.snapshotReceiver.receivedMask,
              runtime.snapshotReceiver.totalChunks == 0 ? 0 :
                  static_cast<unsigned>((1u << runtime.snapshotReceiver.totalChunks) - 1u));
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID) ||
            (runtime.visiblePlayerMask & playerBit(playerID)) == 0) continue;
        MP_DEBUGF("  P%u character=%s spawn=%s\n", playerID,
                  runtime.participants[playerID].characterReceived
                      ? "ready" : "missing",
                  runtime.participants[playerID].spawnReceived
                      ? "ready" : "missing");
    }
#else
    (void)now;
#endif
}

void finishClientLoading()
{
    if (!clientLoadComplete()) return;
    if (runtime.worldApplied) return;
    if (!applyWorldSnapshot(runtime.snapshotReceiver.data,
                            runtime.snapshotReceiver.dataSize))
        return;
    runtime.worldApplied = true;
    MP_DEBUG("World snapshot complete and applied");
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID) ||
            (runtime.visiblePlayerMask & playerBit(playerID)) == 0) continue;
        const Participant& participant = runtime.participants[playerID];
        if (spawnPlayerFromState(participantCharacter(playerID),
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
    sendActivityReady(runtime.clientRoomLoading);
    runtime.lastReadyAt = millis();
    setGameMessage("Area ready. Waiting for host...");
    MP_DEBUG("ACTIVITY_READY sent");
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

void sendPlayerRoomChangeTo(PlayerID destination, PlayerID movingPlayer)
{
    const Participant& participant = runtime.participants[movingPlayer];
    PlayerRoomChangePayload change{};
    change.activityID = runtime.activityID;
    change.playerID = movingPlayer;
    change.roomID = participant.spawn.roomID;
    change.entryDirection = participant.transitionEntry;
    change.x = participant.spawn.x;
    change.y = participant.spawn.y;
    change.movementSequence = participant.lastMoveSequence;
    uint8_t bytes[PLAYER_ROOM_CHANGE_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodePlayerRoomChange(change, bytes, sizeof(bytes), size))
        sendToPlayer(destination, NetworkPacketType::PLAYER_ROOM_CHANGE,
                     bytes, size);
}

void announcePlayerRoomChange(PlayerID movingPlayer)
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (isParticipant(playerID) && playerID != movingPlayer)
            sendPlayerRoomChangeTo(playerID, movingPlayer);
}

bool beginIndependentRoomTransition(
    PlayerID movingPlayer,
    uint8_t destinationRoom,
    RoomEntry entry,
    uint16_t transitionSequence,
    uint32_t now)
{
    if (!multiplayerSession.isHost() || !isParticipant(movingPlayer) ||
        destinationRoom >= dungeon.roomCount) return false;
    Participant& participant = runtime.participants[movingPlayer];
    const uint8_t sourceRoom = participant.spawn.roomID;
    if (!activateAuthoritativeDungeonRoom(
            destinationRoom, entry, movingPlayer))
    {
        restoreHostDungeonRoom();
        return false;
    }
    participant.lastTransitionSequence = transitionSequence;
    participant.transitionPending = false;
    participant.transitionEntry = static_cast<uint8_t>(entry);
    if (runtime.roomRevisions[destinationRoom] == 0)
        runtime.roomRevisions[destinationRoom] = 1;
    capturePlayersInLoadedDungeonRoom();
    checkAuthoritativeRoomCombat(movingPlayer);
    if (multiplayerCombat != nullptr && multiplayerCombat->active)
        for (uint8_t index = 0; index < multiplayerCombat->combatantCount;
             ++index)
        {
            MultiplayerCombatantRef& reference =
                multiplayerCombat->slots[index].initiative.combatant;
            if (reference.kind == MultiplayerCombatantKind::PLAYER &&
                reference.playerID == movingPlayer)
            {
                reference.roomID = destinationRoom;
                if (multiplayerCombat->slots[index].movementRemaining > 0)
                    --multiplayerCombat->slots[index].movementRemaining;
                Entity* movedEntity = resolveVisibleCombatant(reference);
                if (movedEntity != nullptr)
                {
                    movedEntity->turn.turnActive = true;
                    movedEntity->turn.movementRemaining =
                        multiplayerCombat->slots[index].movementRemaining;
                }
            }
        }

    announcePlayerRoomChange(movingPlayer);
    char message[64] = {};
    snprintf(message, sizeof(message), "%s moves to Room %u.",
             participantCharacter(movingPlayer).displayName[0] != '\0'
                ? participantCharacter(movingPlayer).displayName
                : "Adventurer",
             static_cast<unsigned>(destinationRoom + 1));
    publishSharedGameplayMessage(message, SharedMessageCategory::ROOM);
    MP_DEBUGF("Player %u authoritative room %u -> %u\n",
              movingPlayer, sourceRoom, destinationRoom);

    if (movingPlayer == HOST_PLAYER_ID)
    {
        runtime.currentRoom = destinationRoom;
        dungeon.currentRoom = destinationRoom;
        setFullMapRedraw();
        return true;
    }

    if (!restoreHostDungeonRoom()) return false;
    participant.roomLoading = true;
    participant.ready = false;
    participant.transferRoom = destinationRoom;
    participant.transferEpoch = runtime.roomRevisions[destinationRoom];
    participant.transferIncludesGraph = false;
    participant.roomLoadStartedAt = now;
    participant.syncStage = SyncStage::PREPARE;
    participant.syncItem = 0;
    participant.retryCount = 0;
    participant.awaitingAck = false;
    participant.lastSendAt = 0;
    MP_DEBUGF("Sending Room %u revision %u to Player %u\n",
              destinationRoom, participant.transferEpoch, movingPlayer);
    return true;
}

bool localMoveTargetsDungeonDoor(Direction direction)
{
    if (runtime.type != MultiplayerActivityType::DUNGEON || direction >= 8)
        return false;
    Entity* local = getPlayerEntityByOwner(
        dungeon.activeDungeonEntities, dungeon.entityCount,
        runtime.localPlayerID);
    if (local == nullptr) return false;
    const int targetX = local->x + directionOffsets[direction].dx;
    const int targetY = local->y + directionOffsets[direction].dy;
    return targetX >= 0 && targetX < ROOM_WIDTH &&
           targetY >= 0 && targetY < ROOM_HEIGHT &&
           dungeon.rooms[dungeon.currentRoom].map.tiles[targetY][targetX] ==
               TILE_DOOR;
}

void sendPendingRoomTransitionRequest(Participant& participant, uint32_t now)
{
    PlayerRoomTransitionRequestPayload request{};
    request.activityID = runtime.activityID;
    request.sourceRoomID = participant.pendingTransitionSourceRoom;
    request.direction = participant.pendingTransitionDirection;
    request.transitionSequence = participant.lastTransitionSequence;
    uint8_t bytes[PLAYER_ROOM_TRANSITION_REQUEST_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodePlayerRoomTransitionRequest(
            request, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::PLAYER_ROOM_TRANSITION_REQUEST,
                   bytes, size);
    participant.lastTransitionRequestAt = now;
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
        participant.syncStage = SyncStage::PREPARE;
        participant.syncItem = 0;
        participant.retryCount = 0;
        participant.awaitingAck = false;
        participant.lastSendAt = 0;
    }
    runtime.phaseStartedAt = now;
    setGameMessage("Party entering next room...");
}

void applyPositionUpdate(const PlayerPositionPayload& position)
{
    if (position.activityID != runtime.activityID ||
        !isParticipant(position.playerID) ||
        position.x >= (runtime.type == MultiplayerActivityType::FOREST
            ? FOREST_WIDTH : ROOM_WIDTH) ||
        position.y >= (runtime.type == MultiplayerActivityType::FOREST
            ? FOREST_HEIGHT : ROOM_HEIGHT)) return;
    Participant& participant = runtime.participants[position.playerID];
    if (participant.hasMoveSequence &&
        !isNewerMovementSequence(position.movementSequence,
                                 participant.lastMoveSequence)) return;
    participant.spawn.roomID = position.roomID;
    participant.spawn.x = position.x;
    participant.spawn.y = position.y;
    participant.lastMoveSequence = position.movementSequence;
    participant.hasMoveSequence = true;
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        position.roomID != runtime.currentRoom) return;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* entity = entities != nullptr
        ? getPlayerEntityByOwner(entities, count, position.playerID) : nullptr;
    if (entity == nullptr) return;
    markEntityFootprintDirty(*entity);
    entity->x = position.x;
    entity->y = position.y;
    markEntityFootprintDirty(*entity);
    if (position.playerID == multiplayerSession.getLocalPlayerID())
        moveDirection = static_cast<Direction>(position.facing);
    needsRedraw = true;
}

void sendStoredTravelResponse(uint32_t now)
{
    if (!multiplayerSession.isClient() ||
        (!runtime.pendingInviteAccepted && !runtime.pendingInviteDeclined))
        return;
    TravelResponsePayload response{};
    response.activityID = runtime.activityID;
    copyCharacterToNetwork(multiplayerSession.getLocalPlayerID(),
                           player, response.character);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodeTravelResponse(response, bytes, sizeof(bytes), size))
        sendToHost(runtime.pendingInviteAccepted
            ? NetworkPacketType::TRAVEL_ACCEPT
            : NetworkPacketType::TRAVEL_DECLINE, bytes, size);
    runtime.lastTravelResponseAt = now;
    if (runtime.travelResponseStartedAt == 0)
        runtime.travelResponseStartedAt = now;
}

void processTravelInvite(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    TravelInvitePayload invite{};
    if (!decodeTravelInvite(payload, payloadSize, invite)) return;
    if (isDuplicateTravelInvitation(
            runtime.activityID, runtime.type, runtime.pendingInvite,
            runtime.pendingInviteAccepted, runtime.pendingInviteDeclined,
            invite))
    {
        MP_DEBUG("Duplicate travel invitation ignored");
        return;
    }
    if (invite.activityID == runtime.activityID)
    {
        if (invite.activityType != runtime.type) return;
        if (runtime.phase != ActivityPhase::NONE)
        {
            MP_DEBUG("Duplicate travel invitation ignored");
            return;
        }
    }
    if (runtime.phase != ActivityPhase::NONE || gameState != GAME_TOWN) return;
    runtime.pendingInvite = true;
    runtime.pendingInviteAccepted = false;
    runtime.pendingInviteDeclined = false;
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
        participantCharacter(sender) = response.character;
        participant.characterReceived = true;
        runtime.participantMask |= playerBit(sender);
        MP_DEBUGF("Player %u accepted travel\n", sender);
    }
    else
        runtime.participantMask &= ~playerBit(sender);
}

void sendActivityAckToHost(const ActivityAckPayload& ack)
{
    uint8_t bytes[ACTIVITY_ACK_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodeActivityAck(ack, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::ACK, bytes, size,
                   NETWORK_FLAG_CONTROL_EVENT);
}

void processActivityAck(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || !isParticipant(sender) ||
        sender == HOST_PLAYER_ID) return;
    ActivityAckPayload ack{};
    if (!decodeActivityAck(payload, payloadSize, ack)) return;
    Participant& participant = runtime.participants[sender];
    if (runtime.phase == ActivityPhase::ACTIVE &&
        ack.activityID == runtime.activityID &&
        ack.packetType == NetworkPacketType::ACTIVITY_START &&
        ack.snapshotEpoch == participant.transferEpoch)
    {
        participant.startAcked = true;
        if (participant.awaitingAck &&
            activityAcksMatch(participant.expectedAck, ack))
        {
            participant.awaitingAck = false;
            participant.retryCount = 0;
            participant.roomLoading = false;
            participant.transitionPending = false;
        }
        MP_DEBUGF("ACTIVITY_START acknowledged by P%u\n", sender);
        return;
    }
    if (runtime.phase != ActivityPhase::LOADING &&
        !(runtime.phase == ActivityPhase::ACTIVE && participant.roomLoading))
        return;
    if (!participant.awaitingAck ||
        !activityAcksMatch(participant.expectedAck, ack)) return;
    MP_DEBUGF("ACK player=%u packet=%u type=%u chunk=%u epoch=%u\n",
              sender, static_cast<unsigned>(ack.packetType),
              static_cast<unsigned>(ack.snapshotType), ack.chunkIndex,
              ack.snapshotEpoch);
    advanceReliableTransfer(participant);
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
        (prepare.activityType == MultiplayerActivityType::DUNGEON &&
         prepare.roomID >= NETWORK_MAX_DUNGEON_ROOMS)) return;

    const uint16_t knownRoomRevision = prepare.roomID < NETWORK_MAX_DUNGEON_ROOMS
        ? runtime.roomRevisions[prepare.roomID] : 0;
    if (knownRoomRevision != 0 && prepare.roomID == runtime.currentRoom &&
        prepare.snapshotEpoch != knownRoomRevision &&
        !isNewerMovementSequence(prepare.snapshotEpoch, knownRoomRevision))
        return;

    if (runtime.phase == ActivityPhase::ACTIVE &&
        prepare.roomID == runtime.currentRoom &&
        prepare.snapshotEpoch == runtime.snapshotEpoch)
    {
        ActivityAckPayload ack = controlAck(NetworkPacketType::ACTIVITY_PREPARE);
        ack.snapshotEpoch = prepare.snapshotEpoch;
        sendActivityAckToHost(ack);
        return;
    }

    const bool wasActive = runtime.phase == ActivityPhase::ACTIVE;
    const bool newEpoch = runtime.currentRoom != prepare.roomID ||
        shouldResetActivitySnapshot(runtime.snapshotEpoch,
                                    prepare.snapshotEpoch);
    const bool firstPrepare = runtime.phase != ActivityPhase::LOADING;
    if (newEpoch)
    {
        resetSnapshotReceiver(runtime.snapshotReceiver);
        runtime.graphApplied = prepare.graphChunkCount == 0;
        runtime.detailApplied = prepare.detailChunkCount == 0;
        runtime.worldApplied = false;
        runtime.beginReceived = false;
        for (Participant& participant : runtime.participants)
        {
            participant.characterReceived = false;
            participant.spawnReceived = false;
        }
    }
    runtime.phase = ActivityPhase::LOADING;
    runtime.clientRoomLoading = wasActive || runtime.clientRoomLoading;
    runtime.type = prepare.activityType;
    runtime.snapshotEpoch = prepare.snapshotEpoch;
    runtime.participantMask = prepare.participantMask;
    runtime.visiblePlayerMask = prepare.roomPlayerMask;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        runtime.participants[playerID].participating =
            (prepare.participantMask & playerBit(playerID)) != 0;
    runtime.currentRoom = prepare.roomID;
    if (prepare.roomID < NETWORK_MAX_DUNGEON_ROOMS)
        runtime.roomRevisions[prepare.roomID] = prepare.snapshotEpoch;
    runtime.expectedGraphChunks = prepare.graphChunkCount;
    runtime.expectedDetailChunks = prepare.detailChunkCount;
    runtime.expectedWorldChunks = prepare.worldChunkCount;
    if (newEpoch || firstPrepare)
    {
        runtime.phaseStartedAt = millis();
        setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
            ? "Joining Dungeon..." : "Joining Forest...");
        needsRedraw = true;
    }
    runtime.pendingInviteAccepted = false;
    runtime.participants[runtime.localPlayerID].transitionPending = false;
    runtime.travelResponseStartedAt = 0;
    sendActivityAckToHost(controlAck(NetworkPacketType::ACTIVITY_PREPARE));
    MP_DEBUGF("ActivityPrepare received epoch=%u graph=%u detail=%u world=%u\n",
              prepare.snapshotEpoch, prepare.graphChunkCount,
              prepare.detailChunkCount, prepare.worldChunkCount);
}

void processDungeonBegin(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING) return;
    DungeonBeginPayload begin{};
    if (!decodeDungeonBegin(payload, payloadSize, begin) ||
        begin.activityID != runtime.activityID ||
        begin.snapshotEpoch != runtime.snapshotEpoch ||
        runtime.type != MultiplayerActivityType::DUNGEON ||
        begin.entranceRoom != runtime.currentRoom) return;
    runtime.beginReceived = true;
    sendActivityAckToHost(controlAck(NetworkPacketType::DUNGEON_BEGIN));
    MP_DEBUGF("DungeonBegin received rooms=%u entrance=%u epoch=%u\n",
              begin.roomCount, begin.entranceRoom, begin.snapshotEpoch);
}

void processForestBegin(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING) return;
    ActivityReadyPayload begin{};
    if (!decodeActivityReady(payload, payloadSize, begin) ||
        begin.activityID != runtime.activityID ||
        begin.snapshotEpoch != runtime.snapshotEpoch ||
        runtime.type != MultiplayerActivityType::FOREST) return;
    runtime.beginReceived = true;
    sendActivityAckToHost(controlAck(NetworkPacketType::FOREST_BEGIN));
    MP_DEBUGF("ForestBegin received epoch=%u\n", begin.snapshotEpoch);
}

void processRoomTransition(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING) return;
    RoomTransitionPayload transition{};
    if (!decodeRoomTransition(payload, payloadSize, transition) ||
        transition.activityID != runtime.activityID ||
        transition.snapshotEpoch != runtime.snapshotEpoch ||
        transition.roomID != runtime.currentRoom ||
        runtime.type != MultiplayerActivityType::DUNGEON) return;
    runtime.beginReceived = true;
    sendActivityAckToHost(controlAck(NetworkPacketType::ROOM_TRANSITION));
    MP_DEBUGF("RoomTransition received room=%u epoch=%u\n",
              transition.roomID, transition.snapshotEpoch);
}

void recordSnapshotReject(const SnapshotChunkPayload& chunk, const char* reason)
{
#if MULTIPLAYER_DEBUG
    if (multiplayerCombat != nullptr) ++multiplayerCombat->snapshotChunkRejects;
    MP_DEBUGF("Snapshot rejected reason=%s type=%u epoch=%u chunk=%u/%u\n",
              reason, static_cast<unsigned>(chunk.snapshotType),
              chunk.snapshotEpoch, chunk.chunkIndex + 1, chunk.totalChunks);
#else
    (void)chunk;
    (void)reason;
#endif
}

void processSnapshotChunk(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase != ActivityPhase::LOADING) return;
    SnapshotChunkPayload chunk{};
    if (!decodeSnapshotChunk(payload, payloadSize, chunk))
    {
#if MULTIPLAYER_DEBUG
        if (multiplayerCombat != nullptr) ++multiplayerCombat->snapshotChunkRejects;
#endif
        return;
    }
    uint8_t expectedCount = 0;
    uint8_t expectedRoom = NETWORK_NO_ROOM;
    bool alreadyApplied = false;
    if (chunk.snapshotType == SnapshotType::DUNGEON_GRAPH)
    {
        expectedCount = runtime.expectedGraphChunks;
        alreadyApplied = runtime.graphApplied;
    }
    else if (chunk.snapshotType == SnapshotType::DUNGEON_ROOM_DETAIL)
    {
        if (!runtime.graphApplied)
        {
            recordSnapshotReject(chunk, "graph-not-ready");
            return;
        }
        expectedCount = runtime.expectedDetailChunks;
        expectedRoom = runtime.currentRoom;
        alreadyApplied = runtime.detailApplied;
    }
    else
    {
        if (runtime.type == MultiplayerActivityType::DUNGEON &&
            (!runtime.graphApplied || !runtime.detailApplied))
        {
            recordSnapshotReject(chunk, "room-metadata-not-ready");
            return;
        }
        const SnapshotType expectedType =
            runtime.type == MultiplayerActivityType::FOREST
                ? SnapshotType::FOREST_STATE : SnapshotType::DUNGEON_ROOM;
        if (chunk.snapshotType != expectedType)
        {
            recordSnapshotReject(chunk, "wrong-world-type");
            return;
        }
        expectedCount = runtime.expectedWorldChunks;
        expectedRoom = runtime.currentRoom;
        alreadyApplied = runtime.worldApplied;
    }
    ActivityAckPayload ack = controlAck(NetworkPacketType::SNAPSHOT_CHUNK);
    ack.snapshotType = chunk.snapshotType;
    ack.roomID = chunk.roomID;
    ack.chunkIndex = chunk.chunkIndex;
    if (alreadyApplied)
    {
        if (chunk.activityID == runtime.activityID &&
            chunk.snapshotEpoch == runtime.snapshotEpoch &&
            chunk.roomID == expectedRoom && chunk.totalChunks == expectedCount)
        {
#if MULTIPLAYER_DEBUG
            if (multiplayerCombat != nullptr) ++multiplayerCombat->snapshotChunkDuplicates;
#endif
            sendActivityAckToHost(ack);
        }
        else
            recordSnapshotReject(chunk, "applied-snapshot-mismatch");
        return;
    }
    const bool duplicate = isDuplicateSnapshotChunk(runtime.snapshotReceiver, chunk);
    if (!acceptSnapshotChunk(runtime.snapshotReceiver, chunk, runtime.activityID,
                             expectedRoom, runtime.snapshotEpoch,
                             expectedCount))
    {
        recordSnapshotReject(chunk, "validation");
        return;
    }
    if (duplicate)
    {
#if MULTIPLAYER_DEBUG
        if (multiplayerCombat != nullptr) ++multiplayerCombat->snapshotChunkDuplicates;
#endif
        MP_DEBUGF("Snapshot duplicate type=%u epoch=%u chunk=%u/%u\n",
                  static_cast<unsigned>(chunk.snapshotType),
                  chunk.snapshotEpoch, chunk.chunkIndex + 1,
                  chunk.totalChunks);
    }
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
    sendActivityAckToHost(ack);
    MP_DEBUGF("Snapshot received type=%u epoch=%u chunk=%u/%u\n",
              static_cast<unsigned>(chunk.snapshotType), chunk.snapshotEpoch,
              chunk.chunkIndex + 1, chunk.totalChunks);
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
    participantCharacter(state.character.playerID) = state.character;
    participant.characterReceived = true;
    finishClientLoading();
    sendActivityAckToHost(controlAck(
        NetworkPacketType::PLAYER_CHARACTER_STATE, state.character.playerID));
    MP_DEBUGF("Character state received P%u\n", state.character.playerID);
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
    sendActivityAckToHost(controlAck(
        NetworkPacketType::PLAYER_SPAWN, spawn.playerID));
    finishClientLoading();
    MP_DEBUGF("Player spawn received P%u at %u,%u\n",
              spawn.playerID, spawn.x, spawn.y);
}

void processActivityReady(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize,
    bool roomReady)
{
    if (!multiplayerSession.isHost() ||
        (runtime.phase != ActivityPhase::LOADING &&
         runtime.phase != ActivityPhase::ACTIVE) ||
        !isParticipant(sender)) return;
    ActivityReadyPayload ready{};
    if (!decodeActivityReady(payload, payloadSize, ready) ||
        ready.activityID != runtime.activityID) return;
    Participant& participant = runtime.participants[sender];
    if (ready.snapshotEpoch != participant.transferEpoch ||
        participant.syncStage != SyncStage::COMPLETE) return;
    participant.ready = true;
    if (roomReady)
    {
        participant.startAcked = false;
        participant.retryCount = 0;
        sendTargetedActivityStart(sender);
        MP_DEBUGF("Player %u Room %u ready\n", sender,
                  participant.spawn.roomID);
    }
    MP_DEBUGF("Player %u ready, epoch %u\n", sender, ready.snapshotEpoch);
}

void processActivityStart(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID) return;
    ActivityReadyPayload start{};
    if (!decodeActivityReady(payload, payloadSize, start) ||
        start.activityID != runtime.activityID ||
        start.snapshotEpoch != runtime.snapshotEpoch) return;
    ActivityAckPayload ack = controlAck(NetworkPacketType::ACTIVITY_START);
    if (runtime.phase == ActivityPhase::ACTIVE)
    {
        sendActivityAckToHost(ack);
        return;
    }
    if (runtime.phase != ActivityPhase::LOADING || !clientLoadComplete()) return;
    runtime.phase = ActivityPhase::ACTIVE;
    runtime.clientRoomLoading = false;
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = false;
    if (isMultiplayerCombatActive())
    {
        const MultiplayerCombatantRef* current = currentCombatRef();
        if (current != nullptr)
        {
            Entity* currentEntity = resolveVisibleCombatant(*current);
            if (currentEntity != nullptr)
            {
                const MultiplayerCombatSlot& slot = multiplayerCombat->slots[
                    multiplayerCombat->currentTurnIndex];
                currentEntity->turn.turnActive = true;
                currentEntity->turn.movementRemaining = slot.movementRemaining;
                currentEntity->turn.moveActionUsed = slot.moveActionUsed;
                currentEntity->turn.standardActionUsed =
                    slot.standardActionUsed;
            }
        }
    }
    setGameMessage(runtime.type == MultiplayerActivityType::DUNGEON
        ? "Party entered dungeon." : "Party entered forest.");
    sendActivityAckToHost(ack);
    MP_DEBUG("ACTIVITY_START received");
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
    endActivityLocally(runtime.phase == ActivityPhase::LOADING
        ? runtime.type == MultiplayerActivityType::DUNGEON
            ? "Unable to join dungeon." : "Unable to join forest."
        : "The host ended the activity.");
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
    if (isMultiplayerCombatActive())
    {
        const MultiplayerCombatantRef* current = currentCombatRef();
        if (current == nullptr ||
            current->kind != MultiplayerCombatantKind::PLAYER ||
            current->playerID != sender) return;
    }
    MP_DEBUGF("Player %u move request seq %u\n",
              sender, request.clientMovementSequence);
    Participant& participant = runtime.participants[sender];
    if (participant.hasMoveSequence &&
        !isNewerMovementSequence(request.clientMovementSequence,
                                 participant.lastMoveSequence)) return;
    const uint8_t senderRoom = participant.spawn.roomID;
    const uint8_t hostRoom = runtime.participants[HOST_PLAYER_ID].spawn.roomID;
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        !activateAuthoritativeDungeonRoom(senderRoom, ENTRY_START)) return;
    MultiplayerCombatSlot* combatSlot = nullptr;
    if (isMultiplayerCombatActive())
    {
        combatSlot = &multiplayerCombat->slots[
            multiplayerCombat->currentTurnIndex];
        uint8_t movingCount = 0;
        Entity* movingEntities = getActiveMapEntities(movingCount);
        Entity* movingPlayer = movingEntities != nullptr
            ? getPlayerEntityByOwner(movingEntities, movingCount, sender)
            : nullptr;
        if (movingPlayer != nullptr)
        {
            movingPlayer->turn.turnActive = true;
            movingPlayer->turn.movementRemaining =
                combatSlot->movementRemaining;
            movingPlayer->turn.moveActionUsed = combatSlot->moveActionUsed;
            movingPlayer->turn.standardActionUsed =
                combatSlot->standardActionUsed;
        }
        combat.waitingForPlayer = true;
    }
    bool roomChanged = false;
    tryMovePlayerAuthoritative(
        dungeon, sender, static_cast<Direction>(request.direction),
        false, roomChanged);
    checkAuthoritativeRoomCombat(sender);
    if (combatSlot != nullptr)
    {
        uint8_t movingCount = 0;
        Entity* movingEntities = getActiveMapEntities(movingCount);
        Entity* movingPlayer = movingEntities != nullptr
            ? getPlayerEntityByOwner(movingEntities, movingCount, sender)
            : nullptr;
        if (movingPlayer != nullptr)
        {
            combatSlot->movementRemaining =
                movingPlayer->turn.movementRemaining;
            combatSlot->moveActionUsed = movingPlayer->turn.moveActionUsed;
            combatSlot->standardActionUsed =
                movingPlayer->turn.standardActionUsed;
        }
        combat.waitingForPlayer = isLocalPlayersMultiplayerTurn();
    }
    participant.lastMoveSequence = request.clientMovementSequence;
    participant.hasMoveSequence = true;
    capturePlayersInLoadedDungeonRoom();
    sendAuthoritativePosition(
        sender, request.clientMovementSequence,
        static_cast<Direction>(request.direction));
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        senderRoom != hostRoom)
        restoreHostDungeonRoom();
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

void processRoomTransitionRequest(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || runtime.phase != ActivityPhase::ACTIVE ||
        runtime.type != MultiplayerActivityType::DUNGEON ||
        !isParticipant(sender) || sender == HOST_PLAYER_ID) return;
    PlayerRoomTransitionRequestPayload request{};
    if (!decodePlayerRoomTransitionRequest(payload, payloadSize, request) ||
        request.activityID != runtime.activityID) return;
    Participant& participant = runtime.participants[sender];
    if (participant.spawn.roomID != request.sourceRoomID ||
        participant.roomLoading ||
        (participant.hasTransitionSequence &&
         !isNewerMovementSequence(request.transitionSequence,
                                   participant.lastTransitionSequence))) return;
    if (isMultiplayerCombatActive())
    {
        const MultiplayerCombatantRef* current = currentCombatRef();
        if (current == nullptr ||
            current->kind != MultiplayerCombatantKind::PLAYER ||
            current->playerID != sender ||
            multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]
                    .movementRemaining == 0)
            return;
    }
    participant.transitionPending = true;
    participant.lastTransitionSequence = request.transitionSequence;
    participant.hasTransitionSequence = true;
    participant.pendingTransitionDirection = request.direction;
    participant.pendingTransitionSourceRoom = request.sourceRoomID;
    MP_DEBUGF("Player %u transition request R%u direction=%u\n",
              sender, request.sourceRoomID, request.direction);

    const uint8_t hostRoom = runtime.participants[HOST_PLAYER_ID].spawn.roomID;
    if (!activateAuthoritativeDungeonRoom(request.sourceRoomID, ENTRY_START))
        return;
    uint8_t destinationRoom = NETWORK_NO_ROOM;
    RoomEntry entry = ENTRY_START;
    const bool valid = resolvePlayerDoorTransition(
        sender, static_cast<Direction>(request.direction),
        destinationRoom, entry);
    if (!valid)
    {
        participant.transitionPending = false;
        restoreHostDungeonRoom();
        sendAuthoritativePosition(
            sender, participant.lastMoveSequence,
            static_cast<Direction>(request.direction));
        return;
    }
    beginIndependentRoomTransition(
        sender, destinationRoom, entry, request.transitionSequence, millis());
    if (dungeon.loadedRoom != hostRoom) restoreHostDungeonRoom();
}

void processPlayerRoomChange(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase == ActivityPhase::NONE) return;
    PlayerRoomChangePayload change{};
    if (!decodePlayerRoomChange(payload, payloadSize, change) ||
        change.activityID != runtime.activityID ||
        !isParticipant(change.playerID)) return;
    Participant& participant = runtime.participants[change.playerID];
    const uint8_t previousRoom = participant.spawn.roomID;
    participant.spawn.roomID = change.roomID;
    participant.spawn.x = change.x;
    participant.spawn.y = change.y;
    participant.transitionEntry = change.entryDirection;
    if (multiplayerCombat != nullptr && multiplayerCombat->active)
        for (uint8_t index = 0; index < multiplayerCombat->combatantCount;
             ++index)
        {
            MultiplayerCombatantRef& reference =
                multiplayerCombat->slots[index].initiative.combatant;
            if (reference.kind == MultiplayerCombatantKind::PLAYER &&
                reference.playerID == change.playerID)
                reference.roomID = change.roomID;
        }
    if (change.playerID == runtime.localPlayerID) return;
    if (previousRoom == runtime.currentRoom)
        removePlayerEntity(change.playerID);
    if (change.roomID == runtime.currentRoom)
        spawnPlayerFromState(participantCharacter(change.playerID),
                             change.x, change.y);
    needsRedraw = true;
}

void processSharedGameplayMessage(
    PlayerID sender,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        runtime.phase == ActivityPhase::NONE) return;
    SharedGameplayMessagePayload message{};
    if (!decodeSharedGameplayMessage(payload, payloadSize, message) ||
        message.activityID != runtime.activityID) return;
    if (runtime.hasSharedMessageSequence &&
        !isNewerMovementSequence(message.messageSequence,
                                 runtime.lastSharedMessageSequence)) return;
    runtime.lastSharedMessageSequence = message.messageSequence;
    runtime.hasSharedMessageSequence = true;
    setGameMessage(message.text);
}

bool combatRefsEqual(
    const MultiplayerCombatantRef& left,
    const MultiplayerCombatantRef& right)
{
    return left.kind == right.kind && left.playerID == right.playerID &&
           left.roomID == right.roomID && left.entityID == right.entityID;
}

MultiplayerCombatantRef combatRefForEntity(const Entity& entity)
{
    MultiplayerCombatantRef reference{};
    reference.roomID = getCombatRoomID();
    if (entity.type == ENTITY_PLAYER && isValidPlayerID(entity.ownerPlayerID))
    {
        reference.kind = MultiplayerCombatantKind::PLAYER;
        reference.playerID = entity.ownerPlayerID;
        return reference;
    }
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    if (entity.type == ENTITY_MONSTER && entities != nullptr &&
        &entity >= entities && &entity < entities + count)
    {
        reference.kind = MultiplayerCombatantKind::MONSTER;
        reference.playerID = INVALID_PLAYER_ID;
        reference.entityID = static_cast<uint8_t>(&entity - entities + 1);
    }
    return reference;
}

Entity* resolveVisibleCombatant(const MultiplayerCombatantRef& reference)
{
    if (reference.roomID != getCombatRoomID()) return nullptr;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    if (entities == nullptr) return nullptr;
    if (reference.kind == MultiplayerCombatantKind::PLAYER)
        return getPlayerEntityByOwner(entities, count, reference.playerID);
    if (reference.kind != MultiplayerCombatantKind::MONSTER ||
        reference.entityID == 0 || reference.entityID > count) return nullptr;
    Entity* entity = &entities[reference.entityID - 1];
    return entity->type == ENTITY_MONSTER ? entity : nullptr;
}

int findCombatSlot(const MultiplayerCombatantRef& reference)
{
    if (multiplayerCombat == nullptr) return -1;
    for (uint8_t index = 0; index < multiplayerCombat->combatantCount; ++index)
        if (combatRefsEqual(multiplayerCombat->slots[index].initiative.combatant,
                            reference)) return index;
    return -1;
}

const MultiplayerCombatantRef* currentCombatRef()
{
    if (multiplayerCombat == nullptr || !multiplayerCombat->active ||
        multiplayerCombat->currentTurnIndex >= multiplayerCombat->combatantCount)
        return nullptr;
    return &multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]
                .initiative.combatant;
}

void resetMultiplayerTurnState(MultiplayerCombatSlot& slot);
const char* combatantDisplayName(
    const MultiplayerCombatantRef& reference,
    const Entity* entity);

uint8_t combatRecipientMask()
{
    return static_cast<uint8_t>(runtime.participantMask &
                                ~playerBit(HOST_PLAYER_ID));
}

void sendPendingCombatPacket()
{
    if (multiplayerCombat == nullptr || multiplayerCombat->pendingSize == 0)
        return;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if ((multiplayerCombat->pendingPlayerMask & playerBit(playerID)) != 0)
            sendToPlayer(playerID, multiplayerCombat->pendingPacketType,
                         multiplayerCombat->pendingBytes,
                         multiplayerCombat->pendingSize,
                         NETWORK_FLAG_CONTROL_EVENT);
    multiplayerCombat->lastSendAt = millis();
}

bool queueReliableCombatPacket(
    NetworkPacketType packetType,
    uint16_t eventSequence,
    const uint8_t* bytes,
    size_t size)
{
    if (multiplayerCombat == nullptr || bytes == nullptr || size == 0 ||
        size > sizeof(multiplayerCombat->pendingBytes) ||
        multiplayerCombat->pendingSize != 0) return false;
    multiplayerCombat->pendingPacketType = packetType;
    multiplayerCombat->pendingEventSequence = eventSequence;
    multiplayerCombat->pendingPlayerMask = combatRecipientMask();
    multiplayerCombat->retryCount = 0;
    multiplayerCombat->pendingSize = static_cast<uint16_t>(size);
    memcpy(multiplayerCombat->pendingBytes, bytes, size);
    if (multiplayerCombat->pendingPlayerMask == 0)
    {
        multiplayerCombat->pendingSize = 0;
        return true;
    }
    sendPendingCombatPacket();
    return true;
}

void sendCombatAck(NetworkPacketType packetType, uint16_t eventSequence)
{
    MultiplayerCombatAckPayload ack{};
    ack.activityID = runtime.activityID;
    ack.combatID = multiplayerCombat->combatID;
    ack.eventSequence = eventSequence;
    ack.packetType = packetType;
    uint8_t bytes[COMBAT_ACK_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (encodeMultiplayerCombatAck(ack, bytes, sizeof(bytes), size))
        sendToHost(NetworkPacketType::COMBAT_ACK, bytes, size,
                   NETWORK_FLAG_CONTROL_EVENT);
}

void processCombatAck(
    PlayerID sender, const uint8_t* payload, uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || multiplayerCombat == nullptr ||
        !multiplayerCombat->active || !isParticipant(sender)) return;
    MultiplayerCombatAckPayload ack{};
    if (!decodeMultiplayerCombatAck(payload, payloadSize, ack) ||
        ack.activityID != runtime.activityID ||
        ack.combatID != multiplayerCombat->combatID ||
        ack.eventSequence != multiplayerCombat->pendingEventSequence ||
        ack.packetType != multiplayerCombat->pendingPacketType) return;
    multiplayerCombat->pendingPlayerMask &= ~playerBit(sender);
    if (multiplayerCombat->pendingPlayerMask == 0)
    {
        multiplayerCombat->pendingSize = 0;
        multiplayerCombat->retryCount = 0;
    }
}

void applyCombatantEventState(
    const MultiplayerCombatantRef& reference,
    uint8_t x,
    uint8_t y,
    int16_t hp,
    uint8_t state)
{
    if (reference.kind == MultiplayerCombatantKind::PLAYER &&
        isValidPlayerID(reference.playerID))
    {
        NetworkCharacterState& character = participantCharacter(reference.playerID);
        character.currentHP = hp;
        character.characterState = state;
        runtime.participants[reference.playerID].spawn.roomID = reference.roomID;
        runtime.participants[reference.playerID].spawn.x = x;
        runtime.participants[reference.playerID].spawn.y = y;
    }
    Entity* entity = resolveVisibleCombatant(reference);
    if (entity == nullptr) return;
    markEntityFootprintDirty(*entity);
    entity->x = x;
    entity->y = y;
    entity->character.health.currentHP = hp;
    entity->character.state = static_cast<CharacterState>(state);
    markEntityFootprintDirty(*entity);
    needsRedraw = true;
}

void processCombatBegin(
    PlayerID sender, const uint8_t* payload, uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        multiplayerCombat == nullptr) return;
    MultiplayerCombatBeginPayload begin{};
    if (!decodeMultiplayerCombatBegin(payload, payloadSize, begin) ||
        begin.activityID != runtime.activityID) return;
    if (multiplayerCombat->active && begin.combatID == multiplayerCombat->combatID &&
        (!isNewerMovementSequence(begin.eventSequence,
                                  multiplayerCombat->lastEventSequence)))
    {
        sendCombatAck(NetworkPacketType::COMBAT_BEGIN, begin.eventSequence);
        return;
    }
    const bool reinforcementUpdate = multiplayerCombat->active &&
        begin.combatID == multiplayerCombat->combatID;
    MultiplayerCombatSlot priorCurrent{};
    if (reinforcementUpdate &&
        multiplayerCombat->currentTurnIndex < multiplayerCombat->combatantCount)
        priorCurrent = multiplayerCombat->slots[
            multiplayerCombat->currentTurnIndex];
    if (!reinforcementUpdate)
        *multiplayerCombat = MultiplayerCombatRuntime{};
    multiplayerCombat->active = true;
    multiplayerCombat->combatID = begin.combatID;
    multiplayerCombat->lastEventSequence = begin.eventSequence;
    multiplayerCombat->hasEventSequence = true;
    multiplayerCombat->combatantCount = begin.combatantCount;
    multiplayerCombat->currentTurnIndex = begin.currentTurnIndex;
    multiplayerCombat->round = begin.round;
    if (multiplayerCombat->pendingPacketType ==
        NetworkPacketType::COMBAT_ACTION_REQUEST)
    {
        multiplayerCombat->pendingSize = 0;
        multiplayerCombat->retryCount = 0;
    }
    for (uint8_t index = 0; index < begin.combatantCount; ++index)
    {
        multiplayerCombat->slots[index].initiative = begin.combatants[index];
        Entity* entity = resolveVisibleCombatant(
            begin.combatants[index].combatant);
        if (entity != nullptr && entity->type == ENTITY_MONSTER)
        {
            entity->awareOfPlayer = true;
            entity->revealedToPlayer = true;
            markEntityFootprintDirty(*entity);
        }
    }
    if (reinforcementUpdate)
    {
        const int priorIndex = findCombatSlot(priorCurrent.initiative.combatant);
        if (priorIndex >= 0)
        {
            MultiplayerCombatSlot& current = multiplayerCombat->slots[priorIndex];
            current.movementRemaining = priorCurrent.movementRemaining;
            current.moveActionUsed = priorCurrent.moveActionUsed;
            current.standardActionUsed = priorCurrent.standardActionUsed;
            current.fullDefense = priorCurrent.fullDefense;
        }
    }
    else
        resetMultiplayerTurnState(
            multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]);
    combat.active = true;
    combat.phase = COMBAT_TURN;
    combat.waitingForPlayer = isLocalPlayersMultiplayerTurn();
    setGameMessage(reinforcementUpdate
        ? "More enemies join combat!" : "Combat begins!");
    sendCombatAck(NetworkPacketType::COMBAT_BEGIN, begin.eventSequence);
    setFullMapRedraw();
}

void processCombatEvent(
    PlayerID sender,
    NetworkPacketType packetType,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (!multiplayerSession.isClient() || sender != HOST_PLAYER_ID ||
        multiplayerCombat == nullptr || !multiplayerCombat->active) return;
    MultiplayerCombatEventPayload event{};
    if (!decodeMultiplayerCombatEvent(payload, payloadSize, event) ||
        event.activityID != runtime.activityID ||
        event.combatID != multiplayerCombat->combatID) return;
    if (multiplayerCombat->hasEventSequence &&
        !isNewerMovementSequence(event.eventSequence,
                                 multiplayerCombat->lastEventSequence))
    {
        sendCombatAck(packetType, event.eventSequence);
        return;
    }
    multiplayerCombat->lastEventSequence = event.eventSequence;
    multiplayerCombat->hasEventSequence = true;
    if (multiplayerCombat->pendingPacketType ==
        NetworkPacketType::COMBAT_ACTION_REQUEST)
    {
        multiplayerCombat->pendingSize = 0;
        multiplayerCombat->retryCount = 0;
    }
    applyCombatantEventState(event.actor, event.actorX, event.actorY,
                             event.actorHP, event.actorState);
    applyCombatantEventState(event.target, event.targetX, event.targetY,
                             event.targetHP, event.targetState);
    Entity* previousTurnEntity = getVisibleMultiplayerCurrentCombatant();
    if (previousTurnEntity != nullptr)
        markEntityFootprintDirty(*previousTurnEntity);
    multiplayerCombat->currentTurnIndex = event.currentTurnIndex;
    multiplayerCombat->round = event.round;
    Entity* currentTurnEntity = getVisibleMultiplayerCurrentCombatant();
    if (currentTurnEntity != nullptr)
        markEntityFootprintDirty(*currentTurnEntity);
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        event.actor.roomID < NETWORK_MAX_DUNGEON_ROOMS)
        runtime.roomRevisions[event.actor.roomID] = event.roomRevision;
    if (event.textLength > 0) setGameMessage(event.text);
    if (packetType == NetworkPacketType::COMBAT_END ||
        event.eventType == MultiplayerCombatEventType::COMBAT_ENDED)
    {
        multiplayerCombat->active = false;
        combat.active = false;
        combat.phase = COMBAT_NONE;
        combat.waitingForPlayer = false;
    }
    else
    {
        resetMultiplayerTurnState(
            multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]);
        combat.waitingForPlayer = isLocalPlayersMultiplayerTurn();
    }
    sendCombatAck(packetType, event.eventSequence);
}

bool processAuthoritativeCombatAction(
    PlayerID sender, const MultiplayerCombatActionRequestPayload& request);

void processCombatActionRequest(
    PlayerID sender, const uint8_t* payload, uint16_t payloadSize)
{
    if (!multiplayerSession.isHost() || multiplayerCombat == nullptr ||
        !multiplayerCombat->active || multiplayerCombat->pendingSize != 0)
        return;
    MultiplayerCombatActionRequestPayload request{};
    if (!decodeMultiplayerCombatActionRequest(payload, payloadSize, request) ||
        request.activityID != runtime.activityID ||
        request.combatID != multiplayerCombat->combatID ||
        !isValidPlayerID(sender) || request.actor.playerID != sender) return;
    if (multiplayerCombat->hasActionSequence[sender] &&
        !isNewerMovementSequence(request.actionSequence,
            multiplayerCombat->lastActionSequence[sender])) return;
    multiplayerCombat->hasActionSequence[sender] = true;
    multiplayerCombat->lastActionSequence[sender] = request.actionSequence;
    processAuthoritativeCombatAction(sender, request);
}

void handleGameplayPacket(
    PlayerID sender,
    NetworkPacketType packetType,
    const uint8_t* payload,
    uint16_t payloadSize)
{
    if (participantCharacters == nullptr) return;
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
        case NetworkPacketType::DUNGEON_BEGIN:
            processDungeonBegin(sender, payload, payloadSize);
            break;
        case NetworkPacketType::FOREST_BEGIN:
            processForestBegin(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ROOM_TRANSITION:
            processRoomTransition(sender, payload, payloadSize);
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
            processActivityReady(sender, payload, payloadSize, false);
            break;
        case NetworkPacketType::ROOM_READY:
            processActivityReady(sender, payload, payloadSize, true);
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
        case NetworkPacketType::PLAYER_ROOM_TRANSITION_REQUEST:
            processRoomTransitionRequest(sender, payload, payloadSize);
            break;
        case NetworkPacketType::PLAYER_ROOM_CHANGE:
            processPlayerRoomChange(sender, payload, payloadSize);
            break;
        case NetworkPacketType::SHARED_GAMEPLAY_MESSAGE:
            processSharedGameplayMessage(sender, payload, payloadSize);
            break;
        case NetworkPacketType::COMBAT_BEGIN:
            processCombatBegin(sender, payload, payloadSize);
            break;
        case NetworkPacketType::COMBAT_ACTION_REQUEST:
            processCombatActionRequest(sender, payload, payloadSize);
            break;
        case NetworkPacketType::COMBAT_ACTION_RESULT:
        case NetworkPacketType::COMBAT_TURN:
        case NetworkPacketType::COMBAT_END:
            processCombatEvent(sender, packetType, payload, payloadSize);
            break;
        case NetworkPacketType::COMBAT_ACK:
            processCombatAck(sender, payload, payloadSize);
            break;
        case NetworkPacketType::ACK:
            processActivityAck(sender, payload, payloadSize);
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
        const NetworkCharacterState& character = participantCharacter(playerID);
        snprintf(message, sizeof(message), "%s disconnected.",
                 character.displayName[0] != '\0'
                    ? character.displayName : "Adventurer");
        removePlayerEntity(playerID);
        runtime.participantMask &= ~playerBit(playerID);
        participant.participating = false;
        if (multiplayerCombat != nullptr)
        {
            multiplayerCombat->pendingPlayerMask &= ~playerBit(playerID);
            if (multiplayerCombat->pendingPlayerMask == 0)
                multiplayerCombat->pendingSize = 0;
        }
        if (multiplayerSession.isHost()) broadcastPlayerDespawn(playerID);
        setGameMessage(message);
    }
}

bool multiplayerCombatantAlive(const MultiplayerCombatantRef& reference)
{
    if (reference.kind == MultiplayerCombatantKind::PLAYER)
    {
        if (!isValidPlayerID(reference.playerID) ||
            !isParticipant(reference.playerID)) return false;
        const NetworkCharacterState& character = participantCharacter(reference.playerID);
        return character.currentHP > 0 &&
               character.characterState == STATE_ALIVE;
    }
    if (reference.kind != MultiplayerCombatantKind::MONSTER ||
        reference.entityID == 0)
        return false;
    if (runtime.type == MultiplayerActivityType::FOREST)
    {
        Entity* entity = resolveVisibleCombatant(reference);
        return entity != nullptr && entity->active &&
               entity->character.state == STATE_ALIVE;
    }
    if (reference.roomID >= dungeon.roomCount) return false;
    if (reference.roomID == dungeon.loadedRoom)
    {
        Entity* entity = resolveVisibleCombatant(reference);
        return entity != nullptr && entity->active &&
               entity->character.state == STATE_ALIVE;
    }
    const DungeonRoomRuntime& room = dungeon.roomRuntime[reference.roomID];
    const uint8_t index = reference.entityID - 1;
    if (!room.persistenceReady || index >= room.persistentEntityCount)
        return false;
    const PersistentEntity& entity =
        room.entityStorage.compact.persistentEntities[index];
    return entity.type == ENTITY_MONSTER &&
           (entity.flags & PERSISTENT_ENTITY_ACTIVE) != 0 &&
           entity.payload.monster.state == STATE_ALIVE &&
           entity.payload.monster.currentHP > 0;
}

bool hasLivingMultiplayerHostile()
{
    for (uint8_t index = 0; index < multiplayerCombat->combatantCount; ++index)
    {
        const MultiplayerCombatantRef& reference =
            multiplayerCombat->slots[index].initiative.combatant;
        if (reference.kind == MultiplayerCombatantKind::MONSTER &&
            multiplayerCombatantAlive(reference)) return true;
    }
    return false;
}

void resetMultiplayerTurnState(MultiplayerCombatSlot& slot)
{
    slot.moveActionUsed = false;
    slot.standardActionUsed = false;
    slot.fullDefense = false;
    const MultiplayerCombatantRef& reference = slot.initiative.combatant;
    slot.movementRemaining = reference.kind == MultiplayerCombatantKind::PLAYER &&
        isValidPlayerID(reference.playerID)
        ? participantCharacter(reference.playerID).speed
        : 6;
    Entity* entity = resolveVisibleCombatant(reference);
    if (entity != nullptr)
    {
        entity->turn = TurnState{};
        entity->turn.turnActive = true;
        entity->turn.movementRemaining = slot.movementRemaining;
        entity->turn.fullDefense = slot.fullDefense;
    }
}

void advanceMultiplayerTurn()
{
    if (multiplayerCombat == nullptr || multiplayerCombat->combatantCount == 0)
        return;
    Entity* previousVisible = getVisibleMultiplayerCurrentCombatant();
    if (previousVisible != nullptr) markEntityFootprintDirty(*previousVisible);
    for (uint8_t attempts = 0; attempts < multiplayerCombat->combatantCount;
         ++attempts)
    {
        ++multiplayerCombat->currentTurnIndex;
        if (multiplayerCombat->currentTurnIndex >=
            multiplayerCombat->combatantCount)
        {
            multiplayerCombat->currentTurnIndex = 0;
            ++multiplayerCombat->round;
        }
        MultiplayerCombatSlot& slot = multiplayerCombat->slots[
            multiplayerCombat->currentTurnIndex];
        if (multiplayerCombatantAlive(slot.initiative.combatant))
        {
            resetMultiplayerTurnState(slot);
            break;
        }
    }
    Entity* currentVisible = getVisibleMultiplayerCurrentCombatant();
    if (currentVisible != nullptr) markEntityFootprintDirty(*currentVisible);
    needsRedraw = true;
    combat.waitingForPlayer = isLocalPlayersMultiplayerTurn();
}

void fillCombatEventState(
    MultiplayerCombatEventPayload& event,
    const MultiplayerCombatantRef& reference,
    bool actor)
{
    Entity* entity = resolveVisibleCombatant(reference);
    uint8_t x = 0;
    uint8_t y = 0;
    int16_t hp = 0;
    uint8_t state = STATE_DEAD;
    if (reference.kind == MultiplayerCombatantKind::PLAYER &&
        isValidPlayerID(reference.playerID))
    {
        const Participant& participant = runtime.participants[reference.playerID];
        const NetworkCharacterState& character = participantCharacter(reference.playerID);
        x = participant.spawn.x;
        y = participant.spawn.y;
        hp = character.currentHP;
        state = character.characterState;
    }
    else if (runtime.type == MultiplayerActivityType::DUNGEON &&
             reference.kind == MultiplayerCombatantKind::MONSTER &&
             reference.roomID < dungeon.roomCount && reference.entityID > 0 &&
             reference.roomID != dungeon.loadedRoom)
    {
        const DungeonRoomRuntime& room = dungeon.roomRuntime[reference.roomID];
        const uint8_t index = reference.entityID - 1;
        if (room.persistenceReady && index < room.persistentEntityCount)
        {
            const PersistentEntity& persistent =
                room.entityStorage.compact.persistentEntities[index];
            if (persistent.type == ENTITY_MONSTER)
            {
                x = persistent.x;
                y = persistent.y;
                hp = static_cast<int16_t>(persistent.payload.monster.currentHP);
                state = static_cast<uint8_t>(persistent.payload.monster.state);
            }
        }
    }
    if (entity != nullptr)
    {
        x = entity->x;
        y = entity->y;
        hp = static_cast<int16_t>(entity->character.health.currentHP);
        state = static_cast<uint8_t>(entity->character.state);
    }
    if (actor)
    {
        event.actor = reference;
        event.actorX = x;
        event.actorY = y;
        event.actorHP = hp;
        event.actorState = state;
    }
    else
    {
        event.target = reference;
        event.targetX = x;
        event.targetY = y;
        event.targetHP = hp;
        event.targetState = state;
    }
}

bool emitCombatEvent(
    NetworkPacketType packetType,
    MultiplayerCombatEventType eventType,
    const MultiplayerCombatantRef& actor,
    const MultiplayerCombatantRef& target,
    const char* message)
{
    MultiplayerCombatEventPayload event{};
    event.activityID = runtime.activityID;
    event.combatID = multiplayerCombat->combatID;
    event.eventSequence = multiplayerCombat->nextEventSequence++;
    if (multiplayerCombat->nextEventSequence == 0)
        multiplayerCombat->nextEventSequence = 1;
    event.eventType = eventType;
    event.currentTurnIndex = multiplayerCombat->currentTurnIndex;
    event.round = multiplayerCombat->round;
    fillCombatEventState(event, actor, true);
    fillCombatEventState(event, target, false);
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        actor.roomID < NETWORK_MAX_DUNGEON_ROOMS)
        event.roomRevision = runtime.roomRevisions[actor.roomID];
    if (message != nullptr)
    {
        char combined[SHARED_MESSAGE_TEXT_SIZE] = {};
        snprintf(combined, sizeof(combined), "%s", message);
        if (eventType != MultiplayerCombatEventType::COMBAT_ENDED)
        {
            const MultiplayerCombatantRef* next = currentCombatRef();
            if (next != nullptr)
            {
                const char* nextName = combatantDisplayName(
                    *next, resolveVisibleCombatant(*next));
                const size_t used = strnlen(combined, sizeof(combined));
                if (used < sizeof(combined) - 1)
                    snprintf(combined + used, sizeof(combined) - used,
                             " %s's turn.", nextName);
            }
        }
        const size_t length = strnlen(combined, SHARED_MESSAGE_TEXT_SIZE - 1);
        memcpy(event.text, combined, length);
        event.text[length] = '\0';
        event.textLength = static_cast<uint8_t>(length);
        setGameMessage(event.text);
    }
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    return encodeMultiplayerCombatEvent(event, bytes, sizeof(bytes), size) &&
        queueReliableCombatPacket(packetType, event.eventSequence, bytes, size);
}

bool prepareCombatantRoom(const MultiplayerCombatantRef& reference)
{
    if (runtime.type != MultiplayerActivityType::DUNGEON ||
        reference.roomID == dungeon.loadedRoom) return true;
    if (!activateAuthoritativeDungeonRoom(reference.roomID, ENTRY_START))
        return false;
    for (uint8_t index = 0; index < multiplayerCombat->combatantCount; ++index)
    {
        MultiplayerCombatSlot& slot = multiplayerCombat->slots[index];
        if (slot.initiative.combatant.roomID != reference.roomID) continue;
        Entity* entity = resolveVisibleCombatant(slot.initiative.combatant);
        if (entity == nullptr) continue;
        entity->turn.turnActive = true;
        entity->turn.movementRemaining = slot.movementRemaining;
        entity->turn.moveActionUsed = slot.moveActionUsed;
        entity->turn.standardActionUsed = slot.standardActionUsed;
        entity->turn.fullDefense = slot.fullDefense;
    }
    return true;
}

void saveCombatRoomAndRestoreHost(uint8_t changedRoom)
{
    if (runtime.type == MultiplayerActivityType::DUNGEON &&
        changedRoom < NETWORK_MAX_DUNGEON_ROOMS)
    {
        if (++runtime.roomRevisions[changedRoom] == 0)
            runtime.roomRevisions[changedRoom] = 1;
        capturePlayersInLoadedDungeonRoom();
        refreshHostParticipantCharacters();
        captureMultiplayerRoomEffects(changedRoom);
    }
    if (runtime.type == MultiplayerActivityType::DUNGEON)
        restoreHostDungeonRoom();
}

void scheduleRoomRefreshForClients(uint8_t roomID)
{
    if (!multiplayerSession.isHost() ||
        runtime.type != MultiplayerActivityType::DUNGEON ||
        roomID >= NETWORK_MAX_DUNGEON_ROOMS) return;
    const uint32_t now = millis();
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        Participant& participant = runtime.participants[playerID];
        if (!participant.participating ||
            participant.spawn.roomID != roomID || participant.roomLoading)
            continue;
        participant.roomLoading = true;
        participant.ready = false;
        participant.startAcked = false;
        participant.transferRoom = roomID;
        participant.transferEpoch = runtime.roomRevisions[roomID];
        participant.transferIncludesGraph = false;
        participant.transitionEntry = ENTRY_START;
        participant.roomLoadStartedAt = now;
        participant.syncStage = SyncStage::PREPARE;
        participant.syncItem = 0;
        participant.retryCount = 0;
        participant.awaitingAck = false;
        participant.lastSendAt = 0;
        MP_DEBUGF("Refreshing Room %u revision %u for Player %u\n",
                  roomID, participant.transferEpoch, playerID);
    }
}

const char* combatantDisplayName(
    const MultiplayerCombatantRef& reference,
    const Entity* entity)
{
    if (reference.kind == MultiplayerCombatantKind::PLAYER &&
        isValidPlayerID(reference.playerID))
        return participantCharacter(reference.playerID).displayName;
    return entity != nullptr ? getEntityName(entity) : "Monster";
}

bool multiplayerCombatantsAreHostile(
    const Entity& attacker, const Entity& target)
{
    return attacker.character.team != TEAM_NEUTRAL &&
           target.character.team != TEAM_NEUTRAL &&
           attacker.character.team != target.character.team;
}

bool resolveMultiplayerAttack(
    const MultiplayerCombatActionRequestPayload& request,
    bool ranged)
{
    if (!prepareCombatantRoom(request.actor)) return false;
    Entity* actor = resolveVisibleCombatant(request.actor);
    Entity* target = resolveVisibleCombatant(request.target);
    if (actor == nullptr || target == nullptr ||
        request.actor.roomID != request.target.roomID ||
        actor->character.state != STATE_ALIVE ||
        target->character.state != STATE_ALIVE ||
        !multiplayerCombatantsAreHostile(*actor, *target))
    {
        restoreHostDungeonRoom();
        return false;
    }
    const int dx = abs(static_cast<int>(actor->x) - target->x);
    const int dy = abs(static_cast<int>(actor->y) - target->y);
    if (!ranged && (dx > 1 || dy > 1))
    {
        restoreHostDungeonRoom();
        return false;
    }
    const EquipmentSlot equipmentSlot =
        ranged ? SLOT_RANGED_WEAPON : SLOT_MELEE_WEAPON;
    const ItemInstance& item =
        actor->character.equipment.equipped[equipmentSlot];
    const Weapon* weapon = getWeapon(item.itemID);
    if (weapon == nullptr)
    {
        restoreHostDungeonRoom();
        return false;
    }
    int rangePenalty = 0;
    if (ranged)
    {
        if (!canSee(*actor) ||
            !hasLineOfSightBetweenFootprintsAt(
                *actor, actor->x, actor->y, *target))
        {
            restoreHostDungeonRoom();
            return false;
        }
        const int distanceFeet = getEntityGridDistance(*actor, *target) * 5;
        const int increments = weapon->rangeIncrement > 0
            ? (distanceFeet - 1) / weapon->rangeIncrement : 10;
        if (increments >= 10)
        {
            restoreHostDungeonRoom();
            return false;
        }
        rangePenalty = -2 * increments;
    }
    const int roll = rollDie(20);
    const int bonus = (ranged ? getRangedAttackBonus(actor->character)
                              : getMeleeAttackBonus(actor->character)) +
                      rangePenalty;
    const int armorClass = getArmorClass(target->character,
                                         target->turn.fullDefense ? 4 : 0);
    const bool hit = roll == 20 || (roll != 1 && roll + bonus >= armorClass);
    bool critical = false;
    if (hit && roll >= weapon->criticalThreat)
    {
        const int confirmation = rollDie(20);
        critical = confirmation == 20 ||
            (confirmation != 1 && confirmation + bonus >= armorClass);
    }
    int damage = 0;
    if (hit)
    {
        damage = rollDice(weapon->damageDice, weapon->damageSides) +
                 getItemWeaponDamageBonus(item);
        if (!ranged)
            damage += getAbilityModifier(actor->character, ABILITY_STRENGTH);
        if (damage < 1) damage = 1;
        if (critical) damage *= weapon->criticalMultiplier;
        damage += rollWeaponEnergyDamage(item, target->character);
        damageCharacter(target->character, damage);
        if (target->character.health.currentHP <= 0)
        {
            target->character.health.currentHP = 0;
            target->character.state = target->type == ENTITY_PLAYER
                ? STATE_UNCONSCIOUS : STATE_DEAD;
        }
        markEntityFootprintDirty(*target);
    }
    if (request.actor.kind == MultiplayerCombatantKind::PLAYER)
        copyCharacterToNetwork(request.actor.playerID, actor->character,
                               participantCharacter(request.actor.playerID));
    if (request.target.kind == MultiplayerCombatantKind::PLAYER)
        copyCharacterToNetwork(request.target.playerID, target->character,
                               participantCharacter(request.target.playerID));
    char message[SHARED_MESSAGE_TEXT_SIZE] = {};
    if (hit)
        snprintf(message, sizeof(message), "%s hits %s for %d.",
                 combatantDisplayName(request.actor, actor),
                 combatantDisplayName(request.target, target), damage);
    else
        snprintf(message, sizeof(message), "%s misses %s.",
                 combatantDisplayName(request.actor, actor),
                 combatantDisplayName(request.target, target));
    MultiplayerCombatSlot& combatSlot = multiplayerCombat->slots[
        multiplayerCombat->currentTurnIndex];
    combatSlot.standardActionUsed = true;
    const uint8_t roomID = request.actor.roomID;
    saveCombatRoomAndRestoreHost(roomID);
    if (!hasLivingMultiplayerHostile())
    {
        multiplayerCombat->ending = true;
        emitCombatEvent(NetworkPacketType::COMBAT_END,
                        MultiplayerCombatEventType::COMBAT_ENDED,
                        request.actor, request.target, "Combat ends.");
        return true;
    }
    advanceMultiplayerTurn();
    return emitCombatEvent(NetworkPacketType::COMBAT_ACTION_RESULT,
                           MultiplayerCombatEventType::ACTION_RESULT,
                           request.actor, request.target, message);
}

bool resolveMultiplayerAbility(
    const MultiplayerCombatActionRequestPayload& request)
{
    if ((request.flags & 0x01u) != 0) return false;
    if (!prepareCombatantRoom(request.actor)) return false;
    Entity* actor = resolveVisibleCombatant(request.actor);
    Entity* target = resolveVisibleCombatant(request.target);
    if (actor == nullptr || actor->character.state != STATE_ALIVE)
    {
        restoreHostDungeonRoom();
        return false;
    }
    const AbilityID abilityID = static_cast<AbilityID>(request.abilityID);
    AbilityResolution result{};
    if (isDirectionalAbility(abilityID))
        result = resolveAbilityInDirection(
            *actor, static_cast<Direction>(request.direction), abilityID);
    else if (isGroundTargetAbility(abilityID))
        result = resolveAbilityAt(
            *actor, request.targetX, request.targetY, abilityID);
    else
    {
        if (target == nullptr || request.actor.roomID != request.target.roomID)
        {
            restoreHostDungeonRoom();
            return false;
        }
        result = resolveAbility(*actor, target, abilityID,
            AbilityCastSource::NORMAL, static_cast<DamageType>(request.damageType));
    }
    if (result.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(result.result));
        restoreHostDungeonRoom();
        return false;
    }
    copyCharacterToNetwork(request.actor.playerID, actor->character,
                           participantCharacter(request.actor.playerID));
    if (target != nullptr && request.target.kind == MultiplayerCombatantKind::PLAYER)
        copyCharacterToNetwork(request.target.playerID, target->character,
                               participantCharacter(request.target.playerID));
    multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]
        .standardActionUsed = true;
    char message[SHARED_MESSAGE_TEXT_SIZE] = {};
    snprintf(message, sizeof(message), "%s casts %s.",
             combatantDisplayName(request.actor, actor), getAbilityName(abilityID));
    const uint8_t roomID = request.actor.roomID;
    saveCombatRoomAndRestoreHost(roomID);
    scheduleRoomRefreshForClients(roomID);
    if (!hasLivingMultiplayerHostile())
    {
        multiplayerCombat->ending = true;
        return emitCombatEvent(NetworkPacketType::COMBAT_END,
            MultiplayerCombatEventType::COMBAT_ENDED,
            request.actor, request.target, "Combat ends.");
    }
    advanceMultiplayerTurn();
    return emitCombatEvent(NetworkPacketType::COMBAT_ACTION_RESULT,
        MultiplayerCombatEventType::ACTION_RESULT,
        request.actor, request.target, message);
}

bool processAuthoritativeCombatAction(
    PlayerID sender, const MultiplayerCombatActionRequestPayload& request)
{
    const MultiplayerCombatantRef* current = currentCombatRef();
    if (current == nullptr || current->kind != MultiplayerCombatantKind::PLAYER ||
        current->playerID != sender || !combatRefsEqual(*current, request.actor))
        return false;
    const MultiplayerCombatSlot& currentSlot = multiplayerCombat->slots[
        multiplayerCombat->currentTurnIndex];
    if ((request.action == MultiplayerCombatActionType::MELEE_ATTACK ||
         request.action == MultiplayerCombatActionType::RANGED_ATTACK ||
         request.action == MultiplayerCombatActionType::CAST_ABILITY) &&
        currentSlot.standardActionUsed)
        return false;
    if (request.action == MultiplayerCombatActionType::FULL_DEFENSE &&
        (currentSlot.standardActionUsed || currentSlot.moveActionUsed))
        return false;
    switch (request.action)
    {
        case MultiplayerCombatActionType::MOVE:
        {
            if (!prepareCombatantRoom(request.actor)) return false;
            Entity* actor = resolveVisibleCombatant(request.actor);
            if (actor == nullptr)
            {
                restoreHostDungeonRoom();
                return false;
            }
            MultiplayerCombatSlot& slot = multiplayerCombat->slots[
                multiplayerCombat->currentTurnIndex];
            actor->turn.turnActive = true;
            actor->turn.movementRemaining = slot.movementRemaining;
            actor->turn.moveActionUsed = slot.moveActionUsed;
            actor->turn.standardActionUsed = slot.standardActionUsed;
            combat.waitingForPlayer = true;
            bool roomChanged = false;
            const bool moved = tryMovePlayerAuthoritative(
                dungeon, sender, static_cast<Direction>(request.direction),
                false, roomChanged);
            slot.movementRemaining = actor->turn.movementRemaining;
            slot.moveActionUsed = actor->turn.moveActionUsed;
            slot.standardActionUsed = actor->turn.standardActionUsed;
            capturePlayersInLoadedDungeonRoom();
            copyCharacterToNetwork(sender, actor->character,
                                   participantCharacter(sender));
            Participant& participant = runtime.participants[sender];
            participant.lastMoveSequence = request.actionSequence;
            participant.hasMoveSequence = true;
            sendAuthoritativePosition(
                sender, request.actionSequence,
                static_cast<Direction>(request.direction));
            checkAuthoritativeRoomCombat(sender);
            if (moved)
                saveCombatRoomAndRestoreHost(request.actor.roomID);
            else
                restoreHostDungeonRoom();
            if (multiplayerCombat->pendingSize != 0) return true;
            char message[SHARED_MESSAGE_TEXT_SIZE] = {};
            snprintf(message, sizeof(message), moved ? "%s moves." :
                     "%s cannot move there.",
                     combatantDisplayName(request.actor, actor));
            return emitCombatEvent(NetworkPacketType::COMBAT_ACTION_RESULT,
                MultiplayerCombatEventType::ACTION_RESULT,
                request.actor, MultiplayerCombatantRef{}, message);
        }
        case MultiplayerCombatActionType::MELEE_ATTACK:
            return resolveMultiplayerAttack(request, false);
        case MultiplayerCombatActionType::RANGED_ATTACK:
            return resolveMultiplayerAttack(request, true);
        case MultiplayerCombatActionType::CAST_ABILITY:
            return resolveMultiplayerAbility(request);
        case MultiplayerCombatActionType::FULL_DEFENSE:
        {
            if (!prepareCombatantRoom(request.actor)) return false;
            Entity* actor = resolveVisibleCombatant(request.actor);
            if (actor == nullptr)
            {
                restoreHostDungeonRoom();
                return false;
            }
            actor->turn.fullDefense = true;
            MultiplayerCombatSlot& slot = multiplayerCombat->slots[
                multiplayerCombat->currentTurnIndex];
            slot.standardActionUsed = true;
            slot.moveActionUsed = true;
            slot.fullDefense = true;
            char message[SHARED_MESSAGE_TEXT_SIZE] = {};
            snprintf(message, sizeof(message), "%s takes Full Defense.",
                     combatantDisplayName(request.actor, actor));
            saveCombatRoomAndRestoreHost(request.actor.roomID);
            advanceMultiplayerTurn();
            return emitCombatEvent(NetworkPacketType::COMBAT_TURN,
                MultiplayerCombatEventType::TURN_ADVANCE,
                request.actor, MultiplayerCombatantRef{}, message);
        }
        case MultiplayerCombatActionType::END_TURN:
        {
            char message[SHARED_MESSAGE_TEXT_SIZE] = {};
            snprintf(message, sizeof(message), "%s ends their turn.",
                     combatantDisplayName(request.actor,
                         resolveVisibleCombatant(request.actor)));
            advanceMultiplayerTurn();
            return emitCombatEvent(NetworkPacketType::COMBAT_TURN,
                MultiplayerCombatEventType::TURN_ADVANCE,
                request.actor, MultiplayerCombatantRef{}, message);
        }
        default:
            return false;
    }
}

void failParticipantLoad(PlayerID playerID)
{
    Participant& participant = runtime.participants[playerID];
    char message[64] = {};
    const NetworkCharacterState& character = participantCharacter(playerID);
    snprintf(message, sizeof(message), "%s could not load the %s.",
             character.displayName[0] != '\0'
                ? character.displayName : "Adventurer",
             runtime.type == MultiplayerActivityType::DUNGEON
                ? "dungeon" : "forest");
    removePlayerEntity(playerID);
    runtime.participantMask &= ~playerBit(playerID);
    participant.participating = false;
    participant.awaitingAck = false;
    sendActivityEndTo(playerID);
    broadcastPlayerDespawn(playerID);
    setGameMessage(message);
    MP_DEBUGF("Activity participant %u removed due load failure\n", playerID);
}
}

void initializeMultiplayerActivity()
{
    if (participantCharacters == nullptr)
    {
        participantCharacters = static_cast<NetworkCharacterState*>(
            ps_malloc(sizeof(NetworkCharacterState) * MAX_MULTIPLAYER_PLAYERS));
        if (participantCharacters != nullptr)
            memset(participantCharacters, 0,
                   sizeof(NetworkCharacterState) * MAX_MULTIPLAYER_PLAYERS);
    }
    if (multiplayerCombat == nullptr)
    {
        multiplayerCombat = static_cast<MultiplayerCombatRuntime*>(
            ps_malloc(sizeof(MultiplayerCombatRuntime)));
        if (multiplayerCombat != nullptr)
            *multiplayerCombat = MultiplayerCombatRuntime{};
    }
    if (multiplayerRoomEffects == nullptr)
    {
        multiplayerRoomEffects = static_cast<MultiplayerRoomEffectState*>(
            ps_malloc(sizeof(MultiplayerRoomEffectState) *
                      NETWORK_MAX_DUNGEON_ROOMS));
        if (multiplayerRoomEffects != nullptr)
            memset(multiplayerRoomEffects, 0,
                   sizeof(MultiplayerRoomEffectState) *
                   NETWORK_MAX_DUNGEON_ROOMS);
    }
    MP_DEBUGF("Stage3 PSRAM chars=%u combat=%u roomEffects=%u total=%u\n",
              static_cast<unsigned>(sizeof(NetworkCharacterState) *
                                    MAX_MULTIPLAYER_PLAYERS),
              static_cast<unsigned>(sizeof(MultiplayerCombatRuntime)),
              static_cast<unsigned>(sizeof(MultiplayerRoomEffectState) *
                                    NETWORK_MAX_DUNGEON_ROOMS),
              static_cast<unsigned>(sizeof(NetworkCharacterState) *
                                    MAX_MULTIPLAYER_PLAYERS +
                                    sizeof(MultiplayerCombatRuntime) +
                                    sizeof(MultiplayerRoomEffectState) *
                                    NETWORK_MAX_DUNGEON_ROOMS));
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

    if (multiplayerSession.isClient() && runtime.phase == ActivityPhase::NONE &&
        (runtime.pendingInviteAccepted || runtime.pendingInviteDeclined))
    {
        if ((runtime.travelResponseStartedAt == 0 ||
             !elapsed(now, runtime.travelResponseStartedAt,
                      TRAVEL_RESPONSE_TIMEOUT_MS)) &&
            (runtime.lastTravelResponseAt == 0 ||
             elapsed(now, runtime.lastTravelResponseAt,
                     TRAVEL_RESPONSE_RETRY_MS)))
            sendStoredTravelResponse(now);
    }

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

    if (multiplayerSession.isHost() &&
        (runtime.phase == ActivityPhase::LOADING || hasParticipantRoomLoading()))
    {
        if (runtime.phase == ActivityPhase::LOADING &&
            elapsed(now, runtime.phaseStartedAt, ACTIVITY_LOAD_TIMEOUT_MS))
        {
            for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS;
                 ++playerID)
                if (isParticipant(playerID) &&
                    !runtime.participants[playerID].ready)
                    failParticipantLoad(playerID);
        }

        for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        {
            Participant& participant = runtime.participants[playerID];
            if (!participant.participating) continue;
            if (participant.roomLoading && participant.roomLoadStartedAt != 0 &&
                elapsed(now, participant.roomLoadStartedAt,
                        ACTIVITY_LOAD_TIMEOUT_MS))
            {
                failParticipantLoad(playerID);
                continue;
            }
            if (participant.awaitingAck &&
                elapsed(now, participant.lastSendAt, RELIABLE_ACK_TIMEOUT_MS))
            {
                if (participant.retryCount >= RELIABLE_RETRY_LIMIT)
                {
                    failParticipantLoad(playerID);
                    continue;
                }
                participant.awaitingAck = false;
                ++participant.retryCount;
#if MULTIPLAYER_DEBUG
                if (multiplayerCombat != nullptr) ++multiplayerCombat->snapshotRetries;
#endif
                MP_DEBUGF("Reliable retry player=%u stage=%u item=%u retry=%u\n",
                          playerID,
                          static_cast<unsigned>(participant.syncStage),
                          participant.syncItem, participant.retryCount);
                if (participant.expectedAck.packetType ==
                    NetworkPacketType::ACTIVITY_START)
                {
                    sendTargetedActivityStart(playerID);
                    continue;
                }
            }
        }

        if (!hasReliablePacketInFlight() &&
            elapsed(now, runtime.lastReliableSendAt, RELIABLE_SEND_GAP_MS))
        {
            for (uint8_t checked = 0; checked < MAX_MULTIPLAYER_PLAYERS - 1;
                 ++checked)
            {
                PlayerID playerID = runtime.nextTransferPlayer;
                runtime.nextTransferPlayer = playerID + 1;
                if (runtime.nextTransferPlayer >= MAX_MULTIPLAYER_PLAYERS)
                    runtime.nextTransferPlayer = 1;
                Participant& participant = runtime.participants[playerID];
                normalizeSyncStage(participant);
                if (!participant.participating || participant.ready ||
                    participant.awaitingAck ||
                    participant.syncStage == SyncStage::COMPLETE)
                    continue;
                if (!sendCurrentReliableItem(playerID))
                {
                    participant.lastSendAt = now;
                    runtime.lastReliableSendAt = now;
                }
                break;
            }
        }
        if (runtime.phase == ActivityPhase::LOADING && allParticipantsReady())
            startHostActivity(now);
    }

    if (multiplayerSession.isClient() && runtime.phase == ActivityPhase::ACTIVE)
    {
        Participant& local = runtime.participants[runtime.localPlayerID];
        if (local.transitionPending &&
            (local.lastTransitionRequestAt == 0 ||
             elapsed(now, local.lastTransitionRequestAt,
                     RELIABLE_ACK_TIMEOUT_MS)))
            sendPendingRoomTransitionRequest(local, now);
    }

    if (multiplayerSession.isClient() && runtime.phase == ActivityPhase::LOADING &&
        clientLoadComplete() &&
        (runtime.lastReadyAt == 0 ||
         elapsed(now, runtime.lastReadyAt, READY_RETRY_MS)))
    {
        sendActivityReady(runtime.clientRoomLoading);
        runtime.lastReadyAt = now;
    }
    else if (multiplayerSession.isClient() &&
             runtime.phase == ActivityPhase::LOADING)
    {
        logClientLoadBlockers(now);
        if (elapsed(now, runtime.phaseStartedAt, ACTIVITY_LOAD_TIMEOUT_MS + 5000))
        {
            endActivityLocally(runtime.type == MultiplayerActivityType::DUNGEON
                ? "Unable to join dungeon." : "Unable to join forest.");
            return;
        }
    }

#if MULTIPLAYER_DEBUG
    if (elapsed(now, runtime.lastDebugSummaryAt, DEBUG_SUMMARY_INTERVAL_MS))
    {
        runtime.lastDebugSummaryAt = now;
        MP_DEBUGF("MP counters rxQueueDrops=%lu immediateSendFailures=%lu "
                  "sendCallbackFailures=%lu sendCallbackSuccess=%lu "
                  "snapshotRetries=%u chunkDuplicates=%u chunkRejects=%u\n",
                  static_cast<unsigned long>(
                      multiplayerTransport.getDroppedReceiveCount()),
                  static_cast<unsigned long>(
                      multiplayerTransport.getImmediateSendFailureCount()),
                  static_cast<unsigned long>(
                      multiplayerTransport.getSendCompletionFailureCount()),
                  static_cast<unsigned long>(
                      multiplayerTransport.getSendCompletionSuccessCount()),
                  multiplayerCombat != nullptr ? multiplayerCombat->snapshotRetries : 0,
                  multiplayerCombat != nullptr ? multiplayerCombat->snapshotChunkDuplicates : 0,
                  multiplayerCombat != nullptr ? multiplayerCombat->snapshotChunkRejects : 0);
    }
#endif

    if (multiplayerSession.isHost() && runtime.phase == ActivityPhase::ACTIVE &&
        runtime.startRepeatsRemaining > 0 &&
        elapsed(now, runtime.lastStartAt, START_REPEAT_MS))
    {
        broadcastActivityStart(now);
        runtime.startRepeatsRemaining--;
    }
    updateMultiplayerCombat(now);
}

bool requestMultiplayerTravel(MultiplayerActivityType activityType)
{
    if (participantCharacters == nullptr)
    {
        setGameMessage("Multiplayer memory unavailable.");
        return true;
    }
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
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = true;
    runtime.pendingInviteDeclined = false;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    runtime.participantMask = playerBit(multiplayerSession.getLocalPlayerID());
    runtime.travelResponseStartedAt = millis();
    runtime.lastTravelResponseAt = 0;
    sendStoredTravelResponse(millis());
    setGameMessage("Joining party activity...");
}

void declinePendingMultiplayerTravel()
{
    if (!runtime.pendingInvite || !multiplayerSession.isClient()) return;
    runtime.pendingInvite = false;
    runtime.pendingInviteAccepted = false;
    runtime.pendingInviteDeclined = true;
    runtime.localPlayerID = multiplayerSession.getLocalPlayerID();
    runtime.participantMask = 0;
    runtime.travelResponseStartedAt = millis();
    runtime.lastTravelResponseAt = 0;
    sendStoredTravelResponse(millis());
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
    if (isMultiplayerCombatActive() && !isLocalPlayersMultiplayerTurn())
        return true;
    const PlayerID localPlayerID = runtime.localPlayerID;
    Participant& participant = runtime.participants[localPlayerID];
    const uint16_t sequence = ++runtime.localMovementSequence;

    if (localMoveTargetsDungeonDoor(static_cast<Direction>(direction)))
    {
        if (participant.roomLoading || participant.transitionPending)
            return true;
        if (isMultiplayerCombatActive() &&
            multiplayerCombat->slots[multiplayerCombat->currentTurnIndex]
                    .movementRemaining == 0)
            return true;
        if (multiplayerSession.isHost())
        {
            uint8_t destinationRoom = NETWORK_NO_ROOM;
            RoomEntry entry = ENTRY_START;
            if (resolvePlayerDoorTransition(
                    localPlayerID, static_cast<Direction>(direction),
                    destinationRoom, entry))
                moved = beginIndependentRoomTransition(
                    localPlayerID, destinationRoom, entry, sequence, millis());
            return true;
        }
        participant.transitionPending = true;
        participant.hasTransitionSequence = true;
        participant.lastTransitionSequence = sequence;
        participant.pendingTransitionDirection = direction;
        participant.pendingTransitionSourceRoom = runtime.currentRoom;
        participant.lastTransitionRequestAt = 0;
        sendPendingRoomTransitionRequest(participant, millis());
        setGameMessage("Entering next room...");
        return true;
    }

    if (isMultiplayerCombatActive())
    {
        const bool accepted = requestMultiplayerCombatMove(direction);
        moved = multiplayerSession.isHost() && accepted;
        return true;
    }

    if (multiplayerSession.isHost())
    {
        refreshHostParticipantCharacters();
        bool roomChanged = false;
        moved = tryMovePlayerAuthoritative(
            dungeon, localPlayerID, static_cast<Direction>(direction),
            true, roomChanged);
        checkAuthoritativeRoomCombat(localPlayerID);
        if (isMultiplayerCombatActive())
        {
            uint8_t activeCount = 0;
            Entity* activeEntities = getActiveMapEntities(activeCount);
            Entity* activePlayer = activeEntities != nullptr
                ? getPlayerEntityByOwner(
                    activeEntities, activeCount, localPlayerID) : nullptr;
            if (activePlayer != nullptr)
            {
                MultiplayerCombatSlot& slot = multiplayerCombat->slots[
                    multiplayerCombat->currentTurnIndex];
                slot.movementRemaining = activePlayer->turn.movementRemaining;
                slot.moveActionUsed = activePlayer->turn.moveActionUsed;
                slot.standardActionUsed =
                    activePlayer->turn.standardActionUsed;
            }
        }
        if (runtime.type == MultiplayerActivityType::DUNGEON)
            capturePlayersInLoadedDungeonRoom();
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

bool publishSharedGameplayMessage(
    const char* message,
    SharedMessageCategory category)
{
    if (message == nullptr || message[0] == '\0' ||
        runtime.phase == ActivityPhase::NONE ||
        !multiplayerSession.isHost()) return false;
    SharedGameplayMessagePayload payload{};
    payload.activityID = runtime.activityID;
    payload.messageSequence = runtime.nextSharedMessageSequence++;
    if (runtime.nextSharedMessageSequence == 0)
        runtime.nextSharedMessageSequence = 1;
    payload.category = category;
    const size_t length = strnlen(message, SHARED_MESSAGE_TEXT_SIZE - 1);
    memcpy(payload.text, message, length);
    payload.text[length] = '\0';
    payload.textLength = static_cast<uint8_t>(length);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (!encodeSharedGameplayMessage(payload, bytes, sizeof(bytes), size))
        return false;
    setGameMessage(payload.text);
    multiplayerSession.broadcastAuthoritativeGameplayPacket(
        NetworkPacketType::SHARED_GAMEPLAY_MESSAGE, bytes, size,
        NETWORK_FLAG_CONTROL_EVENT);
    MP_DEBUGF("Shared message seq %u: %s\n",
              payload.messageSequence, payload.text);
    return true;
}

uint8_t getMultiplayerPlayerRoom(PlayerID playerID)
{
    if (!isParticipant(playerID)) return NETWORK_NO_ROOM;
    return runtime.participants[playerID].spawn.roomID;
}

uint16_t getMultiplayerRoomRevision(uint8_t roomID)
{
    return roomID < NETWORK_MAX_DUNGEON_ROOMS
        ? runtime.roomRevisions[roomID] : 0;
}

bool isMultiplayerCombatActive()
{
    return multiplayerCombat != nullptr && multiplayerCombat->active;
}

bool startMultiplayerCombatFromCurrentRoom()
{
    if (!isMultiplayerExplorationActive() || !multiplayerSession.isHost() ||
        multiplayerCombat == nullptr) return false;
    if (multiplayerCombat->active)
    {
        if (multiplayerCombat->pendingSize != 0) return true;
        uint8_t entityCount = 0;
        Entity* entities = getActiveMapEntities(entityCount);
        const uint8_t roomID = getCombatRoomID();
        uint8_t added = 0;
        for (uint8_t index = 0; entities != nullptr && index < entityCount &&
             multiplayerCombat->combatantCount < MAX_COMBATANTS; ++index)
        {
            Entity& entity = entities[index];
            if (!entity.active || entity.type != ENTITY_MONSTER ||
                entity.character.state != STATE_ALIVE) continue;
            MultiplayerCombatantRef reference{};
            reference.kind = MultiplayerCombatantKind::MONSTER;
            reference.playerID = INVALID_PLAYER_ID;
            reference.roomID = roomID;
            reference.entityID = index + 1;
            if (findCombatSlot(reference) >= 0) continue;
            entity.awareOfPlayer = true;
            entity.revealedToPlayer = true;
            MultiplayerCombatSlot& slot = multiplayerCombat->slots[
                multiplayerCombat->combatantCount++];
            slot.initiative.combatant = reference;
            slot.initiative.initiative = static_cast<int8_t>(
                rollDie(20) +
                getAbilityModifier(entity.character.abilities.dexterity));
            ++added;
        }
        if (added == 0) return true;
        const uint8_t firstFuture = static_cast<uint8_t>(
            multiplayerCombat->currentTurnIndex + 1);
        for (uint8_t left = firstFuture;
             left + 1 < multiplayerCombat->combatantCount; ++left)
            for (uint8_t right = left + 1;
                 right < multiplayerCombat->combatantCount; ++right)
                if (multiplayerCombat->slots[right].initiative.initiative >
                    multiplayerCombat->slots[left].initiative.initiative)
                {
                    MultiplayerCombatSlot swap = multiplayerCombat->slots[left];
                    multiplayerCombat->slots[left] = multiplayerCombat->slots[right];
                    multiplayerCombat->slots[right] = swap;
                }
        MultiplayerCombatBeginPayload update{};
        update.activityID = runtime.activityID;
        update.combatID = multiplayerCombat->combatID;
        update.eventSequence = multiplayerCombat->nextEventSequence++;
        update.combatantCount = multiplayerCombat->combatantCount;
        update.currentTurnIndex = multiplayerCombat->currentTurnIndex;
        update.round = multiplayerCombat->round;
        for (uint8_t index = 0; index < update.combatantCount; ++index)
            update.combatants[index] =
                multiplayerCombat->slots[index].initiative;
        uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
        size_t size = 0;
        if (encodeMultiplayerCombatBegin(update, bytes, sizeof(bytes), size))
            queueReliableCombatPacket(NetworkPacketType::COMBAT_BEGIN,
                                      update.eventSequence, bytes, size);
        setGameMessage("More enemies join combat!");
        return true;
    }
    *multiplayerCombat = MultiplayerCombatRuntime{};
    multiplayerCombat->active = true;
    multiplayerCombat->combatID = static_cast<uint16_t>(esp_random());
    if (multiplayerCombat->combatID == 0) multiplayerCombat->combatID = 1;
    multiplayerCombat->nextEventSequence = 2;
    multiplayerCombat->combatantCount = 0;

    refreshHostParticipantCharacters();
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID) ||
            participantCharacter(playerID).currentHP <= 0 ||
            participantCharacter(playerID).characterState != STATE_ALIVE ||
            multiplayerCombat->combatantCount >= MAX_COMBATANTS) continue;
        MultiplayerCombatSlot& slot = multiplayerCombat->slots[
            multiplayerCombat->combatantCount++];
        slot.initiative.combatant.kind = MultiplayerCombatantKind::PLAYER;
        slot.initiative.combatant.playerID = playerID;
        slot.initiative.combatant.roomID =
            runtime.type == MultiplayerActivityType::FOREST
                ? 0 : runtime.participants[playerID].spawn.roomID;
        const int initiative = rollDie(20) +
            getAbilityModifier(participantCharacter(playerID).abilityScores[1]);
        slot.initiative.initiative = static_cast<int8_t>(initiative);
        participantCharacter(playerID).initiative =
            static_cast<int8_t>(initiative);
        MP_DEBUGF("Combat %u added P%u initiative %d room=%u\n",
                  multiplayerCombat->combatID, playerID, initiative,
                  slot.initiative.combatant.roomID);
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    const uint8_t triggerRoom = getCombatRoomID();
    for (uint8_t index = 0; entities != nullptr && index < entityCount &&
         multiplayerCombat->combatantCount < MAX_COMBATANTS; ++index)
    {
        Entity& entity = entities[index];
        if (!entity.active || entity.type != ENTITY_MONSTER ||
            entity.character.state != STATE_ALIVE ||
            entity.character.health.currentHP <= 0) continue;
        entity.awareOfPlayer = true;
        entity.revealedToPlayer = true;
        MultiplayerCombatSlot& slot = multiplayerCombat->slots[
            multiplayerCombat->combatantCount++];
        slot.initiative.combatant.kind = MultiplayerCombatantKind::MONSTER;
        slot.initiative.combatant.playerID = INVALID_PLAYER_ID;
        slot.initiative.combatant.roomID = triggerRoom;
        slot.initiative.combatant.entityID = index + 1;
        const int initiative = rollDie(20) +
            getAbilityModifier(entity.character.abilities.dexterity);
        slot.initiative.initiative = static_cast<int8_t>(initiative);
        entity.character.initiative = initiative;
        MP_DEBUGF("Combat %u added monster R%u:E%u initiative %d\n",
                  multiplayerCombat->combatID, triggerRoom, index + 1,
                  initiative);
    }
    if (multiplayerCombat->combatantCount < 2 || !hasLivingMultiplayerHostile())
    {
        multiplayerCombat->active = false;
        return false;
    }
    for (uint8_t left = 0; left + 1 < multiplayerCombat->combatantCount; ++left)
        for (uint8_t right = left + 1;
             right < multiplayerCombat->combatantCount; ++right)
            if (multiplayerCombat->slots[right].initiative.initiative >
                multiplayerCombat->slots[left].initiative.initiative)
            {
                MultiplayerCombatSlot swap = multiplayerCombat->slots[left];
                multiplayerCombat->slots[left] = multiplayerCombat->slots[right];
                multiplayerCombat->slots[right] = swap;
            }
    resetMultiplayerTurnState(multiplayerCombat->slots[0]);
    combat.active = true;
    combat.phase = COMBAT_TURN;
    combat.combatantCount = 0;
    combat.currentTurnIndex = 0;
    combat.combatRound = 1;
    combat.waitingForPlayer = isLocalPlayersMultiplayerTurn();

    MultiplayerCombatBeginPayload begin{};
    begin.activityID = runtime.activityID;
    begin.combatID = multiplayerCombat->combatID;
    begin.eventSequence = 1;
    begin.combatantCount = multiplayerCombat->combatantCount;
    begin.currentTurnIndex = 0;
    begin.round = 1;
    for (uint8_t index = 0; index < begin.combatantCount; ++index)
        begin.combatants[index] = multiplayerCombat->slots[index].initiative;
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (!encodeMultiplayerCombatBegin(begin, bytes, sizeof(bytes), size) ||
        !queueReliableCombatPacket(
            NetworkPacketType::COMBAT_BEGIN, begin.eventSequence, bytes, size))
    {
        multiplayerCombat->active = false;
        combat.active = false;
        return false;
    }
    setGameMessage("Combat begins!");
    MP_DEBUGF("Combat %u started in Room %u with %u combatants\n",
              multiplayerCombat->combatID, triggerRoom,
              multiplayerCombat->combatantCount);
    setFullMapRedraw();
    return true;
}

Entity* getVisibleMultiplayerCurrentCombatant()
{
    const MultiplayerCombatantRef* reference = currentCombatRef();
    return reference != nullptr ? resolveVisibleCombatant(*reference) : nullptr;
}

bool isLocalPlayersMultiplayerTurn()
{
    const MultiplayerCombatantRef* reference = currentCombatRef();
    return reference != nullptr &&
           reference->kind == MultiplayerCombatantKind::PLAYER &&
           reference->playerID == runtime.localPlayerID;
}

bool submitMultiplayerCombatRequest(MultiplayerCombatActionRequestPayload request)
{
    if (!isLocalPlayersMultiplayerTurn() || multiplayerCombat->ending ||
        multiplayerCombat->pendingSize != 0)
        return false;
    request.activityID = runtime.activityID;
    request.combatID = multiplayerCombat->combatID;
    request.actionSequence = ++multiplayerCombat->localActionSequence;
    if (multiplayerCombat->localActionSequence == 0)
        request.actionSequence = ++multiplayerCombat->localActionSequence;
    request.actor = *currentCombatRef();
    if (multiplayerSession.isHost())
        return processAuthoritativeCombatAction(runtime.localPlayerID, request);
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    if (!encodeMultiplayerCombatActionRequest(
            request, bytes, sizeof(bytes), size)) return false;
    multiplayerCombat->pendingPacketType =
        NetworkPacketType::COMBAT_ACTION_REQUEST;
    multiplayerCombat->pendingSize = static_cast<uint16_t>(size);
    multiplayerCombat->retryCount = 0;
    memcpy(multiplayerCombat->pendingBytes, bytes, size);
    multiplayerCombat->lastSendAt = millis();
    return sendToHost(NetworkPacketType::COMBAT_ACTION_REQUEST, bytes, size,
                      NETWORK_FLAG_CONTROL_EVENT);
}

bool requestMultiplayerCombatAttack(bool ranged, const Entity* target)
{
    if (target == nullptr) return false;
    MultiplayerCombatActionRequestPayload request{};
    request.action = ranged ? MultiplayerCombatActionType::RANGED_ATTACK
                            : MultiplayerCombatActionType::MELEE_ATTACK;
    request.target = combatRefForEntity(*target);
    request.direction = static_cast<uint8_t>(moveDirection);
    return request.target.kind != MultiplayerCombatantKind::NONE &&
           submitMultiplayerCombatRequest(request);
}

bool requestMultiplayerCombatMove(uint8_t direction)
{
    if (direction >= 8) return false;
    MultiplayerCombatActionRequestPayload request{};
    request.action = MultiplayerCombatActionType::MOVE;
    request.direction = direction;
    return submitMultiplayerCombatRequest(request);
}

bool requestMultiplayerCombatAbility(
    uint16_t abilityID,
    const Entity* target,
    int8_t targetX,
    int8_t targetY,
    uint8_t direction,
    uint8_t damageType,
    uint8_t flags)
{
    MultiplayerCombatActionRequestPayload request{};
    request.action = MultiplayerCombatActionType::CAST_ABILITY;
    if (target != nullptr) request.target = combatRefForEntity(*target);
    request.abilityID = abilityID;
    request.targetX = targetX;
    request.targetY = targetY;
    request.direction = direction;
    request.damageType = damageType;
    request.flags = flags;
    return submitMultiplayerCombatRequest(request);
}

bool requestMultiplayerCombatEndTurn()
{
    MultiplayerCombatActionRequestPayload request{};
    request.action = MultiplayerCombatActionType::END_TURN;
    request.direction = static_cast<uint8_t>(moveDirection);
    return submitMultiplayerCombatRequest(request);
}

bool requestMultiplayerFullDefense()
{
    MultiplayerCombatActionRequestPayload request{};
    request.action = MultiplayerCombatActionType::FULL_DEFENSE;
    request.direction = static_cast<uint8_t>(moveDirection);
    return submitMultiplayerCombatRequest(request);
}

bool isCurrentMultiplayerCombatant(const Entity& entity)
{
    const MultiplayerCombatantRef* current = currentCombatRef();
    return current != nullptr && combatRefsEqual(*current, combatRefForEntity(entity));
}

bool isMultiplayerCombatParticipant(const Entity& entity)
{
    return isMultiplayerCombatActive() &&
           findCombatSlot(combatRefForEntity(entity)) >= 0;
}

void updateMultiplayerCombat(uint32_t now)
{
    if (multiplayerCombat == nullptr || !multiplayerCombat->active) return;
    if (multiplayerSession.isClient() &&
        multiplayerCombat->pendingPacketType ==
            NetworkPacketType::COMBAT_ACTION_REQUEST &&
        multiplayerCombat->pendingSize != 0)
    {
        if (elapsed(now, multiplayerCombat->lastSendAt,
                    RELIABLE_ACK_TIMEOUT_MS))
        {
            if (multiplayerCombat->retryCount < RELIABLE_RETRY_LIMIT)
            {
                ++multiplayerCombat->retryCount;
                sendToHost(NetworkPacketType::COMBAT_ACTION_REQUEST,
                           multiplayerCombat->pendingBytes,
                           multiplayerCombat->pendingSize,
                           NETWORK_FLAG_CONTROL_EVENT);
                multiplayerCombat->lastSendAt = now;
            }
            else
            {
                multiplayerCombat->pendingSize = 0;
                multiplayerCombat->retryCount = 0;
                setGameMessage("Combat action was not accepted.");
            }
        }
        return;
    }
    if (multiplayerSession.isHost() && multiplayerCombat->pendingSize != 0)
    {
        if (elapsed(now, multiplayerCombat->lastSendAt, RELIABLE_ACK_TIMEOUT_MS))
        {
            if (multiplayerCombat->retryCount < RELIABLE_RETRY_LIMIT)
            {
                ++multiplayerCombat->retryCount;
                sendPendingCombatPacket();
            }
            else
            {
                MP_DEBUGF("Combat event %u timed out mask=%02X\n",
                          multiplayerCombat->pendingEventSequence,
                          multiplayerCombat->pendingPlayerMask);
                multiplayerCombat->pendingPlayerMask = 0;
                multiplayerCombat->pendingSize = 0;
            }
        }
        return;
    }
    if (multiplayerCombat->ending)
    {
        multiplayerCombat->active = false;
        combat.active = false;
        combat.phase = COMBAT_NONE;
        combat.waitingForPlayer = false;
        setFullMapRedraw();
        return;
    }
    if (!multiplayerSession.isHost()) return;
    const MultiplayerCombatantRef* current = currentCombatRef();
    if (current != nullptr && !multiplayerCombatantAlive(*current))
    {
        const MultiplayerCombatantRef skipped = *current;
        if (!hasLivingMultiplayerHostile())
        {
            multiplayerCombat->ending = true;
            emitCombatEvent(NetworkPacketType::COMBAT_END,
                MultiplayerCombatEventType::COMBAT_ENDED,
                skipped, MultiplayerCombatantRef{}, "Combat ends.");
            return;
        }
        advanceMultiplayerTurn();
        emitCombatEvent(NetworkPacketType::COMBAT_TURN,
            MultiplayerCombatEventType::TURN_ADVANCE,
            skipped, MultiplayerCombatantRef{}, "Turn skipped.");
        return;
    }
    if (current == nullptr || current->kind != MultiplayerCombatantKind::MONSTER)
        return;
    if (!prepareCombatantRoom(*current)) return;
    Entity* monster = resolveVisibleCombatant(*current);
    Entity* target = nullptr;
    MultiplayerCombatantRef targetRef{};
    int bestDistance = 1000;
    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!isParticipant(playerID) ||
            (runtime.type == MultiplayerActivityType::DUNGEON &&
             runtime.participants[playerID].spawn.roomID != current->roomID) ||
            participantCharacter(playerID).currentHP <= 0) continue;
        uint8_t activeCount = 0;
        Entity* activeEntities = getActiveMapEntities(activeCount);
        Entity* candidate = activeEntities != nullptr
            ? getPlayerEntityByOwner(activeEntities, activeCount, playerID)
            : nullptr;
        if (candidate == nullptr || monster == nullptr) continue;
        const int distance = abs(static_cast<int>(candidate->x) - monster->x) +
                             abs(static_cast<int>(candidate->y) - monster->y);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            target = candidate;
            targetRef = combatRefForEntity(*candidate);
        }
    }
    MultiplayerCombatActionRequestPayload request{};
    request.actor = *current;
    request.target = targetRef;
    request.direction = 0;
    bool resolved = false;
    if (target != nullptr && monster != nullptr)
    {
        uint8_t steps = static_cast<uint8_t>(
            max(1, static_cast<int>(monster->character.speed)));
        while (steps-- > 0 &&
               (abs(static_cast<int>(target->x) - monster->x) > 1 ||
                abs(static_cast<int>(target->y) - monster->y) > 1))
        {
            const int dx = target->x == monster->x ? 0
                : target->x > monster->x ? 1 : -1;
            const int dy = target->y == monster->y ? 0
                : target->y > monster->y ? 1 : -1;
            int nextX = monster->x + dx;
            int nextY = monster->y;
            if (!canMonsterMoveTo(monster, nextX, nextY))
            {
                nextX = monster->x;
                nextY = monster->y + dy;
            }
            if (!canMonsterMoveTo(monster, nextX, nextY)) break;
            markEntityFootprintDirty(*monster);
            monster->x = static_cast<uint8_t>(nextX);
            monster->y = static_cast<uint8_t>(nextY);
            markEntityFootprintDirty(*monster);
        }
        if (abs(static_cast<int>(target->x) - monster->x) <= 1 &&
            abs(static_cast<int>(target->y) - monster->y) <= 1)
            resolved = resolveMultiplayerAttack(request, false);
    }
    if (!resolved)
    {
        const MultiplayerCombatantRef actor = *current;
        saveCombatRoomAndRestoreHost(actor.roomID);
        advanceMultiplayerTurn();
        emitCombatEvent(NetworkPacketType::COMBAT_TURN,
            MultiplayerCombatEventType::TURN_ADVANCE,
            actor, MultiplayerCombatantRef{}, "Monster holds position.");
    }
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
