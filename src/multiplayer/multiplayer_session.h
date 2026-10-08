#ifndef PATHFINDERMINIEXTREME_025_MULTIPLAYER_SESSION_H
#define PATHFINDERMINIEXTREME_025_MULTIPLAYER_SESSION_H

#include <stdint.h>

#include "espnow_transport.h"
#include "network_protocol.h"

constexpr uint8_t MAX_NEARBY_ADVENTURERS = 8;
constexpr uint32_t DISCOVERY_BEACON_INTERVAL_MS = 1500;
constexpr uint32_t DISCOVERY_EXPIRY_MS = 6000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 2000;
constexpr uint32_t CONNECTION_TIMEOUT_MS = 7000;
constexpr uint32_t JOIN_TIMEOUT_MS = 5000;
constexpr uint32_t JOIN_REQUEST_RETRY_INTERVAL_MS = 750;

enum class MultiplayerSessionState : uint8_t
{
    INACTIVE,
    HOSTING,
    JOINING,
    CLIENT
};

struct NearbyAdventurer
{
    bool active = false;
    NetworkPlayerProfile profile{};
    MultiplayerAvailability availability = MultiplayerAvailability::BUSY;
    bool acceptingPlayers = false;
    uint32_t sessionID = 0;
    uint32_t lastSeenAt = 0;
};

struct SessionMember
{
    bool occupied = false;
    bool connected = false;
    PlayerID playerID = INVALID_PLAYER_ID;
    NetworkPlayerProfile profile{};
    uint32_t lastPacketAt = 0;
};

// Called from update() after version, SessionID, sender address, and sequence
// validation. The payload view is valid only for the duration of the call.
using MultiplayerGameplayPacketHandler = void (*)(
    PlayerID senderPlayerID,
    NetworkPacketType packetType,
    const uint8_t* payload,
    uint16_t payloadSize);

class MultiplayerSession
{
public:
    bool begin();
    void update(uint32_t now, MultiplayerAvailability availability);
    void setLocalProfile(
        const char* displayName,
        uint8_t level,
        uint8_t characterClass);
    void setGameplayPacketHandler(MultiplayerGameplayPacketHandler handler);

    bool hostSession(uint32_t now);
    bool joinNearby(uint8_t nearbyIndex, uint32_t now);
    void leaveSession();
    bool sendGameplayPacketToHost(
        NetworkPacketType packetType,
        const uint8_t* payload,
        size_t payloadSize,
        uint8_t flags = NETWORK_FLAG_NONE);
    bool sendGameplayPacketToPlayer(
        PlayerID playerID,
        NetworkPacketType packetType,
        const uint8_t* payload,
        size_t payloadSize,
        uint8_t flags = NETWORK_FLAG_NONE);
    bool broadcastAuthoritativeGameplayPacket(
        NetworkPacketType packetType,
        const uint8_t* payload,
        size_t payloadSize,
        uint8_t flags = NETWORK_FLAG_NONE);

    bool isTransportReady() const;
    bool isActive() const;
    bool isHost() const;
    bool isClient() const;
    bool isJoining() const;
    MultiplayerSessionState getState() const;
    PlayerID getLocalPlayerID() const;
    uint32_t getSessionID() const;
    uint8_t getConnectedPlayerCount() const;
    const SessionMember* getMember(PlayerID playerID) const;

    uint8_t getNearbyCount() const;
    const NearbyAdventurer* getNearby(uint8_t index) const;
    bool canJoinNearby(uint8_t index) const;

    const char* getStatusText() const;
    bool consumeNotice(char* destination, uint8_t capacity);

private:
    struct MemberSlot
    {
        SessionMember member{};
        TransportAddress address{};
        uint16_t lastSequence = 0;
        bool hasSequence = false;
    };

    struct NearbySlot
    {
        NearbyAdventurer adventurer{};
        TransportAddress address{};
    };

    void processReceivedFrames(uint32_t now);
    void processPacket(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet,
        uint32_t now);
    void processDiscovery(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet,
        uint32_t now);
    void processJoinRequest(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet,
        uint32_t now);
    void processJoinAccept(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet,
        uint32_t now);
    void processJoinReject(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet);
    void processPlayerJoined(
        const DecodedNetworkPacket& packet,
        uint32_t now);
    void processPlayerLeft(const DecodedNetworkPacket& packet);

    void sendDiscovery(uint32_t now);
    void sendHeartbeat(uint32_t now);
    bool sendJoinRequest(uint32_t now);
    bool sendPacketTo(
        const TransportAddress& destination,
        NetworkPacketType packetType,
        const uint8_t* payload,
        size_t payloadSize,
        uint8_t flags = NETWORK_FLAG_NONE);
    bool sendPacketBroadcast(
        NetworkPacketType packetType,
        const uint8_t* payload,
        size_t payloadSize,
        uint8_t flags = NETWORK_FLAG_NONE);
    void sendPlayerJoinedTo(
        const TransportAddress& destination,
        PlayerID joinedPlayerID);
    void broadcastPlayerLeft(
        PlayerID playerID,
        PlayerLeaveReason reason);
    void expireNearby(uint32_t now);
    void expireConnections(uint32_t now);
    void clearSessionState();
    bool acceptMemberPacket(
        const ReceivedNetworkFrame& frame,
        const DecodedNetworkPacket& packet,
        uint32_t now);
    PlayerID findMemberByDevice(
        uint32_t deviceID,
        const TransportAddress& address) const;
    PlayerID findOpenPlayerID() const;
    bool isAcceptingPlayers() const;
    void setNotice(const char* text);

    bool transportReady = false;
    MultiplayerSessionState state = MultiplayerSessionState::INACTIVE;
    NetworkPlayerProfile localProfile{};
    MultiplayerAvailability localAvailability =
        MultiplayerAvailability::AVAILABLE;
    PlayerID localPlayerID = INVALID_PLAYER_ID;
    uint32_t sessionID = 0;
    uint16_t nextSequence = 1;
    uint32_t lastDiscoveryAt = 0;
    uint32_t lastHeartbeatAt = 0;
    uint32_t joinStartedAt = 0;
    uint32_t lastJoinRequestAt = 0;
    TransportAddress pendingHostAddress{};
    NetworkPlayerProfile pendingHostProfile{};
    MemberSlot members[MAX_MULTIPLAYER_PLAYERS] = {};
    NearbySlot nearby[MAX_NEARBY_ADVENTURERS] = {};
    char statusText[64] = "Multiplayer inactive.";
    char pendingNotice[64] = {};
    MultiplayerGameplayPacketHandler gameplayPacketHandler = nullptr;
};

extern MultiplayerSession multiplayerSession;

const char* multiplayerAvailabilityName(MultiplayerAvailability availability);
const char* multiplayerSessionStateName(MultiplayerSessionState state);

#endif
