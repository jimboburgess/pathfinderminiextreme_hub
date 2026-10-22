#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "dungeon/riddles.h"
#include "input/encoderdecoder.h"
#include "multiplayer/multiplayer_activity.h"
#include "multiplayer/network_protocol.h"

namespace
{
bool check(bool condition, const char* expression, const char* file, int line)
{
    if (condition) return true;
    std::cerr << file << ':' << line << ": check failed: " << expression
              << '\n';
    return false;
}

#define CHECK(expression) \
    do \
    { \
        if (!check((expression), #expression, __FILE__, __LINE__)) return false; \
    } while (false)

bool testPacketGoldenVectorAndRoundTrip()
{
    const uint8_t payload[] = {4, 3, 2, 1};
    NetworkPacketHeader header{};
    header.packetType = NetworkPacketType::PING;
    header.senderPlayerID = 2;
    header.flags = NETWORK_FLAG_CONTROL_EVENT;
    header.sequence = 0xABCD;
    header.sessionID = 0x12345678;

    uint8_t encoded[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t encodedSize = 0;
    CHECK(encodePacket(
        header, payload, sizeof(payload), encoded, sizeof(encoded), encodedSize));

    const uint8_t expected[] = {
        0x4D, 0x50, 0x04, 0x60, 0x02, 0x01, 0xCD, 0xAB, 0x04,
        0x00, 0x78, 0x56, 0x34, 0x12, 0x04, 0x03, 0x02, 0x01};
    CHECK(encodedSize == sizeof(expected));
    CHECK(std::memcmp(encoded, expected, sizeof(expected)) == 0);

    DecodedNetworkPacket decoded{};
    CHECK(decodePacket(encoded, encodedSize, decoded));
    CHECK(decoded.header.packetType == NetworkPacketType::PING);
    CHECK(decoded.header.senderPlayerID == 2);
    CHECK(decoded.header.sequence == 0xABCD);
    CHECK(decoded.header.sessionID == 0x12345678);
    CHECK(std::memcmp(decoded.payload, payload, sizeof(payload)) == 0);
    return true;
}

bool testPacketValidationRejectsVersionAndLengthChanges()
{
    NetworkPacketHeader header{};
    header.packetType = NetworkPacketType::HEARTBEAT;
    uint8_t encoded[ESPNOW_MAX_PACKET_SIZE] = {};
    size_t encodedSize = 0;
    CHECK(encodePacket(
        header, nullptr, 0, encoded, sizeof(encoded), encodedSize));

    DecodedNetworkPacket decoded{};
    encoded[2] = static_cast<uint8_t>(MULTIPLAYER_PROTOCOL_VERSION + 1);
    CHECK(decodePacketEnvelope(encoded, encodedSize, decoded));
    CHECK(!decodePacket(encoded, encodedSize, decoded));

    encoded[2] = MULTIPLAYER_PROTOCOL_VERSION;
    encoded[8] = 1;
    CHECK(!decodePacket(encoded, encodedSize, decoded));
    CHECK(!decodePacket(encoded, NETWORK_PACKET_HEADER_SIZE - 1, decoded));
    return true;
}

bool testSharedMessageAndCombatReferencesStayBounded()
{
    SharedGameplayMessagePayload message{};
    message.activityID = 0x1234ABCD;
    message.messageSequence = UINT16_MAX;
    message.category = SharedMessageCategory::COMBAT;
    std::strncpy(
        message.text, "Goblin hits Meriel for 6.", sizeof(message.text) - 1);
    message.textLength = static_cast<uint8_t>(std::strlen(message.text));

    uint8_t bytes[NETWORK_MAX_PAYLOAD_SIZE] = {};
    size_t size = 0;
    CHECK(encodeSharedGameplayMessage(message, bytes, sizeof(bytes), size));
    SharedGameplayMessagePayload decodedMessage{};
    CHECK(decodeSharedGameplayMessage(bytes, size, decodedMessage));
    CHECK(decodedMessage.messageSequence == UINT16_MAX);
    CHECK(std::strcmp(decodedMessage.text, message.text) == 0);

    message.textLength = SHARED_MESSAGE_TEXT_SIZE;
    CHECK(!encodeSharedGameplayMessage(message, bytes, sizeof(bytes), size));

    MultiplayerCombatActionRequestPayload action{};
    action.activityID = 99;
    action.combatID = 12;
    action.actionSequence = 4;
    action.action = MultiplayerCombatActionType::MELEE_ATTACK;
    action.actor.kind = MultiplayerCombatantKind::PLAYER;
    action.actor.playerID = 1;
    action.actor.roomID = 3;
    action.target.kind = MultiplayerCombatantKind::MONSTER;
    action.target.playerID = INVALID_PLAYER_ID;
    action.target.roomID = 3;
    action.target.entityID = 2;
    CHECK(encodeMultiplayerCombatActionRequest(
        action, bytes, sizeof(bytes), size));
    MultiplayerCombatActionRequestPayload decodedAction{};
    CHECK(decodeMultiplayerCombatActionRequest(bytes, size, decodedAction));
    CHECK(decodedAction.actor.playerID == 1);
    CHECK(decodedAction.target.entityID == 2);

    action.actor.playerID = INVALID_PLAYER_ID;
    CHECK(!encodeMultiplayerCombatActionRequest(
        action, bytes, sizeof(bytes), size));
    return true;
}

bool testSnapshotChunksHandleOrderingDuplicatesAndBounds()
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
    std::memset(second.payload, 0x22, second.payloadLength);
    CHECK(acceptSnapshotChunk(receiver, second, 99, 4, 8, 2));
    CHECK(!isSnapshotComplete(receiver));

    SnapshotChunkPayload first = second;
    first.chunkIndex = 0;
    first.payloadLength = SNAPSHOT_CHUNK_DATA_SIZE;
    std::memset(first.payload, 0x11, first.payloadLength);
    CHECK(acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
    CHECK(isSnapshotComplete(receiver));
    CHECK(receiver.dataSize == 210);
    CHECK(acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
    CHECK(isDuplicateSnapshotChunk(receiver, first));

    first.payload[0] = 0x77;
    CHECK(!acceptSnapshotChunk(receiver, first, 99, 4, 8, 2));
    CHECK(!acceptSnapshotChunk(receiver, second, 99, 4, 9, 2));
    return true;
}

bool testMovementSequencesHandleWraparound()
{
    CHECK(isNewerMovementSequence(105, 104));
    CHECK(!isNewerMovementSequence(104, 105));
    CHECK(!isNewerMovementSequence(105, 105));
    CHECK(isNewerMovementSequence(0, UINT16_MAX));
    CHECK(!isNewerMovementSequence(UINT16_MAX, 0));
    return true;
}

struct SpawnTestMap
{
    bool open[25] = {};
    bool occupied[25] = {};
};

bool spawnTestPredicate(int positionX, int positionY, void* context)
{
    const SpawnTestMap* map = static_cast<const SpawnTestMap*>(context);
    const int index = positionY * 5 + positionX;
    return map->open[index] && !map->occupied[index];
}

bool testSpawnSearchIsDeterministicAndSafe()
{
    SpawnTestMap map{};
    map.open[2 * 5 + 2] = true;
    map.open[1 * 5 + 2] = true;
    map.open[2 * 5 + 1] = true;

    uint8_t resultX = 0;
    uint8_t resultY = 0;
    CHECK(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, resultX, resultY));
    CHECK(resultX == 2 && resultY == 2);

    map.occupied[2 * 5 + 2] = true;
    CHECK(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, resultX, resultY));
    CHECK(resultX == 2 && resultY == 1);

    map.occupied[1 * 5 + 2] = true;
    CHECK(findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &map, resultX, resultY));
    CHECK(resultX == 1 && resultY == 2);

    SpawnTestMap fullMap{};
    CHECK(!findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, spawnTestPredicate, &fullMap, resultX, resultY));
    CHECK(!findDeterministicMultiplayerSpawn(
        0, 5, 2, 2, spawnTestPredicate, &map, resultX, resultY));
    CHECK(!findDeterministicMultiplayerSpawn(
        5, 5, 2, 2, nullptr, &map, resultX, resultY));
    return true;
}

bool testRiddleDefinitionsAndShuffleAreStable()
{
    CHECK(RIDDLE_COUNT == 20);
    for (uint8_t index = 0; index < RIDDLE_COUNT; ++index)
    {
        const RiddleDefinition* definition =
            getRiddleDefinition(static_cast<RiddleID>(index));
        CHECK(definition != nullptr);
        CHECK(definition->question != nullptr);
        CHECK(definition->correctAnswerIndex == 0);
        for (const char* answer : definition->answers) CHECK(answer != nullptr);
    }
    CHECK(getRiddleDefinition(RIDDLE_NONE) == nullptr);

    RiddleState state{};
    const uint8_t rolls[3] = {0, 1, 0};
    initializeRiddleState(state, RIDDLE_MOUNTAIN, rolls);
    CHECK(isValidRiddleAnswerOrder(state));
    CHECK(std::strcmp(getDisplayedRiddleAnswer(state, 3), "Mountain") == 0);
    return true;
}

bool testRiddleResultsArePersistentAndRejectInvalidInput()
{
    const uint8_t rolls[3] = {0, 1, 0};
    RiddleState correct{};
    initializeRiddleState(correct, RIDDLE_MOUNTAIN, rolls);
    CHECK(answerRiddle(correct, 3));
    CHECK(correct.result == RIDDLE_ANSWERED_CORRECT);
    CHECK(!answerRiddle(correct, 0));

    RiddleState invalid{};
    initializeRiddleState(invalid, RIDDLE_FIRE, rolls);
    invalid.answerOrder[1] = invalid.answerOrder[0];
    CHECK(!isValidRiddleAnswerOrder(invalid));
    CHECK(!answerRiddle(invalid, 0));
    CHECK(getDisplayedRiddleAnswer(invalid, 0) == nullptr);
    return true;
}

int8_t advanceEncoder(QuadratureDecoderState& decoder, uint8_t phase)
{
    return updateQuadratureDecoder(
        decoder, (phase & 0x02) != 0, (phase & 0x01) != 0);
}

bool testEncoderDetentsAndInversion()
{
    QuadratureDecoderState decoder{};
    resetQuadratureDecoder(decoder, true, true);
    CHECK(advanceEncoder(decoder, 0x02) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x00) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x01) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x03) == QUADRATURE_CLOCKWISE_STEP);
    CHECK(applyEncoderRotationInversion(
              QUADRATURE_CLOCKWISE_STEP, true) ==
          QUADRATURE_COUNTERCLOCKWISE_STEP);

    resetQuadratureDecoder(decoder, true, true);
    CHECK(advanceEncoder(decoder, 0x01) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x00) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x02) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x03) ==
          QUADRATURE_COUNTERCLOCKWISE_STEP);
    return true;
}

bool testEncoderBounceAndInvalidTransitionsDoNotCreateSteps()
{
    QuadratureDecoderState decoder{};
    resetQuadratureDecoder(decoder, true, true);
    CHECK(advanceEncoder(decoder, 0x02) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x03) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x02) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x00) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x01) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x03) == QUADRATURE_CLOCKWISE_STEP);

    resetQuadratureDecoder(decoder, true, true);
    CHECK(advanceEncoder(decoder, 0x02) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x00) == QUADRATURE_NO_STEP);
    CHECK(advanceEncoder(decoder, 0x03) == QUADRATURE_NO_STEP);
    CHECK(decoder.transitionTotal == 0);
    return true;
}

struct TestCase
{
    const char* name;
    bool (*run)();
};
}

int main()
{
    const TestCase tests[] = {
        {"packet golden vector and round trip", testPacketGoldenVectorAndRoundTrip},
        {"packet validation", testPacketValidationRejectsVersionAndLengthChanges},
        {"bounded multiplayer payloads", testSharedMessageAndCombatReferencesStayBounded},
        {"snapshot ordering and bounds", testSnapshotChunksHandleOrderingDuplicatesAndBounds},
        {"movement sequence wraparound", testMovementSequencesHandleWraparound},
        {"deterministic spawn search", testSpawnSearchIsDeterministicAndSafe},
        {"riddle definitions and shuffle", testRiddleDefinitionsAndShuffleAreStable},
        {"riddle answer persistence", testRiddleResultsArePersistentAndRejectInvalidInput},
        {"encoder detents and inversion", testEncoderDetentsAndInversion},
        {"encoder bounce rejection", testEncoderBounceAndInvalidTransitionsDoNotCreateSteps},
    };

    size_t failures = 0;
    for (const TestCase& test : tests)
    {
        std::cout << "[ RUN      ] " << test.name << '\n';
        if (test.run())
            std::cout << "[       OK ] " << test.name << '\n';
        else
        {
            std::cout << "[  FAILED  ] " << test.name << '\n';
            ++failures;
        }
    }

    std::cout << (sizeof(tests) / sizeof(tests[0]) - failures)
              << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
