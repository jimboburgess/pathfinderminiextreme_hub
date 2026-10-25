#ifndef PFME_GAME_SESSION_H
#define PFME_GAME_SESSION_H

#include "dungeon/dungeontopology.h"
#include "multiplayer/network_protocol.h"

namespace pfme
{
constexpr size_t SESSION_EVENT_CAPACITY = 32;

enum class SessionError : uint8_t
{
    Ok, NotInitialized, GenerationFailed, InvalidPlayer, InvalidHandle,
    Disconnected, AlreadyConnected, Full, InvalidName, InvalidDirection,
    InvalidRoom, NoExit, NotAdjacent, InvalidGraph, HistoryGap, InvalidCursor,
    Exhausted
};
const char* sessionErrorName(SessionError error);

enum class SessionEventType : uint8_t
{
    Status, Joined, Disconnected, Reconnected, Moved, Left
};
const char* sessionEventName(SessionEventType type);

class GameSession;

// In-process authority capability, not a serializable authentication token.
// Only join grants a handle. A future authenticated adapter retains it privately.
class PlayerHandle
{
public:
    PlayerHandle() = default;
private:
    friend class GameSession;
    const GameSession* owner_ = nullptr;
    PlayerID id_ = INVALID_PLAYER_ID;
    uint64_t generation_ = 0;
};

struct SessionPlayer
{
    PlayerID id = INVALID_PLAYER_ID;
    bool connected = false;
    uint8_t room = NO_ROOM;
    char name[NETWORK_PLAYER_NAME_SIZE] = {};
};

struct SessionEvent
{
    uint64_t sequence = 0;
    SessionEventType type = SessionEventType::Status;
    PlayerID player = INVALID_PLAYER_ID;
    uint8_t fromRoom = NO_ROOM;
    uint8_t room = NO_ROOM;
    char text[SHARED_MESSAGE_TEXT_SIZE] = {};
};

struct SessionEvents
{
    SessionEvent events[SESSION_EVENT_CAPACITY] = {};
    size_t count = 0;
    uint64_t oldest = 0;
    uint64_t newest = 0;
    bool gap = false;
};

class GameSession
{
public:
    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    // Explicit trusted administration: starts a new session, invalidating handles
    // and event cursors. No network adapter should expose this to unauthenticated input.
    SessionError initialize(uint32_t seed);
    SessionError join(const char* name, PlayerHandle& handle);
    SessionError disconnect(const PlayerHandle& handle);
    SessionError reconnect(const PlayerHandle& handle);
    SessionError leave(const PlayerHandle& handle);
    SessionError move(const PlayerHandle& handle, Direction direction);
    SessionError transition(const PlayerHandle& handle, uint8_t destination);

    SessionError player(PlayerID id, SessionPlayer& result) const;
    SessionError player(const PlayerHandle& handle, SessionPlayer& result) const;
    SessionError eventsSince(uint64_t sequence, SessionEvents& result) const;
    const DungeonTopology& dungeon() const { return dungeon_; }
    bool initialized() const { return initialized_; }
    uint32_t seed() const { return seed_; }
    size_t playerCount() const;
    size_t connectedCount() const;
    uint64_t latestSequence() const { return nextSequence_ - 1; }

private:
    struct Slot
    {
        SessionPlayer player{};
        uint64_t generation = 0;
        bool occupied = false;
    };
    SessionError validate(const PlayerHandle& handle) const;
    bool canEmit() const;
    void emit(SessionEventType type, PlayerID id, uint8_t from,
              uint8_t room, const char* text);

    DungeonTopology dungeon_{};
    Slot players_[MAX_MULTIPLAYER_PLAYERS] = {};
    SessionEvent events_[SESSION_EVENT_CAPACITY] = {};
    size_t eventStart_ = 0;
    size_t eventCount_ = 0;
    uint64_t nextSequence_ = 1;
    // Never reset on initialize; reused protocol slots cannot revive stale handles.
    uint64_t nextGeneration_ = 1;
    uint32_t seed_ = 0;
    bool initialized_ = false;
};
}
#endif
