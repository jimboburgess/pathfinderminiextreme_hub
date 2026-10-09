#include "multiplayer_activity.h"

#include <string.h>

bool isNewerMovementSequence(uint16_t candidate, uint16_t current)
{
    return static_cast<int16_t>(candidate - current) > 0;
}

bool findDeterministicMultiplayerSpawn(
    uint8_t width,
    uint8_t height,
    int originX,
    int originY,
    MultiplayerSpawnTilePredicate predicate,
    void* context,
    uint8_t& resultX,
    uint8_t& resultY)
{
    if (width == 0 || height == 0 || predicate == nullptr) return false;
    const int maximumRadius = width + height;
    for (int radius = 0; radius <= maximumRadius; ++radius)
    {
        for (uint8_t y = 0; y < height; ++y)
        {
            for (uint8_t x = 0; x < width; ++x)
            {
                const int deltaX = static_cast<int>(x) - originX;
                const int deltaY = static_cast<int>(y) - originY;
                const int distance = (deltaX < 0 ? -deltaX : deltaX) +
                                     (deltaY < 0 ? -deltaY : deltaY);
                if (distance == radius && predicate(x, y, context))
                {
                    resultX = x;
                    resultY = y;
                    return true;
                }
            }
        }
    }
    return false;
}

void resetSnapshotReceiver(BoundedSnapshotReceiver& receiver)
{
    receiver = BoundedSnapshotReceiver{};
}

bool acceptSnapshotChunk(
    BoundedSnapshotReceiver& receiver,
    const SnapshotChunkPayload& chunk,
    uint32_t expectedActivityID,
    uint8_t expectedRoomID,
    uint16_t expectedEpoch,
    uint8_t expectedChunkCount)
{
    if (chunk.activityID != expectedActivityID ||
        chunk.roomID != expectedRoomID || chunk.snapshotEpoch != expectedEpoch ||
        chunk.totalChunks != expectedChunkCount || expectedChunkCount == 0 ||
        expectedChunkCount > MAX_SNAPSHOT_CHUNKS ||
        chunk.chunkIndex >= expectedChunkCount || chunk.payloadLength == 0 ||
        (chunk.chunkIndex + 1 < chunk.totalChunks &&
         chunk.payloadLength != SNAPSHOT_CHUNK_DATA_SIZE))
        return false;
    const size_t offset = chunk.chunkIndex * SNAPSHOT_CHUNK_DATA_SIZE;
    if (offset + chunk.payloadLength > sizeof(receiver.data)) return false;

    if (!receiver.active)
    {
        receiver.active = true;
        receiver.type = chunk.snapshotType;
        receiver.activityID = chunk.activityID;
        receiver.roomID = chunk.roomID;
        receiver.epoch = chunk.snapshotEpoch;
        receiver.totalChunks = chunk.totalChunks;
    }
    else if (receiver.type != chunk.snapshotType ||
             receiver.activityID != chunk.activityID ||
             receiver.roomID != chunk.roomID || receiver.epoch != chunk.snapshotEpoch ||
             receiver.totalChunks != chunk.totalChunks)
        return false;

    const uint8_t bit = static_cast<uint8_t>(1u << chunk.chunkIndex);
    if ((receiver.receivedMask & bit) != 0)
        return receiver.chunkLengths[chunk.chunkIndex] == chunk.payloadLength &&
               memcmp(receiver.data + offset, chunk.payload,
                      chunk.payloadLength) == 0;
    memcpy(receiver.data + offset, chunk.payload, chunk.payloadLength);
    receiver.chunkLengths[chunk.chunkIndex] = chunk.payloadLength;
    receiver.receivedMask |= bit;
    const uint16_t end = static_cast<uint16_t>(offset + chunk.payloadLength);
    if (end > receiver.dataSize) receiver.dataSize = end;
    return true;
}

bool isDuplicateSnapshotChunk(
    const BoundedSnapshotReceiver& receiver,
    const SnapshotChunkPayload& chunk)
{
    if (!receiver.active || receiver.type != chunk.snapshotType ||
        receiver.activityID != chunk.activityID ||
        receiver.roomID != chunk.roomID || receiver.epoch != chunk.snapshotEpoch ||
        chunk.chunkIndex >= receiver.totalChunks) return false;
    const uint8_t bit = static_cast<uint8_t>(1u << chunk.chunkIndex);
    return (receiver.receivedMask & bit) != 0;
}

bool isSnapshotComplete(const BoundedSnapshotReceiver& receiver)
{
    if (!receiver.active || receiver.totalChunks == 0 ||
        receiver.totalChunks > MAX_SNAPSHOT_CHUNKS) return false;
    const uint8_t expectedMask = static_cast<uint8_t>(
        (1u << receiver.totalChunks) - 1u);
    return (receiver.receivedMask & expectedMask) == expectedMask;
}

bool activityAcksMatch(
    const ActivityAckPayload& expected,
    const ActivityAckPayload& received)
{
    return expected.activityID == received.activityID &&
           expected.snapshotEpoch == received.snapshotEpoch &&
           expected.packetType == received.packetType &&
           expected.snapshotType == received.snapshotType &&
           expected.roomID == received.roomID &&
           expected.itemIndex == received.itemIndex &&
           expected.chunkIndex == received.chunkIndex;
}

bool isDuplicateTravelInvitation(
    uint32_t currentActivityID,
    MultiplayerActivityType currentType,
    bool pending,
    bool accepted,
    bool declined,
    const TravelInvitePayload& incoming)
{
    return incoming.activityID == currentActivityID &&
           incoming.activityType == currentType &&
           (pending || accepted || declined);
}

bool shouldResetActivitySnapshot(
    uint16_t currentEpoch,
    uint16_t incomingEpoch)
{
    return currentEpoch != incomingEpoch;
}
