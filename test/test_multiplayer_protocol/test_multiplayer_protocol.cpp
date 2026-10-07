#include <Arduino.h>
#include <unity.h>

#include "../../src/multiplayer/network_protocol.cpp"

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
    request.direction = 7;
    request.clientMovementSequence = 123;
    TEST_ASSERT_TRUE(encodePlayerMoveRequest(
        request, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(PLAYER_MOVE_REQUEST_PAYLOAD_SIZE, size);
    PlayerMoveRequestPayload decodedRequest{};
    TEST_ASSERT_TRUE(decodePlayerMoveRequest(bytes, size, decodedRequest));

    PlayerPositionPayload position{};
    position.playerID = 3;
    position.roomID = 11;
    position.x = 14;
    position.y = 13;
    position.movementSequence = 456;
    TEST_ASSERT_TRUE(encodePlayerPosition(
        position, bytes, sizeof(bytes), size));
    TEST_ASSERT_EQUAL_UINT32(PLAYER_POSITION_PAYLOAD_SIZE, size);
    PlayerPositionPayload decodedPosition{};
    TEST_ASSERT_TRUE(decodePlayerPosition(bytes, size, decodedPosition));
    TEST_ASSERT_EQUAL_UINT8(3, decodedPosition.playerID);

    bytes[0] = INVALID_PLAYER_ID;
    TEST_ASSERT_FALSE(decodePlayerPosition(bytes, size, decodedPosition));
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
    UNITY_END();
}

void loop() {}
