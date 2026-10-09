#include <Arduino.h>
#include <unity.h>

#include "../../src/multiplayer/network_protocol.cpp"
#include "../../src/multiplayer/multiplayer_snapshot.cpp"

void setUp() {}
void tearDown() {}

void test_packet_header_round_trip_preserves_session_and_sequence()
{
    const uint8_t payload[] = {4, 3, 2, 1};
    NetworkPacketHeader header{};
    header.packetType = NetworkPacketType::PING;
    header.senderPlayerID = 2;
    header.flags = NETWORK_FLAG_CONTROL_EVENT;
    header.sequence = 0xABCD;
    header.sessionID = 0x12345678;

    uint8_t bytes[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t encodedSize = 0;
    TEST_ASSERT_TRUE(encodePacket(
        header, payload, sizeof(payload), bytes, sizeof(bytes), encodedSize));
    TEST_ASSERT_EQUAL_UINT32(
        NETWORK_PACKET_HEADER_SIZE + sizeof(payload), encodedSize);

    DecodedNetworkPacket decoded{};
    TEST_ASSERT_TRUE(decodePacket(bytes, encodedSize, decoded));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkPacketType::PING),
        static_cast<uint8_t>(decoded.header.packetType));
    TEST_ASSERT_EQUAL_UINT8(2, decoded.header.senderPlayerID);
    TEST_ASSERT_EQUAL_UINT16(0xABCD, decoded.header.sequence);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, decoded.header.sessionID);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), decoded.header.payloadSize);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoded.payload, sizeof(payload));
}

void test_protocol_version_is_visible_but_not_accepted()
{
    NetworkPacketHeader header{};
    header.packetType = NetworkPacketType::JOIN_REQUEST;
    uint8_t bytes[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t encodedSize = 0;
    TEST_ASSERT_TRUE(encodePacket(
        header, nullptr, 0, bytes, sizeof(bytes), encodedSize));
    bytes[2] = MULTIPLAYER_PROTOCOL_VERSION + 1;

    DecodedNetworkPacket decoded{};
    TEST_ASSERT_TRUE(decodePacketEnvelope(bytes, encodedSize, decoded));
    TEST_ASSERT_EQUAL_UINT8(
        MULTIPLAYER_PROTOCOL_VERSION + 1,
        decoded.header.protocolVersion);
    TEST_ASSERT_FALSE(decodePacket(bytes, encodedSize, decoded));
}

void test_packet_rejects_incorrect_payload_length()
{
    NetworkPacketHeader header{};
    header.packetType = NetworkPacketType::HEARTBEAT;
    uint8_t bytes[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t encodedSize = 0;
    TEST_ASSERT_TRUE(encodePacket(
        header, nullptr, 0, bytes, sizeof(bytes), encodedSize));

    bytes[8] = 1;
    DecodedNetworkPacket decoded{};
    TEST_ASSERT_FALSE(decodePacket(bytes, encodedSize, decoded));
}

void test_discovery_beacon_round_trip_is_fixed_and_compact()
{
    DiscoveryBeaconPayload source{};
    source.profile.deviceID = 0xCAFEBABE;
    source.profile.level = 7;
    source.profile.characterClass = 2;
    strncpy(source.profile.displayName, "Meriel",
            sizeof(source.profile.displayName) - 1);
    source.availability = MultiplayerAvailability::AVAILABLE;
    source.acceptingPlayers = true;

    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    TEST_ASSERT_TRUE(encodeDiscoveryBeacon(
        source, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(DISCOVERY_BEACON_PAYLOAD_SIZE, size);

    DiscoveryBeaconPayload decoded{};
    TEST_ASSERT_TRUE(decodeDiscoveryBeacon(bytes, size, decoded));
    TEST_ASSERT_EQUAL_HEX32(source.profile.deviceID, decoded.profile.deviceID);
    TEST_ASSERT_EQUAL_UINT8(7, decoded.profile.level);
    TEST_ASSERT_EQUAL_STRING("Meriel", decoded.profile.displayName);
    TEST_ASSERT_TRUE(decoded.acceptingPlayers);
}

void test_movement_payloads_remain_tiny_and_validate_ids()
{
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;

    PlayerMoveRequestPayload request{};
    request.activityID = 0x10203040;
    request.direction = 7;
    request.clientMovementSequence = 123;
    TEST_ASSERT_TRUE(encodePlayerMoveRequest(
        request, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(PLAYER_MOVE_REQUEST_PAYLOAD_SIZE, size);
    PlayerMoveRequestPayload decodedRequest{};
    TEST_ASSERT_TRUE(decodePlayerMoveRequest(bytes, size, decodedRequest));

    PlayerPositionPayload position{};
    position.activityID = 0x10203040;
    position.playerID = 3;
    position.roomID = 11;
    position.x = 14;
    position.y = 13;
    position.facing = 6;
    position.movementSequence = 456;
    TEST_ASSERT_TRUE(encodePlayerPosition(
        position, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(PLAYER_POSITION_PAYLOAD_SIZE, size);
    PlayerPositionPayload decodedPosition{};
    TEST_ASSERT_TRUE(decodePlayerPosition(bytes, size, decodedPosition));
    TEST_ASSERT_EQUAL_UINT8(3, decodedPosition.playerID);

    bytes[0] = INVALID_PLAYER_ID;
    bytes[4] = INVALID_PLAYER_ID;
    TEST_ASSERT_FALSE(decodePlayerPosition(bytes, size, decodedPosition));
}

void test_travel_and_character_payloads_round_trip()
{
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    TravelInvitePayload invite{};
    invite.activityID = 0xAABBCCDD;
    invite.activityType = MultiplayerActivityType::DUNGEON;
    TEST_ASSERT_TRUE(encodeTravelInvite(invite, bytes, sizeof(bytes), size));
    TravelInvitePayload decodedInvite{};
    TEST_ASSERT_TRUE(decodeTravelInvite(bytes, size, decodedInvite));
    TEST_ASSERT_EQUAL_HEX32(invite.activityID, decodedInvite.activityID);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MultiplayerActivityType::DUNGEON),
        static_cast<uint8_t>(decodedInvite.activityType));

    TravelResponsePayload response{};
    response.activityID = invite.activityID;
    response.character.playerID = 2;
    strncpy(response.character.displayName, "Meriel",
            sizeof(response.character.displayName) - 1);
    response.character.characterClass = 2;
    response.character.level = 5;
    response.character.currentHP = 17;
    response.character.maxHP = 24;
    response.character.speed = 6;
    TEST_ASSERT_TRUE(encodeTravelResponse(response, bytes, sizeof(bytes), size));
    TravelResponsePayload decodedResponse{};
    TEST_ASSERT_TRUE(decodeTravelResponse(bytes, size, decodedResponse));
    TEST_ASSERT_EQUAL_UINT8(2, decodedResponse.character.playerID);
    TEST_ASSERT_EQUAL_STRING("Meriel", decodedResponse.character.displayName);
    bytes[4] = INVALID_PLAYER_ID;
    TEST_ASSERT_FALSE(decodeTravelResponse(bytes, size, decodedResponse));
}

void test_activity_and_room_validation_rejects_invalid_rooms()
{
    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    ActivityPreparePayload prepare{};
    prepare.activityID = 44;
    prepare.activityType = MultiplayerActivityType::DUNGEON;
    prepare.roomID = 3;
    prepare.snapshotEpoch = 7;
    prepare.participantMask = 0x03;
    prepare.graphChunkCount = 2;
    prepare.worldChunkCount = 2;
    TEST_ASSERT_TRUE(encodeActivityPrepare(prepare, bytes, sizeof(bytes), size));
    ActivityPreparePayload decoded{};
    TEST_ASSERT_TRUE(decodeActivityPrepare(bytes, size, decoded));
    bytes[5] = NETWORK_MAX_DUNGEON_ROOMS;
    TEST_ASSERT_FALSE(decodeActivityPrepare(bytes, size, decoded));

    PlayerSpawnPayload spawn{};
    spawn.activityID = 44;
    spawn.playerID = 1;
    spawn.roomID = 2;
    spawn.x = 7;
    spawn.y = 10;
    TEST_ASSERT_TRUE(encodePlayerSpawn(spawn, bytes, sizeof(bytes), size));
    bytes[5] = NETWORK_MAX_DUNGEON_ROOMS;
    PlayerSpawnPayload decodedSpawn{};
    TEST_ASSERT_FALSE(decodePlayerSpawn(bytes, size, decodedSpawn));

    RoomTransitionPayload transition{};
    transition.activityID = 44;
    transition.roomID = 2;
    transition.entryDirection = 1;
    transition.snapshotEpoch = 8;
    TEST_ASSERT_TRUE(encodeRoomTransition(
        transition, bytes, sizeof(bytes), size));
    bytes[4] = NETWORK_MAX_DUNGEON_ROOMS;
    RoomTransitionPayload decodedTransition{};
    TEST_ASSERT_FALSE(decodeRoomTransition(
        bytes, size, decodedTransition));
}

void test_snapshot_chunks_accept_ordering_and_identical_duplicates()
{
    BoundedSnapshotReceiver receiver{};
    SnapshotChunkPayload second{};
    second.activityID = 99;
    second.snapshotType = SnapshotType::DUNGEON_ROOM;
    second.roomID = 4;
    second.snapshotEpoch = 8;
    second.chunkIndex = 1;
    second.totalChunks = 2;
    second.payloadLength = 10;
    memset(second.payload, 0x22, second.payloadLength);
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, second, 99, 4, 8, 2));
    TEST_ASSERT_FALSE(isSnapshotComplete(receiver));

    SnapshotChunkPayload first = second;
    first.chunkIndex = 0;
    first.payloadLength = SNAPSHOT_CHUNK_DATA_SIZE;
    memset(first.payload, 0x11, first.payloadLength);
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
    TEST_ASSERT_TRUE(isSnapshotComplete(receiver));
    TEST_ASSERT_EQUAL_UINT16(210, receiver.dataSize);
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
    TEST_ASSERT_TRUE(isDuplicateSnapshotChunk(receiver, first));
    first.payload[0] = 0x77;
    TEST_ASSERT_FALSE(acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
}

void test_activity_ack_round_trip_identifies_exact_snapshot_chunk()
{
    ActivityAckPayload ack{};
    ack.activityID = 0x12345678;
    ack.snapshotEpoch = 9;
    ack.packetType = NetworkPacketType::SNAPSHOT_CHUNK;
    ack.snapshotType = SnapshotType::DUNGEON_ROOM;
    ack.roomID = 3;
    ack.chunkIndex = 1;

    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    TEST_ASSERT_TRUE(encodeActivityAck(ack, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(ACTIVITY_ACK_PAYLOAD_SIZE, size);
    ActivityAckPayload decoded{};
    TEST_ASSERT_TRUE(decodeActivityAck(bytes, size, decoded));
    TEST_ASSERT_TRUE(activityAcksMatch(ack, decoded));
    decoded.chunkIndex = 0;
    TEST_ASSERT_FALSE(activityAcksMatch(ack, decoded));

    ack.packetType = NetworkPacketType::ACTIVITY_START;
    ack.snapshotType = SnapshotType::FOREST_STATE;
    ack.roomID = NETWORK_NO_ROOM;
    ack.chunkIndex = 0;
    TEST_ASSERT_TRUE(encodeActivityAck(ack, bytes, sizeof(bytes), size));
    TEST_ASSERT_TRUE(decodeActivityAck(bytes, size, decoded));
    TEST_ASSERT_TRUE(activityAcksMatch(ack, decoded));
}

void test_lost_ack_resend_is_duplicate_and_can_be_acked_again()
{
    BoundedSnapshotReceiver receiver{};
    SnapshotChunkPayload chunk{};
    chunk.activityID = 88;
    chunk.snapshotType = SnapshotType::DUNGEON_ROOM;
    chunk.roomID = 2;
    chunk.snapshotEpoch = 4;
    chunk.chunkIndex = 0;
    chunk.totalChunks = 1;
    chunk.payloadLength = 12;
    memset(chunk.payload, 0x5A, chunk.payloadLength);
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, chunk, 88, 2, 4, 1));
    TEST_ASSERT_FALSE(isDuplicateSnapshotChunk(
        BoundedSnapshotReceiver{}, chunk));
    TEST_ASSERT_TRUE(isDuplicateSnapshotChunk(receiver, chunk));
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, chunk, 88, 2, 4, 1));
    TEST_ASSERT_TRUE(isSnapshotComplete(receiver));
}

void test_duplicate_invite_and_prepare_epoch_decisions_are_idempotent()
{
    TravelInvitePayload invite{};
    invite.activityID = 123;
    invite.activityType = MultiplayerActivityType::DUNGEON;
    TEST_ASSERT_TRUE(isDuplicateTravelInvitation(
        123, MultiplayerActivityType::DUNGEON,
        true, false, false, invite));
    TEST_ASSERT_TRUE(isDuplicateTravelInvitation(
        123, MultiplayerActivityType::DUNGEON,
        false, true, false, invite));
    TEST_ASSERT_FALSE(isDuplicateTravelInvitation(
        124, MultiplayerActivityType::DUNGEON,
        true, false, false, invite));
    TEST_ASSERT_FALSE(shouldResetActivitySnapshot(7, 7));
    TEST_ASSERT_TRUE(shouldResetActivitySnapshot(7, 8));
}

void test_snapshot_chunks_reject_stale_missing_and_oversized_data()
{
    BoundedSnapshotReceiver receiver{};
    SnapshotChunkPayload chunk{};
    chunk.activityID = 77;
    chunk.snapshotType = SnapshotType::FOREST_STATE;
    chunk.roomID = NETWORK_NO_ROOM;
    chunk.snapshotEpoch = 10;
    chunk.chunkIndex = 0;
    chunk.totalChunks = 2;
    chunk.payloadLength = SNAPSHOT_CHUNK_DATA_SIZE;
    TEST_ASSERT_FALSE(acceptSnapshotChunk(receiver, chunk, 77,
        NETWORK_NO_ROOM, 11, 2));
    TEST_ASSERT_TRUE(acceptSnapshotChunk(receiver, chunk, 77,
        NETWORK_NO_ROOM, 10, 2));
    TEST_ASSERT_FALSE(isSnapshotComplete(receiver));

    resetSnapshotReceiver(receiver);
    chunk.totalChunks = 3;
    chunk.chunkIndex = 2;
    chunk.payloadLength = 1;
    TEST_ASSERT_FALSE(acceptSnapshotChunk(receiver, chunk, 77,
        NETWORK_NO_ROOM, 10, 3));
}

void test_movement_sequence_comparison_handles_stale_and_wraparound()
{
    TEST_ASSERT_TRUE(isNewerMovementSequence(105, 104));
    TEST_ASSERT_FALSE(isNewerMovementSequence(104, 105));
    TEST_ASSERT_FALSE(isNewerMovementSequence(105, 105));
    TEST_ASSERT_TRUE(isNewerMovementSequence(0, UINT16_MAX));
    TEST_ASSERT_FALSE(isNewerMovementSequence(UINT16_MAX, 0));
}

struct SpawnTestMap
{
    bool open[25] = {};
    bool occupied[25] = {};
};

bool spawnTestPredicate(int x, int y, void* context)
{
    const SpawnTestMap* map = static_cast<const SpawnTestMap*>(context);
    const int index = y * 5 + x;
    return map->open[index] && !map->occupied[index];
}

void test_spawn_search_is_deterministic_and_avoids_occupied_tiles()
{
    SpawnTestMap map{};
    map.open[2 * 5 + 2] = true;
    map.open[1 * 5 + 2] = true;
    map.open[2 * 5 + 1] = true;

    uint8_t x = 0;
    uint8_t y = 0;
    TEST_ASSERT_TRUE(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, x, y));
    TEST_ASSERT_EQUAL_UINT8(2, x);
    TEST_ASSERT_EQUAL_UINT8(2, y);

    map.occupied[2 * 5 + 2] = true;
    TEST_ASSERT_TRUE(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, x, y));
    TEST_ASSERT_EQUAL_UINT8(2, x);
    TEST_ASSERT_EQUAL_UINT8(1, y);

    map.occupied[1 * 5 + 2] = true;
    TEST_ASSERT_TRUE(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, x, y));
    TEST_ASSERT_EQUAL_UINT8(1, x);
    TEST_ASSERT_EQUAL_UINT8(2, y);
}

void test_spawn_search_rejects_invalid_inputs_and_full_maps()
{
    SpawnTestMap map{};
    uint8_t x = 0;
    uint8_t y = 0;
    TEST_ASSERT_FALSE(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, x, y));
    TEST_ASSERT_FALSE(findDeterministicMultiplayerSpawn(
        0, 5, 2, 2, spawnTestPredicate, &map, x, y));
    TEST_ASSERT_FALSE(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, nullptr, &map, x, y));
}

void setup()
{
    delay(1000);
    UNITY_BEGIN();
    RUN_TEST(test_packet_header_round_trip_preserves_session_and_sequence);
    RUN_TEST(test_protocol_version_is_visible_but_not_accepted);
    RUN_TEST(test_packet_rejects_incorrect_payload_length);
    RUN_TEST(test_discovery_beacon_round_trip_is_fixed_and_compact);
    RUN_TEST(test_movement_payloads_remain_tiny_and_validate_ids);
    RUN_TEST(test_travel_and_character_payloads_round_trip);
    RUN_TEST(test_activity_and_room_validation_rejects_invalid_rooms);
    RUN_TEST(test_snapshot_chunks_accept_ordering_and_identical_duplicates);
    RUN_TEST(test_snapshot_chunks_reject_stale_missing_and_oversized_data);
    RUN_TEST(test_activity_ack_round_trip_identifies_exact_snapshot_chunk);
    RUN_TEST(test_lost_ack_resend_is_duplicate_and_can_be_acked_again);
    RUN_TEST(test_duplicate_invite_and_prepare_epoch_decisions_are_idempotent);
    RUN_TEST(test_movement_sequence_comparison_handles_stale_and_wraparound);
    RUN_TEST(test_spawn_search_is_deterministic_and_avoids_occupied_tiles);
    RUN_TEST(test_spawn_search_rejects_invalid_inputs_and_full_maps);
    UNITY_END();
}

void loop() {}
