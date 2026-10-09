#ifndef PATHFINDERMINIEXTREME_025_NETWORK_PROTOCOL_H
#define PATHFINDERMINIEXTREME_025_NETWORK_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "multiplayer_types.h"

constexpr uint8_t MULTIPLAYER_PROTOCOL_VERSION = 3;
constexpr uint16_t MULTIPLAYER_PACKET_MAGIC = 0x504D;
constexpr size_t ESPNOW_MAX_PACKET_SIZE = 250;
constexpr size_t NETWORK_PACKET_HEADER_SIZE = 14;
constexpr size_t NETWORK_MAX_PAYLOAD_SIZE =
    ESPNOW_MAX_PACKET_SIZE - NETWORK_PACKET_HEADER_SIZE;
constexpr uint8_t NETWORK_PLAYER_NAME_SIZE = 16;
constexpr uint8_t NETWORK_CHARACTER_CLASS_COUNT = 4;
constexpr uint16_t DUNGEON_GENERATION_VERSION = 1;
constexpr size_t NETWORK_PLAYER_PROFILE_SIZE = 22;
constexpr size_t DISCOVERY_BEACON_PAYLOAD_SIZE = 24;
constexpr size_t NETWORK_CHARACTER_STATE_SIZE = 30;
constexpr size_t PLAYER_MOVE_REQUEST_PAYLOAD_SIZE = 7;
constexpr size_t PLAYER_POSITION_PAYLOAD_SIZE = 11;
constexpr size_t PLAYER_ROOM_CHANGE_PAYLOAD_SIZE = 11;
constexpr size_t SNAPSHOT_CHUNK_DATA_SIZE = 200;
constexpr size_t ACTIVITY_ACK_PAYLOAD_SIZE = 11;
constexpr uint8_t MAX_SNAPSHOT_CHUNKS = 4;
constexpr uint8_t NETWORK_NO_ROOM = UINT8_MAX;
constexpr uint8_t NETWORK_MAX_DUNGEON_ROOMS = 12;

static_assert(NETWORK_PACKET_HEADER_SIZE == 14,
              "Network header wire size changed unexpectedly");
static_assert(PLAYER_POSITION_PAYLOAD_SIZE <= 12,
              "Authoritative movement updates must remain very small");

enum class NetworkPacketType : uint8_t
{
    DISCOVERY_BEACON = 1,
    JOIN_REQUEST,
    JOIN_ACCEPT,
    JOIN_REJECT,
    PLAYER_JOINED,
    PLAYER_LEFT,
    HEARTBEAT,

    TRAVEL_INVITE,
    TRAVEL_ACCEPT,
    TRAVEL_DECLINE,
    ACTIVITY_PREPARE,
    ACTIVITY_READY,
    ACTIVITY_START,
    ACTIVITY_LEAVE,
    ACTIVITY_END,

    DUNGEON_BEGIN = 16,
    DUNGEON_GRAPH,
    SNAPSHOT_CHUNK,
    FOREST_BEGIN,
    PLAYER_CHARACTER_STATE,
    PLAYER_SPAWN,
    PLAYER_DESPAWN,
    ROOM_TRANSITION,
    ROOM_READY,
    ACK,

    PLAYER_MOVE_REQUEST = 32,
    PLAYER_POSITION,
    PLAYER_ROOM_CHANGE,

    COMBAT_BEGIN = 48,
    COMBAT_STATE,
    COMBAT_ACTION_REQUEST,
    COMBAT_ACTION_RESULT,
    COMBAT_TURN,

    TRADE_REQUEST = 64,
    TRADE_STATE,
    TRADE_CONFIRM,

    QUEST_STATE = 80,
    QUEST_CONTRIBUTION,

    PING = 96,
    PONG,
    ERROR_PACKET
};

enum NetworkPacketFlag : uint8_t
{
    NETWORK_FLAG_NONE = 0,
    NETWORK_FLAG_CONTROL_EVENT = 1 << 0,
    NETWORK_FLAG_SNAPSHOT_BEGIN = 1 << 1,
    NETWORK_FLAG_SNAPSHOT_END = 1 << 2
};

enum class MultiplayerAvailability : uint8_t
{
    AVAILABLE,
    IN_PARTY,
    IN_DUNGEON,
    IN_COMBAT,
    BUSY
};

enum class JoinRejectReason : uint8_t
{
    NONE,
    PROTOCOL_MISMATCH,
    SESSION_FULL,
    SESSION_UNAVAILABLE,
    INVALID_REQUEST
};

enum class PlayerLeaveReason : uint8_t
{
    LEFT_PARTY,
    CONNECTION_TIMEOUT,
    HOST_ENDED_SESSION
};

enum class DungeonSyncMode : uint8_t
{
    AUTHORITATIVE_GRAPH_AND_ROOM_STATE = 1
};

enum class MultiplayerActivityType : uint8_t
{
    NONE,
    FOREST,
    DUNGEON
};

enum class MultiplayerMemberLocation : uint8_t
{
    TOWN,
    CONNECTING,
    FOREST,
    DUNGEON,
    DISCONNECTED
};

enum class SnapshotType : uint8_t
{
    DUNGEON_GRAPH = 1,
    FOREST_STATE,
    DUNGEON_ROOM,
    DUNGEON_ROOM_DETAIL
};

struct NetworkPacketHeader
{
    uint16_t magic = MULTIPLAYER_PACKET_MAGIC;
    uint8_t protocolVersion = MULTIPLAYER_PROTOCOL_VERSION;
    NetworkPacketType packetType = NetworkPacketType::ERROR_PACKET;
    PlayerID senderPlayerID = INVALID_PLAYER_ID;
    uint8_t flags = NETWORK_FLAG_NONE;
    uint16_t sequence = 0;
    uint16_t payloadSize = 0;
    uint32_t sessionID = 0;
};

struct NetworkPlayerProfile
{
    uint32_t deviceID = 0;
    uint8_t level = 0;
    uint8_t characterClass = 0;
    char displayName[NETWORK_PLAYER_NAME_SIZE] = {};
};

struct DiscoveryBeaconPayload
{
    NetworkPlayerProfile profile{};
    MultiplayerAvailability availability = MultiplayerAvailability::BUSY;
    bool acceptingPlayers = false;
};

struct JoinRequestPayload
{
    NetworkPlayerProfile profile{};
};

struct JoinAcceptPayload
{
    PlayerID assignedPlayerID = INVALID_PLAYER_ID;
    uint8_t playerCapacity = MAX_MULTIPLAYER_PLAYERS;
};

struct JoinRejectPayload
{
    JoinRejectReason reason = JoinRejectReason::INVALID_REQUEST;
};

struct PlayerJoinedPayload
{
    PlayerID playerID = INVALID_PLAYER_ID;
    NetworkPlayerProfile profile{};
};

struct PlayerLeftPayload
{
    PlayerID playerID = INVALID_PLAYER_ID;
    PlayerLeaveReason reason = PlayerLeaveReason::LEFT_PARTY;
};

struct DungeonBeginPayload
{
    uint32_t activityID = 0;
    DungeonSyncMode syncMode =
        DungeonSyncMode::AUTHORITATIVE_GRAPH_AND_ROOM_STATE;
    uint16_t generationVersion = DUNGEON_GENERATION_VERSION;
    uint32_t generationSeed = 0;
    uint8_t roomCount = 0;
    uint8_t entranceRoom = 0;
    uint16_t snapshotEpoch = 0;
};

struct NetworkCharacterState
{
    PlayerID playerID = INVALID_PLAYER_ID;
    char displayName[NETWORK_PLAYER_NAME_SIZE] = {};
    uint8_t characterClass = 0;
    uint8_t level = 0;
    int16_t currentHP = 0;
    int16_t maxHP = 0;
    uint8_t abilityScores[6] = {};
    uint8_t speed = 0;
};

struct TravelInvitePayload
{
    uint32_t activityID = 0;
    MultiplayerActivityType activityType = MultiplayerActivityType::NONE;
    PlayerID hostPlayerID = HOST_PLAYER_ID;
};

struct TravelResponsePayload
{
    uint32_t activityID = 0;
    NetworkCharacterState character{};
};

struct ActivityPreparePayload
{
    uint32_t activityID = 0;
    MultiplayerActivityType activityType = MultiplayerActivityType::NONE;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint16_t snapshotEpoch = 0;
    uint8_t participantMask = 0;
    uint8_t graphChunkCount = 0;
    uint8_t detailChunkCount = 0;
    uint8_t worldChunkCount = 0;
};

struct ActivityReadyPayload
{
    uint32_t activityID = 0;
    uint16_t snapshotEpoch = 0;
};

struct ActivityMemberPayload
{
    uint32_t activityID = 0;
    PlayerID playerID = INVALID_PLAYER_ID;
};

struct PlayerSpawnPayload
{
    uint32_t activityID = 0;
    PlayerID playerID = INVALID_PLAYER_ID;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint8_t x = 0;
    uint8_t y = 0;
};

struct PlayerCharacterStatePayload
{
    uint32_t activityID = 0;
    NetworkCharacterState character{};
};

struct SnapshotChunkPayload
{
    uint32_t activityID = 0;
    SnapshotType snapshotType = SnapshotType::FOREST_STATE;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint16_t snapshotEpoch = 0;
    uint8_t chunkIndex = 0;
    uint8_t totalChunks = 0;
    uint8_t payloadLength = 0;
    uint8_t payload[SNAPSHOT_CHUNK_DATA_SIZE] = {};
};

struct ActivityAckPayload
{
    uint32_t activityID = 0;
    uint16_t snapshotEpoch = 0;
    NetworkPacketType packetType = NetworkPacketType::ACTIVITY_PREPARE;
    SnapshotType snapshotType = SnapshotType::FOREST_STATE;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint8_t itemIndex = 0;
    uint8_t chunkIndex = 0;
};

struct RoomTransitionPayload
{
    uint32_t activityID = 0;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint8_t entryDirection = 0;
    uint16_t snapshotEpoch = 0;
};

struct PlayerMoveRequestPayload
{
    uint32_t activityID = 0;
    uint8_t direction = 0;
    uint16_t clientMovementSequence = 0;
};

struct PlayerPositionPayload
{
    uint32_t activityID = 0;
    PlayerID playerID = INVALID_PLAYER_ID;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t facing = 0;
    uint16_t movementSequence = 0;
};

struct PlayerRoomChangePayload
{
    uint32_t activityID = 0;
    PlayerID playerID = INVALID_PLAYER_ID;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint8_t entryDirection = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint16_t movementSequence = 0;
};

struct DecodedNetworkPacket
{
    NetworkPacketHeader header{};
    const uint8_t* payload = nullptr;
};

bool isKnownPacketType(NetworkPacketType packetType);
bool isGameplayPacketType(NetworkPacketType packetType);
bool isValidAvailability(MultiplayerAvailability availability);
bool encodePacket(
    const NetworkPacketHeader& header,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t* destination,
    size_t destinationCapacity,
    size_t& encodedSize);
bool decodePacket(
    const uint8_t* data,
    size_t dataSize,
    DecodedNetworkPacket& packet);
// Parses the common envelope without accepting its protocol version. The
// session uses this only to return JOIN_REJECT/PROTOCOL_MISMATCH safely.
bool decodePacketEnvelope(
    const uint8_t* data,
    size_t dataSize,
    DecodedNetworkPacket& packet);

bool encodeDiscoveryBeacon(
    const DiscoveryBeaconPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodeDiscoveryBeacon(
    const uint8_t* data,
    size_t size,
    DiscoveryBeaconPayload& payload);
bool encodeJoinRequest(
    const JoinRequestPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodeJoinRequest(
    const uint8_t* data,
    size_t size,
    JoinRequestPayload& payload);
bool encodeJoinAccept(
    const JoinAcceptPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodeJoinAccept(
    const uint8_t* data,
    size_t size,
    JoinAcceptPayload& payload);
bool encodeJoinReject(
    const JoinRejectPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodeJoinReject(
    const uint8_t* data,
    size_t size,
    JoinRejectPayload& payload);
bool encodePlayerJoined(
    const PlayerJoinedPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodePlayerJoined(
    const uint8_t* data,
    size_t size,
    PlayerJoinedPayload& payload);
bool encodePlayerLeft(
    const PlayerLeftPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodePlayerLeft(
    const uint8_t* data,
    size_t size,
    PlayerLeftPayload& payload);
bool encodeTravelInvite(const TravelInvitePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeTravelInvite(const uint8_t* data, size_t size,
    TravelInvitePayload& payload);
bool encodeTravelResponse(const TravelResponsePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeTravelResponse(const uint8_t* data, size_t size,
    TravelResponsePayload& payload);
bool encodeActivityPrepare(const ActivityPreparePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeActivityPrepare(const uint8_t* data, size_t size,
    ActivityPreparePayload& payload);
bool encodeActivityReady(const ActivityReadyPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeActivityReady(const uint8_t* data, size_t size,
    ActivityReadyPayload& payload);
bool encodeActivityMember(const ActivityMemberPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeActivityMember(const uint8_t* data, size_t size,
    ActivityMemberPayload& payload);
bool encodeNetworkCharacterState(const NetworkCharacterState& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeNetworkCharacterState(const uint8_t* data, size_t size,
    NetworkCharacterState& payload);
bool encodePlayerCharacterState(const PlayerCharacterStatePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodePlayerCharacterState(const uint8_t* data, size_t size,
    PlayerCharacterStatePayload& payload);
bool encodePlayerSpawn(const PlayerSpawnPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodePlayerSpawn(const uint8_t* data, size_t size,
    PlayerSpawnPayload& payload);
bool encodeSnapshotChunk(const SnapshotChunkPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeSnapshotChunk(const uint8_t* data, size_t size,
    SnapshotChunkPayload& payload);
bool encodeActivityAck(const ActivityAckPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeActivityAck(const uint8_t* data, size_t size,
    ActivityAckPayload& payload);
bool encodeRoomTransition(const RoomTransitionPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size);
bool decodeRoomTransition(const uint8_t* data, size_t size,
    RoomTransitionPayload& payload);
bool encodeDungeonBegin(
    const DungeonBeginPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodeDungeonBegin(
    const uint8_t* data,
    size_t size,
    DungeonBeginPayload& payload);
bool encodePlayerMoveRequest(
    const PlayerMoveRequestPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodePlayerMoveRequest(
    const uint8_t* data,
    size_t size,
    PlayerMoveRequestPayload& payload);
bool encodePlayerPosition(
    const PlayerPositionPayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodePlayerPosition(
    const uint8_t* data,
    size_t size,
    PlayerPositionPayload& payload);
bool encodePlayerRoomChange(
    const PlayerRoomChangePayload& payload,
    uint8_t* destination,
    size_t capacity,
    size_t& size);
bool decodePlayerRoomChange(
    const uint8_t* data,
    size_t size,
    PlayerRoomChangePayload& payload);

#endif
