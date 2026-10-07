#include "network_protocol.h"

#include <string.h>

namespace
{
class PacketWriter
{
public:
    PacketWriter(uint8_t* data, size_t capacity)
        : data(data), capacity(capacity) {}

    bool writeU8(uint8_t value)
    {
        if (position >= capacity) return false;
        data[position++] = value;
        return true;
    }

    bool writeU16(uint16_t value)
    {
        return writeU8(static_cast<uint8_t>(value)) &&
               writeU8(static_cast<uint8_t>(value >> 8));
    }

    bool writeU32(uint32_t value)
    {
        return writeU16(static_cast<uint16_t>(value)) &&
               writeU16(static_cast<uint16_t>(value >> 16));
    }

    bool writeBytes(const void* source, size_t byteCount)
    {
        if (source == nullptr || position + byteCount > capacity) return false;
        memcpy(data + position, source, byteCount);
        position += byteCount;
        return true;
    }

    size_t size() const { return position; }

private:
    uint8_t* data;
    size_t capacity;
    size_t position = 0;
};

class PacketReader
{
public:
    PacketReader(const uint8_t* data, size_t size) : data(data), size(size) {}

    bool readU8(uint8_t& value)
    {
        if (position >= size) return false;
        value = data[position++];
        return true;
    }

    bool readU16(uint16_t& value)
    {
        uint8_t low = 0;
        uint8_t high = 0;
        if (!readU8(low) || !readU8(high)) return false;
        value = static_cast<uint16_t>(low) |
                (static_cast<uint16_t>(high) << 8);
        return true;
    }

    bool readU32(uint32_t& value)
    {
        uint16_t low = 0;
        uint16_t high = 0;
        if (!readU16(low) || !readU16(high)) return false;
        value = static_cast<uint32_t>(low) |
                (static_cast<uint32_t>(high) << 16);
        return true;
    }

    bool readBytes(void* destination, size_t byteCount)
    {
        if (destination == nullptr || position + byteCount > size) return false;
        memcpy(destination, data + position, byteCount);
        position += byteCount;
        return true;
    }

    bool finished() const { return position == size; }

private:
    const uint8_t* data;
    size_t size;
    size_t position = 0;
};

bool writeProfile(PacketWriter& writer, const NetworkPlayerProfile& profile)
{
    return writer.writeU32(profile.deviceID) &&
           writer.writeU8(profile.level) &&
           writer.writeU8(profile.characterClass) &&
           writer.writeBytes(profile.displayName, NETWORK_PLAYER_NAME_SIZE);
}

bool readProfile(PacketReader& reader, NetworkPlayerProfile& profile)
{
    if (!reader.readU32(profile.deviceID) ||
        !reader.readU8(profile.level) ||
        !reader.readU8(profile.characterClass) ||
        !reader.readBytes(profile.displayName, NETWORK_PLAYER_NAME_SIZE))
        return false;
    profile.displayName[NETWORK_PLAYER_NAME_SIZE - 1] = '\0';
    return profile.deviceID != 0 &&
           profile.characterClass < NETWORK_CHARACTER_CLASS_COUNT;
}
}

bool isKnownPacketType(NetworkPacketType packetType)
{
    switch (packetType)
    {
        case NetworkPacketType::DISCOVERY_BEACON:
        case NetworkPacketType::JOIN_REQUEST:
        case NetworkPacketType::JOIN_ACCEPT:
        case NetworkPacketType::JOIN_REJECT:
        case NetworkPacketType::PLAYER_JOINED:
        case NetworkPacketType::PLAYER_LEFT:
        case NetworkPacketType::HEARTBEAT:
        case NetworkPacketType::DUNGEON_BEGIN:
        case NetworkPacketType::DUNGEON_STATE:
        case NetworkPacketType::ROOM_STATE:
        case NetworkPacketType::PLAYER_MOVE_REQUEST:
        case NetworkPacketType::PLAYER_POSITION:
        case NetworkPacketType::PLAYER_ROOM_CHANGE:
        case NetworkPacketType::COMBAT_BEGIN:
        case NetworkPacketType::COMBAT_STATE:
        case NetworkPacketType::COMBAT_ACTION_REQUEST:
        case NetworkPacketType::COMBAT_ACTION_RESULT:
        case NetworkPacketType::COMBAT_TURN:
        case NetworkPacketType::TRADE_REQUEST:
        case NetworkPacketType::TRADE_STATE:
        case NetworkPacketType::TRADE_CONFIRM:
        case NetworkPacketType::QUEST_STATE:
        case NetworkPacketType::QUEST_CONTRIBUTION:
        case NetworkPacketType::PING:
        case NetworkPacketType::PONG:
        case NetworkPacketType::ERROR_PACKET:
            return true;
    }
    return false;
}

bool isValidAvailability(MultiplayerAvailability availability)
{
    return static_cast<uint8_t>(availability) <=
        static_cast<uint8_t>(MultiplayerAvailability::BUSY);
}

bool isGameplayPacketType(NetworkPacketType packetType)
{
    switch (packetType)
    {
        case NetworkPacketType::DUNGEON_BEGIN:
        case NetworkPacketType::DUNGEON_STATE:
        case NetworkPacketType::ROOM_STATE:
        case NetworkPacketType::PLAYER_MOVE_REQUEST:
        case NetworkPacketType::PLAYER_POSITION:
        case NetworkPacketType::PLAYER_ROOM_CHANGE:
        case NetworkPacketType::COMBAT_BEGIN:
        case NetworkPacketType::COMBAT_STATE:
        case NetworkPacketType::COMBAT_ACTION_REQUEST:
        case NetworkPacketType::COMBAT_ACTION_RESULT:
        case NetworkPacketType::COMBAT_TURN:
        case NetworkPacketType::TRADE_REQUEST:
        case NetworkPacketType::TRADE_STATE:
        case NetworkPacketType::TRADE_CONFIRM:
        case NetworkPacketType::QUEST_STATE:
        case NetworkPacketType::QUEST_CONTRIBUTION:
            return true;
        default:
            return false;
    }
}

bool encodePacket(
    const NetworkPacketHeader& header,
    const uint8_t* payload,
    size_t payloadSize,
    uint8_t* destination,
    size_t destinationCapacity,
    size_t& encodedSize)
{
    encodedSize = 0;
    if (destination == nullptr || payloadSize > NETWORK_MAX_PAYLOAD_SIZE ||
        (payloadSize > 0 && payload == nullptr) ||
        destinationCapacity < NETWORK_PACKET_HEADER_SIZE + payloadSize ||
        !isKnownPacketType(header.packetType))
        return false;

    PacketWriter writer(destination, destinationCapacity);
    if (!writer.writeU16(MULTIPLAYER_PACKET_MAGIC) ||
        !writer.writeU8(MULTIPLAYER_PROTOCOL_VERSION) ||
        !writer.writeU8(static_cast<uint8_t>(header.packetType)) ||
        !writer.writeU8(header.senderPlayerID) ||
        !writer.writeU8(header.flags) ||
        !writer.writeU16(header.sequence) ||
        !writer.writeU16(static_cast<uint16_t>(payloadSize)) ||
        !writer.writeU32(header.sessionID) ||
        (payloadSize > 0 && !writer.writeBytes(payload, payloadSize)))
        return false;

    encodedSize = writer.size();
    return true;
}

bool decodePacket(
    const uint8_t* data,
    size_t dataSize,
    DecodedNetworkPacket& packet)
{
    return decodePacketEnvelope(data, dataSize, packet) &&
           packet.header.protocolVersion == MULTIPLAYER_PROTOCOL_VERSION;
}

bool decodePacketEnvelope(
    const uint8_t* data,
    size_t dataSize,
    DecodedNetworkPacket& packet)
{
    packet = DecodedNetworkPacket{};
    if (data == nullptr || dataSize < NETWORK_PACKET_HEADER_SIZE ||
        dataSize > ESPNOW_MAX_PACKET_SIZE)
        return false;

    PacketReader reader(data, NETWORK_PACKET_HEADER_SIZE);
    uint8_t packetType = 0;
    if (!reader.readU16(packet.header.magic) ||
        !reader.readU8(packet.header.protocolVersion) ||
        !reader.readU8(packetType) ||
        !reader.readU8(packet.header.senderPlayerID) ||
        !reader.readU8(packet.header.flags) ||
        !reader.readU16(packet.header.sequence) ||
        !reader.readU16(packet.header.payloadSize) ||
        !reader.readU32(packet.header.sessionID) || !reader.finished())
        return false;

    packet.header.packetType = static_cast<NetworkPacketType>(packetType);
    if (packet.header.magic != MULTIPLAYER_PACKET_MAGIC ||
        !isKnownPacketType(packet.header.packetType) ||
        packet.header.payloadSize > NETWORK_MAX_PAYLOAD_SIZE ||
        dataSize != NETWORK_PACKET_HEADER_SIZE + packet.header.payloadSize)
        return false;

    packet.payload = data + NETWORK_PACKET_HEADER_SIZE;
    return true;
}

bool encodeDiscoveryBeacon(const DiscoveryBeaconPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writeProfile(writer, payload.profile) ||
        !writer.writeU8(static_cast<uint8_t>(payload.availability)) ||
        !writer.writeU8(payload.acceptingPlayers ? 1 : 0)) return false;
    size = writer.size();
    return true;
}

bool decodeDiscoveryBeacon(const uint8_t* data, size_t size,
    DiscoveryBeaconPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t availability = 0;
    uint8_t accepting = 0;
    if (!readProfile(reader, payload.profile) ||
        !reader.readU8(availability) || !reader.readU8(accepting) ||
        !reader.finished()) return false;
    payload.availability = static_cast<MultiplayerAvailability>(availability);
    payload.acceptingPlayers = accepting != 0;
    return isValidAvailability(payload.availability) && accepting <= 1;
}

bool encodeJoinRequest(const JoinRequestPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writeProfile(writer, payload.profile)) return false;
    size = writer.size();
    return true;
}

bool decodeJoinRequest(const uint8_t* data, size_t size,
    JoinRequestPayload& payload)
{
    PacketReader reader(data, size);
    return readProfile(reader, payload.profile) && reader.finished();
}

bool encodeJoinAccept(const JoinAcceptPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.assignedPlayerID) ||
        !writer.writeU8(payload.playerCapacity)) return false;
    size = writer.size();
    return true;
}

bool decodeJoinAccept(const uint8_t* data, size_t size,
    JoinAcceptPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU8(payload.assignedPlayerID) &&
        reader.readU8(payload.playerCapacity) && reader.finished() &&
        isValidPlayerID(payload.assignedPlayerID) &&
        payload.playerCapacity == MAX_MULTIPLAYER_PLAYERS;
}

bool encodeJoinReject(const JoinRejectPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(static_cast<uint8_t>(payload.reason))) return false;
    size = writer.size();
    return true;
}

bool decodeJoinReject(const uint8_t* data, size_t size,
    JoinRejectPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t reason = 0;
    if (!reader.readU8(reason) || !reader.finished() ||
        reason > static_cast<uint8_t>(JoinRejectReason::INVALID_REQUEST))
        return false;
    payload.reason = static_cast<JoinRejectReason>(reason);
    return true;
}

bool encodePlayerJoined(const PlayerJoinedPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.playerID) ||
        !writeProfile(writer, payload.profile)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerJoined(const uint8_t* data, size_t size,
    PlayerJoinedPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU8(payload.playerID) &&
        readProfile(reader, payload.profile) && reader.finished() &&
        isValidPlayerID(payload.playerID);
}

bool encodePlayerLeft(const PlayerLeftPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.playerID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.reason))) return false;
    size = writer.size();
    return true;
}

bool decodePlayerLeft(const uint8_t* data, size_t size,
    PlayerLeftPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t reason = 0;
    if (!reader.readU8(payload.playerID) || !reader.readU8(reason) ||
        !reader.finished() || !isValidPlayerID(payload.playerID) ||
        reason > static_cast<uint8_t>(PlayerLeaveReason::HOST_ENDED_SESSION))
        return false;
    payload.reason = static_cast<PlayerLeaveReason>(reason);
    return true;
}

bool encodeDungeonBegin(const DungeonBeginPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(static_cast<uint8_t>(payload.syncMode)) ||
        !writer.writeU16(payload.generationVersion) ||
        !writer.writeU32(payload.generationSeed) ||
        !writer.writeU8(payload.roomCount)) return false;
    size = writer.size();
    return true;
}

bool decodeDungeonBegin(const uint8_t* data, size_t size,
    DungeonBeginPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t syncMode = 0;
    if (!reader.readU8(syncMode) ||
        !reader.readU16(payload.generationVersion) ||
        !reader.readU32(payload.generationSeed) ||
        !reader.readU8(payload.roomCount) || !reader.finished() ||
        syncMode != static_cast<uint8_t>(
            DungeonSyncMode::AUTHORITATIVE_GRAPH_AND_ROOM_STATE)) return false;
    payload.syncMode = static_cast<DungeonSyncMode>(syncMode);
    return true;
}

bool encodePlayerMoveRequest(const PlayerMoveRequestPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.direction) ||
        !writer.writeU16(payload.clientMovementSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerMoveRequest(const uint8_t* data, size_t size,
    PlayerMoveRequestPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU8(payload.direction) &&
        reader.readU16(payload.clientMovementSequence) && reader.finished() &&
        payload.direction < 8;
}

bool encodePlayerPosition(const PlayerPositionPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.playerID) ||
        !writer.writeU8(payload.roomID) || !writer.writeU8(payload.x) ||
        !writer.writeU8(payload.y) ||
        !writer.writeU16(payload.movementSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerPosition(const uint8_t* data, size_t size,
    PlayerPositionPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU8(payload.playerID) && reader.readU8(payload.roomID) &&
        reader.readU8(payload.x) && reader.readU8(payload.y) &&
        reader.readU16(payload.movementSequence) && reader.finished() &&
        isValidPlayerID(payload.playerID);
}

bool encodePlayerRoomChange(const PlayerRoomChangePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writer.writeU8(payload.playerID) ||
        !writer.writeU8(payload.roomID) ||
        !writer.writeU8(payload.entryDirection) ||
        !writer.writeU8(payload.x) || !writer.writeU8(payload.y) ||
        !writer.writeU16(payload.movementSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerRoomChange(const uint8_t* data, size_t size,
    PlayerRoomChangePayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU8(payload.playerID) && reader.readU8(payload.roomID) &&
        reader.readU8(payload.entryDirection) && reader.readU8(payload.x) &&
        reader.readU8(payload.y) &&
        reader.readU16(payload.movementSequence) && reader.finished() &&
        isValidPlayerID(payload.playerID) && payload.entryDirection < 8;
}
