#include "multiplayer_session.h"

#include <Arduino.h>
#include <esp_system.h>

#include <stdio.h>
#include <string.h>

namespace
{
bool elapsed(uint32_t now, uint32_t then, uint32_t interval)
{
    return static_cast<uint32_t>(now - then) >= interval;
}

bool isNewerSequence(uint16_t incoming, uint16_t previous)
{
    return static_cast<int16_t>(incoming - previous) > 0;
}

uint32_t createDeviceID(const TransportAddress& address)
{
    uint32_t hash = 2166136261u;
    for (uint8_t value : address.bytes)
    {
        hash ^= value;
        hash *= 16777619u;
    }
    return hash == 0 ? 1 : hash;
}

uint32_t createSessionID(const TransportAddress& address, uint32_t now)
{
    uint32_t value = createDeviceID(address) ^ esp_random() ^ now;
    return value == 0 ? 1 : value;
}

const char* joinRejectText(JoinRejectReason reason)
{
    switch (reason)
    {
        case JoinRejectReason::PROTOCOL_MISMATCH:
            return "Protocol versions differ.";
        case JoinRejectReason::SESSION_FULL:
            return "That party is full.";
        case JoinRejectReason::SESSION_UNAVAILABLE:
            return "That party is unavailable.";
        case JoinRejectReason::INVALID_REQUEST:
            return "Join request was rejected.";
        default:
            return "Unable to join party.";
    }
}
}

MultiplayerSession multiplayerSession;

const char* multiplayerAvailabilityName(MultiplayerAvailability availability)
{
    switch (availability)
    {
        case MultiplayerAvailability::AVAILABLE: return "Available";
        case MultiplayerAvailability::IN_PARTY: return "In Party";
        case MultiplayerAvailability::IN_DUNGEON: return "In Dungeon";
        case MultiplayerAvailability::IN_COMBAT: return "In Combat";
        case MultiplayerAvailability::BUSY: return "Busy";
    }
    return "Unknown";
}

const char* multiplayerSessionStateName(MultiplayerSessionState state)
{
    switch (state)
    {
        case MultiplayerSessionState::INACTIVE: return "Inactive";
        case MultiplayerSessionState::HOSTING: return "Host";
        case MultiplayerSessionState::JOINING: return "Joining";
        case MultiplayerSessionState::CLIENT: return "Client";
    }
    return "Unknown";
}

bool MultiplayerSession::begin()
{
    transportReady = multiplayerTransport.begin();
    if (!transportReady)
    {
        snprintf(statusText, sizeof(statusText), "ESP-NOW unavailable.");
        setNotice("ESP-NOW initialization failed.");
        return false;
    }

    localProfile.deviceID = createDeviceID(
        multiplayerTransport.getLocalAddress());
    snprintf(statusText, sizeof(statusText), "Ready for nearby players.");
    return true;
}

void MultiplayerSession::setLocalProfile(
    const char* displayName,
    uint8_t level,
    uint8_t characterClass)
{
    localProfile.level = level;
    localProfile.characterClass = characterClass;
    const char* resolvedName = displayName != nullptr && displayName[0] != '\0'
        ? displayName : "Adventurer";
    strncpy(localProfile.displayName, resolvedName, NETWORK_PLAYER_NAME_SIZE - 1);
    localProfile.displayName[NETWORK_PLAYER_NAME_SIZE - 1] = '\0';
    if (isValidPlayerID(localPlayerID) && members[localPlayerID].member.occupied)
        members[localPlayerID].member.profile = localProfile;
}

void MultiplayerSession::setGameplayPacketHandler(
    MultiplayerGameplayPacketHandler handler)
{
    gameplayPacketHandler = handler;
}

void MultiplayerSession::update(
    uint32_t now,
    MultiplayerAvailability availability)
{
    if (!transportReady) return;
    localAvailability = availability;
    processReceivedFrames(now);
    expireNearby(now);
    expireConnections(now);

    if (elapsed(now, lastDiscoveryAt, DISCOVERY_BEACON_INTERVAL_MS))
        sendDiscovery(now);
    if (isActive() && elapsed(now, lastHeartbeatAt, HEARTBEAT_INTERVAL_MS))
        sendHeartbeat(now);

    if (state == MultiplayerSessionState::JOINING &&
        elapsed(now, joinStartedAt, JOIN_TIMEOUT_MS))
    {
        clearSessionState();
        setNotice("Join request timed out.");
    }
    else if (state == MultiplayerSessionState::JOINING &&
             elapsed(now, lastJoinRequestAt,
                     JOIN_REQUEST_RETRY_INTERVAL_MS))
    {
        sendJoinRequest(now);
    }
}

bool MultiplayerSession::hostSession(uint32_t now)
{
    if (!transportReady || isActive() || isJoining()) return false;
    clearSessionState();
    state = MultiplayerSessionState::HOSTING;
    sessionID = createSessionID(multiplayerTransport.getLocalAddress(), now);
    localPlayerID = HOST_PLAYER_ID;
    members[HOST_PLAYER_ID].member.occupied = true;
    members[HOST_PLAYER_ID].member.connected = true;
    members[HOST_PLAYER_ID].member.playerID = HOST_PLAYER_ID;
    members[HOST_PLAYER_ID].member.profile = localProfile;
    members[HOST_PLAYER_ID].member.lastPacketAt = now;
    members[HOST_PLAYER_ID].address = multiplayerTransport.getLocalAddress();
    snprintf(statusText, sizeof(statusText), "Hosting party. 1/%u players.",
             MAX_MULTIPLAYER_PLAYERS);
    setNotice("Multiplayer party hosted.");
    lastDiscoveryAt = 0;
    return true;
}

bool MultiplayerSession::joinNearby(uint8_t nearbyIndex, uint32_t now)
{
    if (!canJoinNearby(nearbyIndex) || isActive() || isJoining()) return false;

    const NearbyAdventurer* selectedAdventurer = getNearby(nearbyIndex);
    if (selectedAdventurer == nullptr) return false;
    const NearbySlot* selectedSlot = nullptr;
    for (const NearbySlot& slot : nearby)
        if (slot.adventurer.active &&
            slot.adventurer.profile.deviceID ==
                selectedAdventurer->profile.deviceID)
        {
            selectedSlot = &slot;
            break;
        }
    if (selectedSlot == nullptr) return false;
    const NearbySlot selected = *selectedSlot;
    clearSessionState();
    state = MultiplayerSessionState::JOINING;
    sessionID = selected.adventurer.sessionID;
    pendingHostAddress = selected.address;
    pendingHostProfile = selected.adventurer.profile;
    joinStartedAt = now;

    if (!sendJoinRequest(now))
    {
        clearSessionState();
        setNotice("Could not send join request.");
        return false;
    }

    snprintf(statusText, sizeof(statusText), "Joining %s...",
             pendingHostProfile.displayName);
    return true;
}

void MultiplayerSession::leaveSession()
{
    if (!isActive() && !isJoining()) return;

    if (isHost())
    {
        broadcastPlayerLeft(HOST_PLAYER_ID,
                            PlayerLeaveReason::HOST_ENDED_SESSION);
    }
    else if (isClient())
    {
        PlayerLeftPayload payload{};
        payload.playerID = localPlayerID;
        payload.reason = PlayerLeaveReason::LEFT_PARTY;
        uint8_t bytes[8] = {};
        size_t size = 0;
        if (encodePlayerLeft(payload, bytes, sizeof(bytes), size))
            sendPacketTo(members[HOST_PLAYER_ID].address,
                         NetworkPacketType::PLAYER_LEFT, bytes, size,
                         NETWORK_FLAG_CONTROL_EVENT);
    }

    clearSessionState();
    setNotice("Left multiplayer party.");
}

bool MultiplayerSession::sendGameplayPacketToHost(
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags)
{
    return isClient() && isGameplayPacketType(packetType) &&
           members[HOST_PLAYER_ID].member.occupied &&
           sendPacketTo(members[HOST_PLAYER_ID].address,
                        packetType, payload, payloadSize, flags);
}

bool MultiplayerSession::broadcastAuthoritativeGameplayPacket(
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags)
{
    if (!isHost() || !isGameplayPacketType(packetType)) return false;
    bool sentToAll = true;
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (members[playerID].member.occupied &&
            !sendPacketTo(members[playerID].address,
                          packetType, payload, payloadSize, flags))
            sentToAll = false;
    return sentToAll;
}

bool MultiplayerSession::isTransportReady() const { return transportReady; }
bool MultiplayerSession::isActive() const
{
    return state == MultiplayerSessionState::HOSTING ||
           state == MultiplayerSessionState::CLIENT;
}
bool MultiplayerSession::isHost() const
{
    return state == MultiplayerSessionState::HOSTING;
}
bool MultiplayerSession::isClient() const
{
    return state == MultiplayerSessionState::CLIENT;
}
bool MultiplayerSession::isJoining() const
{
    return state == MultiplayerSessionState::JOINING;
}
MultiplayerSessionState MultiplayerSession::getState() const { return state; }
PlayerID MultiplayerSession::getLocalPlayerID() const
{
    return isValidPlayerID(localPlayerID) ? localPlayerID : SINGLE_PLAYER_ID;
}
uint32_t MultiplayerSession::getSessionID() const { return sessionID; }

uint8_t MultiplayerSession::getConnectedPlayerCount() const
{
    uint8_t count = 0;
    for (const MemberSlot& slot : members)
        if (slot.member.occupied && slot.member.connected) ++count;
    return count;
}

const SessionMember* MultiplayerSession::getMember(PlayerID playerID) const
{
    return isValidPlayerID(playerID) && members[playerID].member.occupied
        ? &members[playerID].member : nullptr;
}

uint8_t MultiplayerSession::getNearbyCount() const
{
    uint8_t count = 0;
    for (const NearbySlot& slot : nearby)
        if (slot.adventurer.active) ++count;
    return count;
}

const NearbyAdventurer* MultiplayerSession::getNearby(uint8_t index) const
{
    uint8_t activeIndex = 0;
    for (const NearbySlot& slot : nearby)
    {
        if (!slot.adventurer.active) continue;
        if (activeIndex++ == index) return &slot.adventurer;
    }
    return nullptr;
}

bool MultiplayerSession::canJoinNearby(uint8_t index) const
{
    if (!transportReady || isActive() || isJoining()) return false;
    const NearbyAdventurer* adventurer = getNearby(index);
    return adventurer != nullptr && adventurer->acceptingPlayers &&
           adventurer->sessionID != 0;
}

const char* MultiplayerSession::getStatusText() const { return statusText; }

bool MultiplayerSession::consumeNotice(char* destination, uint8_t capacity)
{
    if (destination == nullptr || capacity == 0 || pendingNotice[0] == '\0')
        return false;
    strncpy(destination, pendingNotice, capacity - 1);
    destination[capacity - 1] = '\0';
    pendingNotice[0] = '\0';
    return true;
}

void MultiplayerSession::processReceivedFrames(uint32_t now)
{
    ReceivedNetworkFrame frame{};
    while (multiplayerTransport.receive(frame))
    {
        DecodedNetworkPacket packet{};
        if (decodePacketEnvelope(frame.data, frame.size, packet))
            processPacket(frame, packet, now);
    }
}

void MultiplayerSession::processPacket(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    if (packet.header.protocolVersion != MULTIPLAYER_PROTOCOL_VERSION)
    {
        if (isHost() &&
            packet.header.packetType == NetworkPacketType::JOIN_REQUEST &&
            packet.header.sessionID == sessionID)
        {
            JoinRejectPayload rejection{};
            rejection.reason = JoinRejectReason::PROTOCOL_MISMATCH;
            uint8_t bytes[4] = {};
            size_t size = 0;
            if (encodeJoinReject(rejection, bytes, sizeof(bytes), size))
                sendPacketTo(frame.sender, NetworkPacketType::JOIN_REJECT,
                             bytes, size, NETWORK_FLAG_CONTROL_EVENT);
        }
        return;
    }

    if (packet.header.packetType == NetworkPacketType::DISCOVERY_BEACON)
    {
        processDiscovery(frame, packet, now);
        return;
    }
    if (packet.header.packetType == NetworkPacketType::JOIN_REQUEST)
    {
        processJoinRequest(frame, packet, now);
        return;
    }
    if (packet.header.packetType == NetworkPacketType::JOIN_ACCEPT)
    {
        processJoinAccept(frame, packet, now);
        return;
    }
    if (packet.header.packetType == NetworkPacketType::JOIN_REJECT)
    {
        processJoinReject(frame, packet);
        return;
    }
    if (!acceptMemberPacket(frame, packet, now)) return;

    switch (packet.header.packetType)
    {
        case NetworkPacketType::PLAYER_JOINED:
            processPlayerJoined(packet, now);
            break;
        case NetworkPacketType::PLAYER_LEFT:
            processPlayerLeft(packet);
            break;
        case NetworkPacketType::HEARTBEAT:
            break;
        case NetworkPacketType::PING:
            sendPacketTo(frame.sender, NetworkPacketType::PONG, nullptr, 0);
            break;
        case NetworkPacketType::PONG:
            break;
        default:
            if (gameplayPacketHandler != nullptr &&
                isGameplayPacketType(packet.header.packetType))
            {
                gameplayPacketHandler(
                    packet.header.senderPlayerID,
                    packet.header.packetType,
                    packet.payload,
                    packet.header.payloadSize);
            }
            break;
    }
}

void MultiplayerSession::processDiscovery(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    DiscoveryBeaconPayload payload{};
    if (!decodeDiscoveryBeacon(
            packet.payload, packet.header.payloadSize, payload) ||
        payload.profile.deviceID == 0 ||
        payload.profile.deviceID == localProfile.deviceID)
        return;

    NearbySlot* destination = nullptr;
    NearbySlot* oldest = nullptr;
    for (NearbySlot& slot : nearby)
    {
        if (slot.adventurer.active &&
            slot.adventurer.profile.deviceID == payload.profile.deviceID)
        {
            destination = &slot;
            break;
        }
        if (!slot.adventurer.active && destination == nullptr)
            destination = &slot;
        if (oldest == nullptr ||
            slot.adventurer.lastSeenAt < oldest->adventurer.lastSeenAt)
            oldest = &slot;
    }
    if (destination == nullptr) destination = oldest;
    if (destination == nullptr) return;

    destination->adventurer.active = true;
    destination->adventurer.profile = payload.profile;
    destination->adventurer.availability = payload.availability;
    destination->adventurer.acceptingPlayers = payload.acceptingPlayers;
    destination->adventurer.sessionID = packet.header.sessionID;
    destination->adventurer.lastSeenAt = now;
    destination->address = frame.sender;
}

void MultiplayerSession::processJoinRequest(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    if (!isHost() || packet.header.sessionID != sessionID ||
        packet.header.senderPlayerID != INVALID_PLAYER_ID) return;

    JoinRequestPayload request{};
    if (!decodeJoinRequest(packet.payload, packet.header.payloadSize, request) ||
        request.profile.deviceID == 0)
        return;

    if (!isAcceptingPlayers())
    {
        JoinRejectPayload rejection{};
        rejection.reason = JoinRejectReason::SESSION_UNAVAILABLE;
        uint8_t bytes[4] = {};
        size_t size = 0;
        if (encodeJoinReject(rejection, bytes, sizeof(bytes), size))
            sendPacketTo(frame.sender, NetworkPacketType::JOIN_REJECT,
                         bytes, size, NETWORK_FLAG_CONTROL_EVENT);
        return;
    }

    PlayerID assignedID = findMemberByDevice(
        request.profile.deviceID, frame.sender);
    if (!isValidPlayerID(assignedID)) assignedID = findOpenPlayerID();
    if (!isValidPlayerID(assignedID))
    {
        JoinRejectPayload rejection{};
        rejection.reason = JoinRejectReason::SESSION_FULL;
        uint8_t bytes[4] = {};
        size_t size = 0;
        if (encodeJoinReject(rejection, bytes, sizeof(bytes), size))
            sendPacketTo(frame.sender, NetworkPacketType::JOIN_REJECT,
                         bytes, size, NETWORK_FLAG_CONTROL_EVENT);
        return;
    }

    MemberSlot& joining = members[assignedID];
    joining.member.occupied = true;
    joining.member.connected = true;
    joining.member.playerID = assignedID;
    joining.member.profile = request.profile;
    joining.member.lastPacketAt = now;
    joining.address = frame.sender;
    joining.hasSequence = false;

    JoinAcceptPayload acceptance{};
    acceptance.assignedPlayerID = assignedID;
    uint8_t acceptBytes[8] = {};
    size_t acceptSize = 0;
    if (!encodeJoinAccept(
            acceptance, acceptBytes, sizeof(acceptBytes), acceptSize) ||
        !sendPacketTo(frame.sender, NetworkPacketType::JOIN_ACCEPT,
                      acceptBytes, acceptSize, NETWORK_FLAG_CONTROL_EVENT))
        return;

    for (PlayerID playerID = 0; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!members[playerID].member.occupied || playerID == assignedID)
            continue;
        sendPlayerJoinedTo(frame.sender, playerID);
    }
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
    {
        if (!members[playerID].member.occupied || playerID == assignedID)
            continue;
        sendPlayerJoinedTo(members[playerID].address, assignedID);
    }

    snprintf(statusText, sizeof(statusText), "Hosting party. %u/%u players.",
             getConnectedPlayerCount(), MAX_MULTIPLAYER_PLAYERS);
    setNotice("A player joined the party.");
}

void MultiplayerSession::processJoinAccept(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    if (!isJoining() || packet.header.sessionID != sessionID ||
        packet.header.senderPlayerID != HOST_PLAYER_ID ||
        frame.sender != pendingHostAddress) return;

    JoinAcceptPayload acceptance{};
    if (!decodeJoinAccept(
            packet.payload, packet.header.payloadSize, acceptance) ||
        acceptance.assignedPlayerID == HOST_PLAYER_ID) return;

    state = MultiplayerSessionState::CLIENT;
    localPlayerID = acceptance.assignedPlayerID;
    members[HOST_PLAYER_ID].member.occupied = true;
    members[HOST_PLAYER_ID].member.connected = true;
    members[HOST_PLAYER_ID].member.playerID = HOST_PLAYER_ID;
    members[HOST_PLAYER_ID].member.profile = pendingHostProfile;
    members[HOST_PLAYER_ID].member.lastPacketAt = now;
    members[HOST_PLAYER_ID].address = pendingHostAddress;
    members[localPlayerID].member.occupied = true;
    members[localPlayerID].member.connected = true;
    members[localPlayerID].member.playerID = localPlayerID;
    members[localPlayerID].member.profile = localProfile;
    members[localPlayerID].member.lastPacketAt = now;
    members[localPlayerID].address = multiplayerTransport.getLocalAddress();
    snprintf(statusText, sizeof(statusText), "Joined %s's party.",
             pendingHostProfile.displayName);
    setNotice("Joined multiplayer party.");
}

void MultiplayerSession::processJoinReject(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet)
{
    if (!isJoining() || packet.header.sessionID != sessionID ||
        packet.header.senderPlayerID != HOST_PLAYER_ID ||
        frame.sender != pendingHostAddress) return;
    JoinRejectPayload rejection{};
    if (!decodeJoinReject(
            packet.payload, packet.header.payloadSize, rejection)) return;
    const char* notice = joinRejectText(rejection.reason);
    clearSessionState();
    setNotice(notice);
}

void MultiplayerSession::processPlayerJoined(
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    if (!isClient() || packet.header.senderPlayerID != HOST_PLAYER_ID) return;
    PlayerJoinedPayload joined{};
    if (!decodePlayerJoined(
            packet.payload, packet.header.payloadSize, joined) ||
        joined.playerID == localPlayerID) return;
    MemberSlot& slot = members[joined.playerID];
    slot.member.occupied = true;
    slot.member.connected = true;
    slot.member.playerID = joined.playerID;
    slot.member.profile = joined.profile;
    slot.member.lastPacketAt = now;
    setNotice("A player joined the party.");
}

void MultiplayerSession::processPlayerLeft(
    const DecodedNetworkPacket& packet)
{
    PlayerLeftPayload left{};
    if (!decodePlayerLeft(packet.payload, packet.header.payloadSize, left)) return;

    if (isClient() && packet.header.senderPlayerID == HOST_PLAYER_ID &&
        left.playerID == HOST_PLAYER_ID)
    {
        clearSessionState();
        setNotice("Host ended the multiplayer session.");
        return;
    }

    if (isHost() && left.playerID == packet.header.senderPlayerID &&
        left.playerID != HOST_PLAYER_ID)
    {
        const PlayerID leavingID = left.playerID;
        multiplayerTransport.removePeer(members[leavingID].address);
        members[leavingID] = MemberSlot{};
        broadcastPlayerLeft(leavingID, left.reason);
        snprintf(statusText, sizeof(statusText), "Hosting party. %u/%u players.",
                 getConnectedPlayerCount(), MAX_MULTIPLAYER_PLAYERS);
        setNotice("A player left the party.");
    }
    else if (isClient() && packet.header.senderPlayerID == HOST_PLAYER_ID &&
             left.playerID != localPlayerID)
    {
        members[left.playerID] = MemberSlot{};
        setNotice("A player left the party.");
    }
}

void MultiplayerSession::sendDiscovery(uint32_t now)
{
    DiscoveryBeaconPayload beacon{};
    beacon.profile = localProfile;
    beacon.availability = localAvailability;
    beacon.acceptingPlayers = isAcceptingPlayers();
    uint8_t payload[32] = {};
    size_t payloadSize = 0;
    if (encodeDiscoveryBeacon(beacon, payload, sizeof(payload), payloadSize))
        sendPacketBroadcast(NetworkPacketType::DISCOVERY_BEACON,
                            payload, payloadSize);
    lastDiscoveryAt = now;
}

void MultiplayerSession::sendHeartbeat(uint32_t now)
{
    if (isHost())
    {
        for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
            if (members[playerID].member.occupied)
                sendPacketTo(members[playerID].address,
                             NetworkPacketType::HEARTBEAT, nullptr, 0);
    }
    else if (isClient() && members[HOST_PLAYER_ID].member.occupied)
    {
        sendPacketTo(members[HOST_PLAYER_ID].address,
                     NetworkPacketType::HEARTBEAT, nullptr, 0);
    }
    lastHeartbeatAt = now;
}

bool MultiplayerSession::sendJoinRequest(uint32_t now)
{
    if (!isJoining()) return false;
    uint8_t payloadBytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t payloadSize = 0;
    JoinRequestPayload payload{};
    payload.profile = localProfile;
    lastJoinRequestAt = now;
    return encodeJoinRequest(
               payload, payloadBytes, sizeof(payloadBytes), payloadSize) &&
           sendPacketTo(
               pendingHostAddress,
               NetworkPacketType::JOIN_REQUEST,
               payloadBytes,
               payloadSize,
               NETWORK_FLAG_CONTROL_EVENT);
}

bool MultiplayerSession::sendPacketTo(
    const TransportAddress& destination,
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags)
{
    NetworkPacketHeader header{};
    header.packetType = packetType;
    header.senderPlayerID = isActive() ? localPlayerID : INVALID_PLAYER_ID;
    header.flags = flags;
    header.sequence = nextSequence++;
    header.sessionID = sessionID;
    uint8_t packet[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t packetSize = 0;
    return encodePacket(header, payload, payloadSize,
                        packet, sizeof(packet), packetSize) &&
           multiplayerTransport.sendTo(destination, packet, packetSize);
}

bool MultiplayerSession::sendPacketBroadcast(
    NetworkPacketType packetType,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t flags)
{
    NetworkPacketHeader header{};
    header.packetType = packetType;
    header.senderPlayerID = isActive() ? localPlayerID : INVALID_PLAYER_ID;
    header.flags = flags;
    header.sequence = nextSequence++;
    header.sessionID = sessionID;
    uint8_t packet[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t packetSize = 0;
    return encodePacket(header, payload, payloadSize,
                        packet, sizeof(packet), packetSize) &&
           multiplayerTransport.sendBroadcast(packet, packetSize);
}

void MultiplayerSession::sendPlayerJoinedTo(
    const TransportAddress& destination,
    PlayerID joinedPlayerID)
{
    if (!isValidPlayerID(joinedPlayerID) ||
        !members[joinedPlayerID].member.occupied) return;
    PlayerJoinedPayload joined{};
    joined.playerID = joinedPlayerID;
    joined.profile = members[joinedPlayerID].member.profile;
    uint8_t bytes[32] = {};
    size_t size = 0;
    if (encodePlayerJoined(joined, bytes, sizeof(bytes), size))
        sendPacketTo(destination, NetworkPacketType::PLAYER_JOINED,
                     bytes, size, NETWORK_FLAG_CONTROL_EVENT);
}

void MultiplayerSession::broadcastPlayerLeft(
    PlayerID playerID,
    PlayerLeaveReason reason)
{
    PlayerLeftPayload left{};
    left.playerID = playerID;
    left.reason = reason;
    uint8_t bytes[8] = {};
    size_t size = 0;
    if (!encodePlayerLeft(left, bytes, sizeof(bytes), size)) return;
    for (PlayerID destination = 1;
         destination < MAX_MULTIPLAYER_PLAYERS; ++destination)
    {
        if (members[destination].member.occupied && destination != playerID)
            sendPacketTo(members[destination].address,
                         NetworkPacketType::PLAYER_LEFT, bytes, size,
                         NETWORK_FLAG_CONTROL_EVENT);
    }
}

void MultiplayerSession::expireNearby(uint32_t now)
{
    for (NearbySlot& slot : nearby)
        if (slot.adventurer.active &&
            elapsed(now, slot.adventurer.lastSeenAt, DISCOVERY_EXPIRY_MS))
            slot = NearbySlot{};
}

void MultiplayerSession::expireConnections(uint32_t now)
{
    if (isHost())
    {
        for (PlayerID playerID = 1;
             playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        {
            if (!members[playerID].member.occupied ||
                !elapsed(now, members[playerID].member.lastPacketAt,
                         CONNECTION_TIMEOUT_MS)) continue;
            multiplayerTransport.removePeer(members[playerID].address);
            members[playerID] = MemberSlot{};
            broadcastPlayerLeft(playerID, PlayerLeaveReason::CONNECTION_TIMEOUT);
            setNotice("A player connection was lost.");
        }
        snprintf(statusText, sizeof(statusText), "Hosting party. %u/%u players.",
                 getConnectedPlayerCount(), MAX_MULTIPLAYER_PLAYERS);
    }
    else if (isClient() && members[HOST_PLAYER_ID].member.occupied &&
             elapsed(now, members[HOST_PLAYER_ID].member.lastPacketAt,
                     CONNECTION_TIMEOUT_MS))
    {
        clearSessionState();
        setNotice("Host connection lost.");
    }
}

void MultiplayerSession::clearSessionState()
{
    for (MemberSlot& member : members)
    {
        if (member.member.occupied &&
            member.address != multiplayerTransport.getLocalAddress())
            multiplayerTransport.removePeer(member.address);
        member = MemberSlot{};
    }
    state = MultiplayerSessionState::INACTIVE;
    localPlayerID = INVALID_PLAYER_ID;
    sessionID = 0;
    joinStartedAt = 0;
    lastJoinRequestAt = 0;
    pendingHostAddress = TransportAddress{};
    pendingHostProfile = NetworkPlayerProfile{};
    snprintf(statusText, sizeof(statusText), "Ready for nearby players.");
}

bool MultiplayerSession::acceptMemberPacket(
    const ReceivedNetworkFrame& frame,
    const DecodedNetworkPacket& packet,
    uint32_t now)
{
    if (!isActive() || packet.header.sessionID != sessionID ||
        !isValidPlayerID(packet.header.senderPlayerID) ||
        packet.header.senderPlayerID == localPlayerID) return false;
    MemberSlot& sender = members[packet.header.senderPlayerID];
    if (!sender.member.occupied || sender.address != frame.sender) return false;
    if (sender.hasSequence &&
        !isNewerSequence(packet.header.sequence, sender.lastSequence))
        return false;
    sender.lastSequence = packet.header.sequence;
    sender.hasSequence = true;
    sender.member.lastPacketAt = now;
    sender.member.connected = true;
    return true;
}

PlayerID MultiplayerSession::findMemberByDevice(
    uint32_t deviceID,
    const TransportAddress& address) const
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (members[playerID].member.occupied &&
            members[playerID].member.profile.deviceID == deviceID &&
            members[playerID].address == address) return playerID;
    return INVALID_PLAYER_ID;
}

PlayerID MultiplayerSession::findOpenPlayerID() const
{
    for (PlayerID playerID = 1; playerID < MAX_MULTIPLAYER_PLAYERS; ++playerID)
        if (!members[playerID].member.occupied) return playerID;
    return INVALID_PLAYER_ID;
}

bool MultiplayerSession::isAcceptingPlayers() const
{
    return isHost() &&
           getConnectedPlayerCount() < MAX_MULTIPLAYER_PLAYERS &&
           (localAvailability == MultiplayerAvailability::AVAILABLE ||
            localAvailability == MultiplayerAvailability::IN_PARTY);
}

void MultiplayerSession::setNotice(const char* text)
{
    if (text == nullptr) return;
    strncpy(pendingNotice, text, sizeof(pendingNotice) - 1);
    pendingNotice[sizeof(pendingNotice) - 1] = '\0';
}
