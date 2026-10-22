#ifndef PATHFINDERMINIEXTREME_025_MULTIPLAYER_ACTIVITY_H
#define PATHFINDERMINIEXTREME_025_MULTIPLAYER_ACTIVITY_H

#include <stddef.h>
#include <stdint.h>

#include "network_protocol.h"

struct Entity;

constexpr size_t MULTIPLAYER_SNAPSHOT_BUFFER_SIZE = 384;

struct BoundedSnapshotReceiver
{
    bool active = false;
    SnapshotType type = SnapshotType::FOREST_STATE;
    uint32_t activityID = 0;
    uint8_t roomID = NETWORK_NO_ROOM;
    uint16_t epoch = 0;
    uint8_t totalChunks = 0;
    uint8_t receivedMask = 0;
    uint8_t chunkLengths[MAX_SNAPSHOT_CHUNKS] = {};
    uint16_t dataSize = 0;
    uint8_t data[MULTIPLAYER_SNAPSHOT_BUFFER_SIZE] = {};
};

bool isNewerMovementSequence(uint16_t candidate, uint16_t current);
using MultiplayerSpawnTilePredicate = bool (*)(int x, int y, void* context);
bool findDeterministicMultiplayerSpawn(
    uint8_t width,
    uint8_t height,
    int originX,
    int originY,
    MultiplayerSpawnTilePredicate predicate,
    void* context,
    uint8_t& resultX,
    uint8_t& resultY);
void resetSnapshotReceiver(BoundedSnapshotReceiver& receiver);
bool acceptSnapshotChunk(
    BoundedSnapshotReceiver& receiver,
    const SnapshotChunkPayload& chunk,
    uint32_t expectedActivityID,
    uint8_t expectedRoomID,
    uint16_t expectedEpoch,
    uint8_t expectedChunkCount);
bool isDuplicateSnapshotChunk(
    const BoundedSnapshotReceiver& receiver,
    const SnapshotChunkPayload& chunk);
bool isSnapshotComplete(const BoundedSnapshotReceiver& receiver);
bool activityAcksMatch(
    const ActivityAckPayload& expected,
    const ActivityAckPayload& received);
bool isDuplicateTravelInvitation(
    uint32_t currentActivityID,
    MultiplayerActivityType currentType,
    bool pending,
    bool accepted,
    bool declined,
    const TravelInvitePayload& incoming);
bool shouldResetActivitySnapshot(
    uint16_t currentEpoch,
    uint16_t incomingEpoch);

void initializeMultiplayerActivity();
void updateMultiplayerActivity(uint32_t now);

bool requestMultiplayerTravel(MultiplayerActivityType activityType);
bool hasPendingMultiplayerTravelInvite();
MultiplayerActivityType getPendingMultiplayerTravelType();
const char* getPendingMultiplayerTravelHostName();
void acceptPendingMultiplayerTravel();
void declinePendingMultiplayerTravel();

bool isMultiplayerExplorationActive();
bool isMultiplayerActivityLoading();
bool isLocalActivityParticipant();
MultiplayerActivityType getMultiplayerActivityType();
MultiplayerMemberLocation getMultiplayerMemberLocation(PlayerID playerID);
const char* multiplayerMemberLocationName(MultiplayerMemberLocation location);

bool handleMultiplayerLocalMove(uint8_t direction, bool& moved);
bool publishSharedGameplayMessage(
    const char* message,
    SharedMessageCategory category = SharedMessageCategory::GAMEPLAY);
uint8_t getMultiplayerPlayerRoom(PlayerID playerID);
uint16_t getMultiplayerRoomRevision(uint8_t roomID);
bool isMultiplayerCombatActive();
bool startMultiplayerCombatFromCurrentRoom();
void updateMultiplayerCombat(uint32_t now);
Entity* getVisibleMultiplayerCurrentCombatant();
bool isLocalPlayersMultiplayerTurn();
bool requestMultiplayerCombatMove(uint8_t direction);
bool requestMultiplayerCombatAttack(bool ranged, const Entity* target);
bool requestMultiplayerCombatAbility(
    uint16_t abilityID,
    const Entity* target,
    int8_t targetX,
    int8_t targetY,
    uint8_t direction,
    uint8_t damageType,
    uint8_t flags = 0);
bool requestMultiplayerCombatEndTurn();
bool requestMultiplayerFullDefense();
bool isCurrentMultiplayerCombatant(const Entity& entity);
bool isMultiplayerCombatParticipant(const Entity& entity);
void leaveMultiplayerActivity();

#endif
