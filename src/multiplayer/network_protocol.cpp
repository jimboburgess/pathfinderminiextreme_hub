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

bool validActivityType(MultiplayerActivityType activityType)
{
    return activityType == MultiplayerActivityType::FOREST ||
           activityType == MultiplayerActivityType::DUNGEON;
}

bool writeCharacterState(
    PacketWriter& writer,
    const NetworkCharacterState& character)
{
    if (!isValidPlayerID(character.playerID) ||
        character.characterClass >= NETWORK_CHARACTER_CLASS_COUNT)
        return false;
    if (character.knownAbilityCount > NETWORK_KNOWN_ABILITY_CAPACITY ||
        character.conditionCount > NETWORK_CONDITION_CAPACITY)
        return false;
    if (!writer.writeU8(character.playerID) ||
        !writer.writeBytes(character.displayName, NETWORK_PLAYER_NAME_SIZE) ||
        !writer.writeU8(character.characterClass) ||
        !writer.writeU8(character.level) ||
        !writer.writeU16(static_cast<uint16_t>(character.currentHP)) ||
        !writer.writeU16(static_cast<uint16_t>(character.maxHP)) ||
        !writer.writeBytes(character.abilityScores,
                           sizeof(character.abilityScores)) ||
        !writer.writeU8(character.speed)) return false;
    if (!writer.writeU8(character.characterState) ||
        !writer.writeU16(static_cast<uint16_t>(character.currentMP)) ||
        !writer.writeU16(static_cast<uint16_t>(character.maxMP)) ||
        !writer.writeU8(static_cast<uint8_t>(character.initiative)) ||
        !writer.writeU8(character.trainedWeaponGroup) ||
        !writer.writeU8(character.channelEnergyCurrent) ||
        !writer.writeU8(character.channelEnergyMax)) return false;
    for (const NetworkCharacterState::EquipmentItem& item : character.equipment)
        if (!writer.writeU8(item.itemID) ||
            !writer.writeU8(static_cast<uint8_t>(item.enhancementBonus)) ||
            !writer.writeU8(item.weaponProperties)) return false;
    if (!writer.writeU8(character.knownAbilityCount)) return false;
    for (uint8_t index = 0; index < NETWORK_KNOWN_ABILITY_CAPACITY; ++index)
        if (!writer.writeU16(character.knownAbilities[index])) return false;
    if (!writer.writeU8(character.conditionCount)) return false;
    for (const NetworkCharacterState::ConditionState& condition :
         character.conditions)
        if (!writer.writeU8(condition.type) ||
            !writer.writeU16(static_cast<uint16_t>(condition.value)) ||
            !writer.writeU16(static_cast<uint16_t>(condition.roundsRemaining)))
            return false;
    return true;
}

bool readCharacterState(
    PacketReader& reader,
    NetworkCharacterState& character)
{
    uint16_t currentHP = 0;
    uint16_t maxHP = 0;
    uint16_t currentMP = 0;
    uint16_t maxMP = 0;
    uint8_t initiative = 0;
    if (!reader.readU8(character.playerID) ||
        !reader.readBytes(character.displayName, NETWORK_PLAYER_NAME_SIZE) ||
        !reader.readU8(character.characterClass) ||
        !reader.readU8(character.level) ||
        !reader.readU16(currentHP) || !reader.readU16(maxHP) ||
        !reader.readBytes(character.abilityScores,
                          sizeof(character.abilityScores)) ||
        !reader.readU8(character.speed) ||
        !reader.readU8(character.characterState) ||
        !reader.readU16(currentMP) || !reader.readU16(maxMP) ||
        !reader.readU8(initiative) ||
        !reader.readU8(character.trainedWeaponGroup) ||
        !reader.readU8(character.channelEnergyCurrent) ||
        !reader.readU8(character.channelEnergyMax))
        return false;
    for (NetworkCharacterState::EquipmentItem& item : character.equipment)
    {
        uint8_t enhancementBonus = 0;
        if (!reader.readU8(item.itemID) || !reader.readU8(enhancementBonus) ||
            !reader.readU8(item.weaponProperties)) return false;
        item.enhancementBonus = static_cast<int8_t>(enhancementBonus);
    }
    if (!reader.readU8(character.knownAbilityCount) ||
        character.knownAbilityCount > NETWORK_KNOWN_ABILITY_CAPACITY)
        return false;
    for (uint16_t& ability : character.knownAbilities)
        if (!reader.readU16(ability)) return false;
    if (!reader.readU8(character.conditionCount) ||
        character.conditionCount > NETWORK_CONDITION_CAPACITY) return false;
    for (NetworkCharacterState::ConditionState& condition :
         character.conditions)
    {
        uint16_t value = 0;
        uint16_t rounds = 0;
        if (!reader.readU8(condition.type) || !reader.readU16(value) ||
            !reader.readU16(rounds)) return false;
        condition.value = static_cast<int16_t>(value);
        condition.roundsRemaining = static_cast<int16_t>(rounds);
    }
    character.displayName[NETWORK_PLAYER_NAME_SIZE - 1] = '\0';
    character.currentHP = static_cast<int16_t>(currentHP);
    character.maxHP = static_cast<int16_t>(maxHP);
    character.currentMP = static_cast<int16_t>(currentMP);
    character.maxMP = static_cast<int16_t>(maxMP);
    character.initiative = static_cast<int8_t>(initiative);
    return isValidPlayerID(character.playerID) &&
           character.characterClass < NETWORK_CHARACTER_CLASS_COUNT &&
           character.maxHP > 0;
}

bool validRoomID(uint8_t roomID)
{
    return roomID < NETWORK_MAX_DUNGEON_ROOMS;
}

bool validCombatantKind(MultiplayerCombatantKind kind)
{
    return kind == MultiplayerCombatantKind::PLAYER ||
           kind == MultiplayerCombatantKind::MONSTER;
}

bool validCombatantRef(const MultiplayerCombatantRef& combatant)
{
    if (!validCombatantKind(combatant.kind) || !validRoomID(combatant.roomID))
        return false;
    if (combatant.kind == MultiplayerCombatantKind::PLAYER)
        return isValidPlayerID(combatant.playerID) && combatant.entityID == 0;
    return combatant.playerID == INVALID_PLAYER_ID && combatant.entityID != 0;
}

bool writeCombatantRef(
    PacketWriter& writer,
    const MultiplayerCombatantRef& combatant)
{
    return validCombatantRef(combatant) &&
           writer.writeU8(static_cast<uint8_t>(combatant.kind)) &&
           writer.writeU8(combatant.playerID) &&
           writer.writeU8(combatant.roomID) &&
           writer.writeU8(combatant.entityID);
}

bool readCombatantRef(
    PacketReader& reader,
    MultiplayerCombatantRef& combatant)
{
    uint8_t kind = 0;
    if (!reader.readU8(kind) || !reader.readU8(combatant.playerID) ||
        !reader.readU8(combatant.roomID) || !reader.readU8(combatant.entityID))
        return false;
    combatant.kind = static_cast<MultiplayerCombatantKind>(kind);
    return validCombatantRef(combatant);
}

bool writeOptionalCombatantRef(
    PacketWriter& writer,
    const MultiplayerCombatantRef& combatant)
{
    if (combatant.kind == MultiplayerCombatantKind::NONE)
        return writer.writeU8(static_cast<uint8_t>(combatant.kind)) &&
               writer.writeU8(INVALID_PLAYER_ID) &&
               writer.writeU8(NETWORK_NO_ROOM) && writer.writeU8(0);
    return writeCombatantRef(writer, combatant);
}

bool readOptionalCombatantRef(
    PacketReader& reader,
    MultiplayerCombatantRef& combatant)
{
    uint8_t kind = 0;
    if (!reader.readU8(kind) || !reader.readU8(combatant.playerID) ||
        !reader.readU8(combatant.roomID) || !reader.readU8(combatant.entityID))
        return false;
    combatant.kind = static_cast<MultiplayerCombatantKind>(kind);
    if (combatant.kind == MultiplayerCombatantKind::NONE)
        return combatant.playerID == INVALID_PLAYER_ID &&
               combatant.roomID == NETWORK_NO_ROOM && combatant.entityID == 0;
    return validCombatantRef(combatant);
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
        case NetworkPacketType::TRAVEL_INVITE:
        case NetworkPacketType::TRAVEL_ACCEPT:
        case NetworkPacketType::TRAVEL_DECLINE:
        case NetworkPacketType::ACTIVITY_PREPARE:
        case NetworkPacketType::ACTIVITY_READY:
        case NetworkPacketType::ACTIVITY_START:
        case NetworkPacketType::ACTIVITY_LEAVE:
        case NetworkPacketType::ACTIVITY_END:
        case NetworkPacketType::DUNGEON_BEGIN:
        case NetworkPacketType::DUNGEON_GRAPH:
        case NetworkPacketType::SNAPSHOT_CHUNK:
        case NetworkPacketType::FOREST_BEGIN:
        case NetworkPacketType::PLAYER_CHARACTER_STATE:
        case NetworkPacketType::PLAYER_SPAWN:
        case NetworkPacketType::PLAYER_DESPAWN:
        case NetworkPacketType::ROOM_TRANSITION:
        case NetworkPacketType::ROOM_READY:
        case NetworkPacketType::ACK:
        case NetworkPacketType::SHARED_GAMEPLAY_MESSAGE:
        case NetworkPacketType::PLAYER_ROOM_TRANSITION_REQUEST:
        case NetworkPacketType::PLAYER_MOVE_REQUEST:
        case NetworkPacketType::PLAYER_POSITION:
        case NetworkPacketType::PLAYER_ROOM_CHANGE:
        case NetworkPacketType::COMBAT_BEGIN:
        case NetworkPacketType::COMBAT_STATE:
        case NetworkPacketType::COMBAT_ACTION_REQUEST:
        case NetworkPacketType::COMBAT_ACTION_RESULT:
        case NetworkPacketType::COMBAT_TURN:
        case NetworkPacketType::COMBAT_END:
        case NetworkPacketType::COMBAT_ACK:
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
        case NetworkPacketType::TRAVEL_INVITE:
        case NetworkPacketType::TRAVEL_ACCEPT:
        case NetworkPacketType::TRAVEL_DECLINE:
        case NetworkPacketType::ACTIVITY_PREPARE:
        case NetworkPacketType::ACTIVITY_READY:
        case NetworkPacketType::ACTIVITY_START:
        case NetworkPacketType::ACTIVITY_LEAVE:
        case NetworkPacketType::ACTIVITY_END:
        case NetworkPacketType::DUNGEON_BEGIN:
        case NetworkPacketType::DUNGEON_GRAPH:
        case NetworkPacketType::SNAPSHOT_CHUNK:
        case NetworkPacketType::FOREST_BEGIN:
        case NetworkPacketType::PLAYER_CHARACTER_STATE:
        case NetworkPacketType::PLAYER_SPAWN:
        case NetworkPacketType::PLAYER_DESPAWN:
        case NetworkPacketType::ROOM_TRANSITION:
        case NetworkPacketType::ROOM_READY:
        case NetworkPacketType::ACK:
        case NetworkPacketType::SHARED_GAMEPLAY_MESSAGE:
        case NetworkPacketType::PLAYER_ROOM_TRANSITION_REQUEST:
        case NetworkPacketType::PLAYER_MOVE_REQUEST:
        case NetworkPacketType::PLAYER_POSITION:
        case NetworkPacketType::PLAYER_ROOM_CHANGE:
        case NetworkPacketType::COMBAT_BEGIN:
        case NetworkPacketType::COMBAT_STATE:
        case NetworkPacketType::COMBAT_ACTION_REQUEST:
        case NetworkPacketType::COMBAT_ACTION_RESULT:
        case NetworkPacketType::COMBAT_TURN:
        case NetworkPacketType::COMBAT_END:
        case NetworkPacketType::COMBAT_ACK:
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

bool encodeTravelInvite(const TravelInvitePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !validActivityType(payload.activityType) ||
        payload.hostPlayerID != HOST_PLAYER_ID ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.activityType)) ||
        !writer.writeU8(payload.hostPlayerID)) return false;
    size = writer.size();
    return true;
}

bool decodeTravelInvite(const uint8_t* data, size_t size,
    TravelInvitePayload& payload)
{
    PacketReader reader(data, size);
    uint8_t activityType = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU8(activityType) ||
        !reader.readU8(payload.hostPlayerID) || !reader.finished())
        return false;
    payload.activityType = static_cast<MultiplayerActivityType>(activityType);
    return payload.activityID != 0 && validActivityType(payload.activityType) &&
           payload.hostPlayerID == HOST_PLAYER_ID;
}

bool encodeTravelResponse(const TravelResponsePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writeCharacterState(writer, payload.character)) return false;
    size = writer.size();
    return true;
}

bool decodeTravelResponse(const uint8_t* data, size_t size,
    TravelResponsePayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) && payload.activityID != 0 &&
           readCharacterState(reader, payload.character) && reader.finished();
}

bool encodeActivityPrepare(const ActivityPreparePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !validActivityType(payload.activityType) ||
        payload.graphChunkCount > MAX_SNAPSHOT_CHUNKS ||
        payload.detailChunkCount > MAX_SNAPSHOT_CHUNKS ||
        payload.worldChunkCount == 0 ||
        payload.worldChunkCount > MAX_SNAPSHOT_CHUNKS ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.activityType)) ||
        !writer.writeU8(payload.roomID) ||
        !writer.writeU16(payload.snapshotEpoch) ||
        !writer.writeU8(payload.participantMask) ||
        !writer.writeU8(payload.roomPlayerMask) ||
        !writer.writeU8(payload.graphChunkCount) ||
        !writer.writeU8(payload.detailChunkCount) ||
        !writer.writeU8(payload.worldChunkCount)) return false;
    size = writer.size();
    return true;
}

bool decodeActivityPrepare(const uint8_t* data, size_t size,
    ActivityPreparePayload& payload)
{
    PacketReader reader(data, size);
    uint8_t activityType = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU8(activityType) || !reader.readU8(payload.roomID) ||
        !reader.readU16(payload.snapshotEpoch) ||
        !reader.readU8(payload.participantMask) ||
        !reader.readU8(payload.roomPlayerMask) ||
        !reader.readU8(payload.graphChunkCount) ||
        !reader.readU8(payload.detailChunkCount) ||
        !reader.readU8(payload.worldChunkCount) || !reader.finished())
        return false;
    payload.activityType = static_cast<MultiplayerActivityType>(activityType);
    return payload.activityID != 0 && validActivityType(payload.activityType) &&
           ((payload.activityType == MultiplayerActivityType::FOREST &&
             payload.roomID == NETWORK_NO_ROOM) ||
            (payload.activityType == MultiplayerActivityType::DUNGEON &&
             payload.roomID < NETWORK_MAX_DUNGEON_ROOMS)) &&
           payload.graphChunkCount <= MAX_SNAPSHOT_CHUNKS &&
           payload.detailChunkCount <= MAX_SNAPSHOT_CHUNKS &&
           payload.worldChunkCount > 0 &&
           payload.worldChunkCount <= MAX_SNAPSHOT_CHUNKS &&
           payload.roomPlayerMask != 0 &&
           (payload.roomPlayerMask & ~payload.participantMask) == 0 &&
           (payload.participantMask & ~((1u << MAX_MULTIPLAYER_PLAYERS) - 1u)) == 0;
}

bool encodeActivityReady(const ActivityReadyPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.snapshotEpoch)) return false;
    size = writer.size();
    return true;
}

bool decodeActivityReady(const uint8_t* data, size_t size,
    ActivityReadyPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
           reader.readU16(payload.snapshotEpoch) && reader.finished() &&
           payload.activityID != 0;
}

bool encodeActivityMember(const ActivityMemberPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !isValidPlayerID(payload.playerID) ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.playerID)) return false;
    size = writer.size();
    return true;
}

bool decodeActivityMember(const uint8_t* data, size_t size,
    ActivityMemberPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
           reader.readU8(payload.playerID) && reader.finished() &&
           payload.activityID != 0 && isValidPlayerID(payload.playerID);
}

bool encodeNetworkCharacterState(const NetworkCharacterState& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (!writeCharacterState(writer, payload)) return false;
    size = writer.size();
    return true;
}

bool decodeNetworkCharacterState(const uint8_t* data, size_t size,
    NetworkCharacterState& payload)
{
    PacketReader reader(data, size);
    return readCharacterState(reader, payload) && reader.finished();
}

bool encodePlayerCharacterState(const PlayerCharacterStatePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writeCharacterState(writer, payload.character)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerCharacterState(const uint8_t* data, size_t size,
    PlayerCharacterStatePayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) && payload.activityID != 0 &&
           readCharacterState(reader, payload.character) && reader.finished();
}

bool encodePlayerSpawn(const PlayerSpawnPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !isValidPlayerID(payload.playerID) ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.playerID) || !writer.writeU8(payload.roomID) ||
        !writer.writeU8(payload.x) || !writer.writeU8(payload.y)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerSpawn(const uint8_t* data, size_t size,
    PlayerSpawnPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
           reader.readU8(payload.playerID) && reader.readU8(payload.roomID) &&
           reader.readU8(payload.x) && reader.readU8(payload.y) &&
           reader.finished() && payload.activityID != 0 &&
           isValidPlayerID(payload.playerID) &&
           (payload.roomID == NETWORK_NO_ROOM ||
            payload.roomID < NETWORK_MAX_DUNGEON_ROOMS);
}

bool encodeSnapshotChunk(const SnapshotChunkPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || payload.totalChunks == 0 ||
        payload.totalChunks > MAX_SNAPSHOT_CHUNKS ||
        payload.chunkIndex >= payload.totalChunks ||
        payload.payloadLength == 0 ||
        payload.payloadLength > SNAPSHOT_CHUNK_DATA_SIZE ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.snapshotType)) ||
        !writer.writeU8(payload.roomID) ||
        !writer.writeU16(payload.snapshotEpoch) ||
        !writer.writeU8(payload.chunkIndex) ||
        !writer.writeU8(payload.totalChunks) ||
        !writer.writeU8(payload.payloadLength) ||
        !writer.writeBytes(payload.payload, payload.payloadLength)) return false;
    size = writer.size();
    return true;
}

bool decodeSnapshotChunk(const uint8_t* data, size_t size,
    SnapshotChunkPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t snapshotType = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU8(snapshotType) || !reader.readU8(payload.roomID) ||
        !reader.readU16(payload.snapshotEpoch) ||
        !reader.readU8(payload.chunkIndex) ||
        !reader.readU8(payload.totalChunks) ||
        !reader.readU8(payload.payloadLength) ||
        payload.payloadLength == 0 ||
        payload.payloadLength > SNAPSHOT_CHUNK_DATA_SIZE ||
        !reader.readBytes(payload.payload, payload.payloadLength) ||
        !reader.finished()) return false;
    payload.snapshotType = static_cast<SnapshotType>(snapshotType);
    return payload.activityID != 0 &&
           snapshotType >= static_cast<uint8_t>(SnapshotType::DUNGEON_GRAPH) &&
           snapshotType <= static_cast<uint8_t>(SnapshotType::DUNGEON_ROOM_DETAIL) &&
           payload.totalChunks > 0 &&
           payload.totalChunks <= MAX_SNAPSHOT_CHUNKS &&
           payload.chunkIndex < payload.totalChunks &&
           ((payload.snapshotType == SnapshotType::DUNGEON_GRAPH &&
             payload.roomID == NETWORK_NO_ROOM) ||
            (payload.snapshotType == SnapshotType::FOREST_STATE &&
             payload.roomID == NETWORK_NO_ROOM) ||
            (payload.snapshotType == SnapshotType::DUNGEON_ROOM &&
             payload.roomID < NETWORK_MAX_DUNGEON_ROOMS) ||
           (payload.snapshotType == SnapshotType::DUNGEON_ROOM_DETAIL &&
             payload.roomID < NETWORK_MAX_DUNGEON_ROOMS));
}

bool encodeActivityAck(const ActivityAckPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.snapshotEpoch) ||
        !writer.writeU8(static_cast<uint8_t>(payload.packetType)) ||
        !writer.writeU8(static_cast<uint8_t>(payload.snapshotType)) ||
        !writer.writeU8(payload.roomID) ||
        !writer.writeU8(payload.itemIndex) ||
        !writer.writeU8(payload.chunkIndex)) return false;
    size = writer.size();
    return true;
}

bool decodeActivityAck(const uint8_t* data, size_t size,
    ActivityAckPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t packetType = 0;
    uint8_t snapshotType = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.snapshotEpoch) ||
        !reader.readU8(packetType) || !reader.readU8(snapshotType) ||
        !reader.readU8(payload.roomID) ||
        !reader.readU8(payload.itemIndex) ||
        !reader.readU8(payload.chunkIndex) || !reader.finished()) return false;
    payload.packetType = static_cast<NetworkPacketType>(packetType);
    payload.snapshotType = static_cast<SnapshotType>(snapshotType);
    const bool reliablePacket =
        payload.packetType == NetworkPacketType::ACTIVITY_PREPARE ||
        payload.packetType == NetworkPacketType::DUNGEON_BEGIN ||
        payload.packetType == NetworkPacketType::FOREST_BEGIN ||
        payload.packetType == NetworkPacketType::ROOM_TRANSITION ||
        payload.packetType == NetworkPacketType::ACTIVITY_START ||
        payload.packetType == NetworkPacketType::PLAYER_CHARACTER_STATE ||
        payload.packetType == NetworkPacketType::SNAPSHOT_CHUNK ||
        payload.packetType == NetworkPacketType::PLAYER_SPAWN;
    return payload.activityID != 0 && reliablePacket &&
           snapshotType >= static_cast<uint8_t>(SnapshotType::DUNGEON_GRAPH) &&
           snapshotType <= static_cast<uint8_t>(SnapshotType::DUNGEON_ROOM_DETAIL) &&
           (payload.roomID == NETWORK_NO_ROOM ||
            payload.roomID < NETWORK_MAX_DUNGEON_ROOMS) &&
           payload.itemIndex < MAX_MULTIPLAYER_PLAYERS &&
           payload.chunkIndex < MAX_SNAPSHOT_CHUNKS;
}

bool encodeRoomTransition(const RoomTransitionPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || payload.roomID == NETWORK_NO_ROOM ||
        payload.entryDirection > 4 ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.roomID) ||
        !writer.writeU8(payload.entryDirection) ||
        !writer.writeU16(payload.snapshotEpoch)) return false;
    size = writer.size();
    return true;
}

bool decodeRoomTransition(const uint8_t* data, size_t size,
    RoomTransitionPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
           reader.readU8(payload.roomID) &&
           reader.readU8(payload.entryDirection) &&
           reader.readU16(payload.snapshotEpoch) && reader.finished() &&
           payload.activityID != 0 &&
           payload.roomID < NETWORK_MAX_DUNGEON_ROOMS &&
           payload.entryDirection <= 4;
}

bool encodePlayerRoomTransitionRequest(
    const PlayerRoomTransitionRequestPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !validRoomID(payload.sourceRoomID) ||
        payload.direction >= 8 ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.sourceRoomID) ||
        !writer.writeU8(payload.direction) ||
        !writer.writeU16(payload.transitionSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerRoomTransitionRequest(
    const uint8_t* data, size_t size,
    PlayerRoomTransitionRequestPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
           reader.readU8(payload.sourceRoomID) &&
           reader.readU8(payload.direction) &&
           reader.readU16(payload.transitionSequence) && reader.finished() &&
           payload.activityID != 0 && validRoomID(payload.sourceRoomID) &&
           payload.direction < 8;
}

bool encodeSharedGameplayMessage(
    const SharedGameplayMessagePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    const uint8_t category = static_cast<uint8_t>(payload.category);
    if (payload.activityID == 0 ||
        category > static_cast<uint8_t>(SharedMessageCategory::SYSTEM) ||
        payload.textLength == 0 ||
        payload.textLength >= SHARED_MESSAGE_TEXT_SIZE ||
        payload.text[payload.textLength] != '\0' ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.messageSequence) ||
        !writer.writeU8(category) || !writer.writeU8(payload.textLength) ||
        !writer.writeBytes(payload.text, payload.textLength)) return false;
    size = writer.size();
    return true;
}

bool decodeSharedGameplayMessage(
    const uint8_t* data, size_t size,
    SharedGameplayMessagePayload& payload)
{
    PacketReader reader(data, size);
    uint8_t category = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.messageSequence) ||
        !reader.readU8(category) || !reader.readU8(payload.textLength) ||
        category > static_cast<uint8_t>(SharedMessageCategory::SYSTEM) ||
        payload.textLength == 0 || payload.textLength >= SHARED_MESSAGE_TEXT_SIZE ||
        !reader.readBytes(payload.text, payload.textLength) || !reader.finished())
        return false;
    payload.category = static_cast<SharedMessageCategory>(category);
    payload.text[payload.textLength] = '\0';
    return payload.activityID != 0;
}

bool encodeMultiplayerCombatBegin(
    const MultiplayerCombatBeginPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || payload.combatID == 0 ||
        payload.combatantCount == 0 || payload.combatantCount > 16 ||
        payload.currentTurnIndex >= payload.combatantCount ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.combatID) ||
        !writer.writeU16(payload.eventSequence) ||
        !writer.writeU8(payload.combatantCount) ||
        !writer.writeU8(payload.currentTurnIndex) ||
        !writer.writeU16(payload.round)) return false;
    for (uint8_t index = 0; index < payload.combatantCount; ++index)
        if (!writeCombatantRef(writer, payload.combatants[index].combatant) ||
            !writer.writeU8(static_cast<uint8_t>(
                payload.combatants[index].initiative))) return false;
    size = writer.size();
    return true;
}

bool decodeMultiplayerCombatBegin(
    const uint8_t* data, size_t size,
    MultiplayerCombatBeginPayload& payload)
{
    PacketReader reader(data, size);
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.combatID) ||
        !reader.readU16(payload.eventSequence) ||
        !reader.readU8(payload.combatantCount) ||
        !reader.readU8(payload.currentTurnIndex) ||
        !reader.readU16(payload.round) || payload.activityID == 0 ||
        payload.combatID == 0 || payload.combatantCount == 0 ||
        payload.combatantCount > 16 ||
        payload.currentTurnIndex >= payload.combatantCount) return false;
    for (uint8_t index = 0; index < payload.combatantCount; ++index)
    {
        uint8_t initiative = 0;
        if (!readCombatantRef(reader, payload.combatants[index].combatant) ||
            !reader.readU8(initiative)) return false;
        payload.combatants[index].initiative = static_cast<int8_t>(initiative);
    }
    return reader.finished();
}

bool encodeMultiplayerCombatActionRequest(
    const MultiplayerCombatActionRequestPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    const uint8_t action = static_cast<uint8_t>(payload.action);
    if (payload.activityID == 0 || payload.combatID == 0 ||
        action == 0 ||
        action > static_cast<uint8_t>(MultiplayerCombatActionType::END_TURN) ||
        payload.direction >= 8 ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.combatID) ||
        !writer.writeU16(payload.actionSequence) || !writer.writeU8(action) ||
        !writeCombatantRef(writer, payload.actor) ||
        !writeOptionalCombatantRef(writer, payload.target) ||
        !writer.writeU16(payload.abilityID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.targetX)) ||
        !writer.writeU8(static_cast<uint8_t>(payload.targetY)) ||
        !writer.writeU8(payload.direction) ||
        !writer.writeU8(payload.damageType) ||
        !writer.writeU8(payload.flags)) return false;
    size = writer.size();
    return true;
}

bool decodeMultiplayerCombatActionRequest(
    const uint8_t* data, size_t size,
    MultiplayerCombatActionRequestPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t action = 0;
    uint8_t targetX = 0;
    uint8_t targetY = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.combatID) ||
        !reader.readU16(payload.actionSequence) || !reader.readU8(action) ||
        !readCombatantRef(reader, payload.actor) ||
        !readOptionalCombatantRef(reader, payload.target) ||
        !reader.readU16(payload.abilityID) || !reader.readU8(targetX) ||
        !reader.readU8(targetY) || !reader.readU8(payload.direction) ||
        !reader.readU8(payload.damageType) || !reader.readU8(payload.flags) ||
        !reader.finished()) return false;
    payload.action = static_cast<MultiplayerCombatActionType>(action);
    payload.targetX = static_cast<int8_t>(targetX);
    payload.targetY = static_cast<int8_t>(targetY);
    return payload.activityID != 0 && payload.combatID != 0 && action != 0 &&
        action <= static_cast<uint8_t>(MultiplayerCombatActionType::END_TURN) &&
        payload.direction < 8;
}

bool encodeMultiplayerCombatEvent(
    const MultiplayerCombatEventPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    const uint8_t eventType = static_cast<uint8_t>(payload.eventType);
    if (payload.activityID == 0 || payload.combatID == 0 ||
        eventType > static_cast<uint8_t>(MultiplayerCombatEventType::COMBAT_ENDED) ||
        payload.textLength >= SHARED_MESSAGE_TEXT_SIZE ||
        payload.text[payload.textLength] != '\0' ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.combatID) ||
        !writer.writeU16(payload.eventSequence) || !writer.writeU8(eventType) ||
        !writer.writeU8(payload.currentTurnIndex) ||
        !writer.writeU16(payload.round) ||
        !writeOptionalCombatantRef(writer, payload.actor) ||
        !writeOptionalCombatantRef(writer, payload.target) ||
        !writer.writeU8(payload.actorX) || !writer.writeU8(payload.actorY) ||
        !writer.writeU8(payload.targetX) || !writer.writeU8(payload.targetY) ||
        !writer.writeU16(static_cast<uint16_t>(payload.actorHP)) ||
        !writer.writeU16(static_cast<uint16_t>(payload.targetHP)) ||
        !writer.writeU8(payload.actorState) ||
        !writer.writeU8(payload.targetState) ||
        !writer.writeU16(payload.roomRevision) ||
        !writer.writeU8(payload.textLength) ||
        (payload.textLength > 0 &&
         !writer.writeBytes(payload.text, payload.textLength))) return false;
    size = writer.size();
    return true;
}

bool decodeMultiplayerCombatEvent(
    const uint8_t* data, size_t size,
    MultiplayerCombatEventPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t eventType = 0;
    uint16_t actorHP = 0;
    uint16_t targetHP = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.combatID) ||
        !reader.readU16(payload.eventSequence) || !reader.readU8(eventType) ||
        !reader.readU8(payload.currentTurnIndex) ||
        !reader.readU16(payload.round) ||
        !readOptionalCombatantRef(reader, payload.actor) ||
        !readOptionalCombatantRef(reader, payload.target) ||
        !reader.readU8(payload.actorX) || !reader.readU8(payload.actorY) ||
        !reader.readU8(payload.targetX) || !reader.readU8(payload.targetY) ||
        !reader.readU16(actorHP) || !reader.readU16(targetHP) ||
        !reader.readU8(payload.actorState) ||
        !reader.readU8(payload.targetState) ||
        !reader.readU16(payload.roomRevision) ||
        !reader.readU8(payload.textLength) ||
        eventType > static_cast<uint8_t>(MultiplayerCombatEventType::COMBAT_ENDED) ||
        payload.textLength >= SHARED_MESSAGE_TEXT_SIZE ||
        (payload.textLength > 0 &&
         !reader.readBytes(payload.text, payload.textLength)) ||
        !reader.finished()) return false;
    payload.eventType = static_cast<MultiplayerCombatEventType>(eventType);
    payload.actorHP = static_cast<int16_t>(actorHP);
    payload.targetHP = static_cast<int16_t>(targetHP);
    payload.text[payload.textLength] = '\0';
    return payload.activityID != 0 && payload.combatID != 0;
}

bool encodeMultiplayerCombatAck(
    const MultiplayerCombatAckPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || payload.combatID == 0 ||
        (payload.packetType != NetworkPacketType::COMBAT_BEGIN &&
         payload.packetType != NetworkPacketType::COMBAT_ACTION_RESULT &&
         payload.packetType != NetworkPacketType::COMBAT_TURN &&
         payload.packetType != NetworkPacketType::COMBAT_END) ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU16(payload.combatID) ||
        !writer.writeU16(payload.eventSequence) ||
        !writer.writeU8(static_cast<uint8_t>(payload.packetType)) ||
        !writer.writeU8(payload.reserved)) return false;
    size = writer.size();
    return true;
}

bool decodeMultiplayerCombatAck(
    const uint8_t* data, size_t size,
    MultiplayerCombatAckPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t packetType = 0;
    if (!reader.readU32(payload.activityID) ||
        !reader.readU16(payload.combatID) ||
        !reader.readU16(payload.eventSequence) ||
        !reader.readU8(packetType) || !reader.readU8(payload.reserved) ||
        !reader.finished()) return false;
    payload.packetType = static_cast<NetworkPacketType>(packetType);
    return payload.activityID != 0 && payload.combatID != 0 &&
        (payload.packetType == NetworkPacketType::COMBAT_BEGIN ||
         payload.packetType == NetworkPacketType::COMBAT_ACTION_RESULT ||
         payload.packetType == NetworkPacketType::COMBAT_TURN ||
         payload.packetType == NetworkPacketType::COMBAT_END);
}

bool encodeDungeonBegin(const DungeonBeginPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 ||
        !writer.writeU32(payload.activityID) ||
        !writer.writeU8(static_cast<uint8_t>(payload.syncMode)) ||
        !writer.writeU16(payload.generationVersion) ||
        !writer.writeU32(payload.generationSeed) ||
        !writer.writeU8(payload.roomCount) ||
        !writer.writeU8(payload.entranceRoom) ||
        !writer.writeU16(payload.snapshotEpoch)) return false;
    size = writer.size();
    return true;
}

bool decodeDungeonBegin(const uint8_t* data, size_t size,
    DungeonBeginPayload& payload)
{
    PacketReader reader(data, size);
    uint8_t syncMode = 0;
    if (!reader.readU32(payload.activityID) || !reader.readU8(syncMode) ||
        !reader.readU16(payload.generationVersion) ||
        !reader.readU32(payload.generationSeed) ||
        !reader.readU8(payload.roomCount) ||
        !reader.readU8(payload.entranceRoom) ||
        !reader.readU16(payload.snapshotEpoch) || !reader.finished() ||
        payload.activityID == 0 || payload.roomCount < 7 ||
        payload.roomCount > NETWORK_MAX_DUNGEON_ROOMS ||
        payload.entranceRoom >= payload.roomCount ||
        syncMode != static_cast<uint8_t>(
            DungeonSyncMode::AUTHORITATIVE_GRAPH_AND_ROOM_STATE)) return false;
    payload.syncMode = static_cast<DungeonSyncMode>(syncMode);
    return true;
}

bool encodePlayerMoveRequest(const PlayerMoveRequestPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.direction) ||
        !writer.writeU16(payload.clientMovementSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerMoveRequest(const uint8_t* data, size_t size,
    PlayerMoveRequestPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
        reader.readU8(payload.direction) &&
        reader.readU16(payload.clientMovementSequence) && reader.finished() &&
        payload.activityID != 0 && payload.direction < 8;
}

bool encodePlayerPosition(const PlayerPositionPayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.playerID) ||
        !writer.writeU8(payload.roomID) || !writer.writeU8(payload.x) ||
        !writer.writeU8(payload.y) || !writer.writeU8(payload.facing) ||
        !writer.writeU16(payload.movementSequence)) return false;
    size = writer.size();
    return true;
}

bool decodePlayerPosition(const uint8_t* data, size_t size,
    PlayerPositionPayload& payload)
{
    PacketReader reader(data, size);
    return reader.readU32(payload.activityID) &&
        reader.readU8(payload.playerID) && reader.readU8(payload.roomID) &&
        reader.readU8(payload.x) && reader.readU8(payload.y) &&
        reader.readU8(payload.facing) &&
        reader.readU16(payload.movementSequence) && reader.finished() &&
        payload.activityID != 0 && isValidPlayerID(payload.playerID) &&
        (payload.roomID == NETWORK_NO_ROOM ||
         payload.roomID < NETWORK_MAX_DUNGEON_ROOMS) &&
        payload.facing < 8;
}

bool encodePlayerRoomChange(const PlayerRoomChangePayload& payload,
    uint8_t* destination, size_t capacity, size_t& size)
{
    PacketWriter writer(destination, capacity);
    if (payload.activityID == 0 || !writer.writeU32(payload.activityID) ||
        !writer.writeU8(payload.playerID) ||
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
    return reader.readU32(payload.activityID) &&
        reader.readU8(payload.playerID) && reader.readU8(payload.roomID) &&
        reader.readU8(payload.entryDirection) && reader.readU8(payload.x) &&
        reader.readU8(payload.y) &&
        reader.readU16(payload.movementSequence) && reader.finished() &&
        payload.activityID != 0 && isValidPlayerID(payload.playerID) &&
        payload.roomID < NETWORK_MAX_DUNGEON_ROOMS &&
        payload.entryDirection < 8;
}
