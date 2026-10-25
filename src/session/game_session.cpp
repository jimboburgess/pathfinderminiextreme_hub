#include "game_session.h"

#include <stdio.h>
#include <string.h>

namespace pfme
{
namespace
{
const Direction CARDINAL[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
bool cardinal(Direction direction)
{
    for (Direction value : CARDINAL) if (direction == value) return true;
    return false;
}
}

const char* sessionErrorName(SessionError error)
{
    switch (error)
    {
#define PFME_ERROR(value) case SessionError::value: return #value
        PFME_ERROR(Ok); PFME_ERROR(NotInitialized); PFME_ERROR(GenerationFailed);
        PFME_ERROR(InvalidPlayer); PFME_ERROR(InvalidHandle);
        PFME_ERROR(Disconnected); PFME_ERROR(AlreadyConnected); PFME_ERROR(Full);
        PFME_ERROR(InvalidName); PFME_ERROR(InvalidDirection);
        PFME_ERROR(InvalidRoom); PFME_ERROR(NoExit); PFME_ERROR(NotAdjacent);
        PFME_ERROR(InvalidGraph); PFME_ERROR(HistoryGap);
        PFME_ERROR(InvalidCursor); PFME_ERROR(Exhausted);
#undef PFME_ERROR
    }
    return "Unknown";
}

const char* sessionEventName(SessionEventType type)
{
    switch (type)
    {
        case SessionEventType::Status: return "status";
        case SessionEventType::Joined: return "joined";
        case SessionEventType::Disconnected: return "disconnected";
        case SessionEventType::Reconnected: return "reconnected";
        case SessionEventType::Moved: return "moved";
        case SessionEventType::Left: return "left";
    }
    return "unknown";
}

SessionError GameSession::initialize(uint32_t seed)
{
    DungeonTopology candidate;
    DungeonGenerationReport report;
    SeededDungeonRandom random(seed);
    if (!generateDungeonLayout(candidate, random, report) ||
        validateDungeonLayoutDetailed(candidate) != DUNGEON_TOPOLOGY_VALID)
        return SessionError::GenerationFailed;
    dungeon_ = candidate;
    for (Slot& slot : players_) slot = Slot{};
    eventStart_ = eventCount_ = 0;
    nextSequence_ = 1;
    seed_ = seed;
    initialized_ = true;
    emit(SessionEventType::Status, INVALID_PLAYER_ID, NO_ROOM, NO_ROOM,
         "Topology session created.");
    return SessionError::Ok;
}

bool GameSession::canEmit() const { return nextSequence_ < UINT64_MAX; }

void GameSession::emit(SessionEventType type, PlayerID id, uint8_t from,
                       uint8_t room, const char* text)
{
    const size_t index = (eventStart_ + eventCount_) % SESSION_EVENT_CAPACITY;
    SessionEvent event;
    event.sequence = nextSequence_++;
    event.type = type;
    event.player = id;
    event.fromRoom = from;
    event.room = room;
    snprintf(event.text, sizeof(event.text), "%s", text);
    events_[index] = event;
    if (eventCount_ < SESSION_EVENT_CAPACITY) ++eventCount_;
    else eventStart_ = (eventStart_ + 1) % SESSION_EVENT_CAPACITY;
}

SessionError GameSession::join(const char* name, PlayerHandle& handle)
{
    if (!initialized_) return SessionError::NotInitialized;
    if (name == nullptr || name[0] == '\0') return SessionError::InvalidName;
    size_t length = 0;
    while (length < NETWORK_PLAYER_NAME_SIZE && name[length] != '\0')
    {
        if (name[length] < 33 || name[length] > 126)
            return SessionError::InvalidName;
        ++length;
    }
    if (length >= NETWORK_PLAYER_NAME_SIZE) return SessionError::InvalidName;
    PlayerID id = INVALID_PLAYER_ID;
    for (PlayerID i = 0; i < MAX_MULTIPLAYER_PLAYERS; ++i)
        if (!players_[i].occupied) { id = i; break; }
    if (!isValidPlayerID(id)) return SessionError::Full;
    if (!canEmit() || nextGeneration_ == UINT64_MAX) return SessionError::Exhausted;
    Slot& slot = players_[id];
    slot = Slot{};
    slot.occupied = true;
    slot.generation = nextGeneration_++;
    slot.player.id = id;
    slot.player.connected = true;
    slot.player.room = 0; // Shared generator's entrance index.
    memcpy(slot.player.name, name, length + 1);
    handle.owner_ = this;
    handle.id_ = id;
    handle.generation_ = slot.generation;
    emit(SessionEventType::Joined, id, NO_ROOM, 0, slot.player.name);
    return SessionError::Ok;
}

SessionError GameSession::validate(const PlayerHandle& handle) const
{
    if (!initialized_) return SessionError::NotInitialized;
    if (handle.owner_ != this || !isValidPlayerID(handle.id_))
        return SessionError::InvalidHandle;
    const Slot& slot = players_[handle.id_];
    if (!slot.occupied || slot.generation != handle.generation_)
        return SessionError::InvalidHandle;
    return SessionError::Ok;
}

SessionError GameSession::player(PlayerID id, SessionPlayer& result) const
{
    if (!initialized_) return SessionError::NotInitialized;
    if (!isValidPlayerID(id) || !players_[id].occupied)
        return SessionError::InvalidPlayer;
    result = players_[id].player;
    return SessionError::Ok;
}

SessionError GameSession::player(const PlayerHandle& handle, SessionPlayer& result) const
{
    const SessionError error = validate(handle);
    return error == SessionError::Ok ? player(handle.id_, result) : error;
}

SessionError GameSession::disconnect(const PlayerHandle& handle)
{
    const SessionError error = validate(handle);
    if (error != SessionError::Ok) return error;
    SessionPlayer& record = players_[handle.id_].player;
    if (!record.connected) return SessionError::Disconnected;
    if (!canEmit()) return SessionError::Exhausted;
    record.connected = false;
    emit(SessionEventType::Disconnected, record.id, record.room, record.room,
         record.name);
    return SessionError::Ok;
}

SessionError GameSession::reconnect(const PlayerHandle& handle)
{
    const SessionError error = validate(handle);
    if (error != SessionError::Ok) return error;
    SessionPlayer& record = players_[handle.id_].player;
    if (record.connected) return SessionError::AlreadyConnected;
    if (!canEmit()) return SessionError::Exhausted;
    record.connected = true;
    emit(SessionEventType::Reconnected, record.id, record.room, record.room,
         record.name);
    return SessionError::Ok;
}

SessionError GameSession::leave(const PlayerHandle& handle)
{
    const SessionError error = validate(handle);
    if (error != SessionError::Ok) return error;
    if (!canEmit()) return SessionError::Exhausted;
    Slot& slot = players_[handle.id_];
    emit(SessionEventType::Left, slot.player.id, slot.player.room, NO_ROOM,
         slot.player.name);
    slot = Slot{};
    return SessionError::Ok;
}

SessionError GameSession::move(const PlayerHandle& handle, Direction direction)
{
    const SessionError error = validate(handle);
    if (error != SessionError::Ok) return error;
    const SessionPlayer& record = players_[handle.id_].player;
    if (!record.connected) return SessionError::Disconnected;
    if (!cardinal(direction)) return SessionError::InvalidDirection;
    if (record.room >= dungeon_.roomCount) return SessionError::InvalidGraph;
    const uint8_t destination = getRoomNeighbor(dungeon_.rooms[record.room], direction);
    if (destination == NO_ROOM) return SessionError::NoExit;
    return transition(handle, destination);
}

SessionError GameSession::transition(const PlayerHandle& handle, uint8_t destination)
{
    const SessionError error = validate(handle);
    if (error != SessionError::Ok) return error;
    SessionPlayer& record = players_[handle.id_].player;
    if (!record.connected) return SessionError::Disconnected;
    if (destination >= dungeon_.roomCount) return SessionError::InvalidRoom;
    if (validateDungeonLayoutDetailed(dungeon_) != DUNGEON_TOPOLOGY_VALID ||
        record.room >= dungeon_.roomCount) return SessionError::InvalidGraph;
    bool adjacent = false;
    for (Direction direction : CARDINAL)
        if (getRoomNeighbor(dungeon_.rooms[record.room], direction) == destination)
            adjacent = true;
    if (!adjacent) return SessionError::NotAdjacent;
    if (!canEmit()) return SessionError::Exhausted;
    const uint8_t from = record.room;
    record.room = destination;
    emit(SessionEventType::Moved, record.id, from, destination, record.name);
    return SessionError::Ok;
}

SessionError GameSession::eventsSince(uint64_t sequence, SessionEvents& result) const
{
    result = SessionEvents{};
    if (!initialized_) return SessionError::NotInitialized;
    result.oldest = events_[eventStart_].sequence;
    result.newest = latestSequence();
    if (sequence > result.newest) return SessionError::InvalidCursor;
    result.gap = sequence < result.oldest - 1;
    for (size_t i = 0; i < eventCount_; ++i)
    {
        const SessionEvent& event = events_[(eventStart_ + i) % SESSION_EVENT_CAPACITY];
        if (event.sequence > sequence) result.events[result.count++] = event;
    }
    return result.gap ? SessionError::HistoryGap : SessionError::Ok;
}

size_t GameSession::playerCount() const
{
    size_t count = 0;
    for (const Slot& slot : players_) if (slot.occupied) ++count;
    return count;
}

size_t GameSession::connectedCount() const
{
    size_t count = 0;
    for (const Slot& slot : players_)
        if (slot.occupied && slot.player.connected) ++count;
    return count;
}
}
