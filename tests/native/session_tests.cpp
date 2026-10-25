#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "session/game_session.h"
#include "interactive.h"

namespace
{
using namespace pfme;
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)
const Direction DIRECTIONS[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
const char* const NAMES[] = {"north", "east", "south", "west"};

std::string digest(const GameSession& session)
{
    std::ostringstream out;
    out << session.initialized() << ':' << session.seed() << ':'
        << session.playerCount() << ':' << session.connectedCount() << ':';
    const DungeonTopology& graph = session.dungeon();
    out << unsigned(graph.roomCount) << ':' << unsigned(graph.bossRoom) << ':'
        << unsigned(graph.treasureRoom) << ':' << unsigned(graph.riddleRoom);
    for (uint8_t i = 0; i < graph.roomCount; ++i)
    {
        out << ':' << unsigned(graph.rooms[i].type) << ':' << int(graph.rooms[i].dungeonX)
            << ':' << int(graph.rooms[i].dungeonY);
        for (Direction d : DIRECTIONS) out << ':' << unsigned(getRoomNeighbor(graph.rooms[i], d));
    }
    for (PlayerID i = 0; i < MAX_MULTIPLAYER_PLAYERS; ++i)
    {
        SessionPlayer player;
        if (session.player(i, player) == SessionError::Ok)
            out << ':' << unsigned(player.id) << ':' << player.name << ':'
                << player.connected << ':' << unsigned(player.room);
    }
    SessionEvents events;
    session.eventsSince(0, events);
    out << ':' << events.oldest << ':' << events.newest << ':' << events.count;
    for (size_t i = 0; i < events.count; ++i)
    {
        const SessionEvent& event = events.events[i];
        out << ':' << event.sequence << ':' << unsigned(event.type) << ':'
            << unsigned(event.player) << ':' << unsigned(event.fromRoom) << ':'
            << unsigned(event.room) << ':' << event.text;
    }
    return out.str();
}

bool initializationAndAuthority()
{
    GameSession session, other;
    PlayerHandle invalid, handle;
    SessionPlayer player;
    CHECK(session.join("Fighter", handle) == SessionError::NotInitialized);
    CHECK(session.move(invalid, DIR_NORTH) == SessionError::NotInitialized);
    CHECK(session.initialize(12345) == SessionError::Ok);
    CHECK(other.initialize(12345) == SessionError::Ok);
    CHECK(digest(session) == digest(other));
    CHECK(validateDungeonLayoutDetailed(session.dungeon()) == DUNGEON_TOPOLOGY_VALID);
    CHECK(session.dungeon().rooms[0].type == ROOM_ENTRANCE);
    const std::string before = digest(session);
    CHECK(session.player(INVALID_PLAYER_ID, player) == SessionError::InvalidPlayer);
    CHECK(session.move(invalid, DIR_NORTH) == SessionError::InvalidHandle);
    CHECK(session.join(nullptr, handle) == SessionError::InvalidName);
    CHECK(session.join("", handle) == SessionError::InvalidName);
    CHECK(session.join("1234567890123456", handle) == SessionError::InvalidName);
    CHECK(session.join("bad\033name", handle) == SessionError::InvalidName);
    CHECK(session.join("two names", handle) == SessionError::InvalidName);
    CHECK(digest(session) == before);
    CHECK(session.join("Fighter", handle) == SessionError::Ok);
    CHECK(session.player(handle, player) == SessionError::Ok);
    CHECK(player.id == 0 && player.room == 0 && player.connected);
    CHECK(other.move(handle, DIR_NORTH) == SessionError::InvalidHandle);
    const PlayerHandle stale = handle;
    CHECK(session.initialize(12345) == SessionError::Ok);
    CHECK(session.join("New", handle) == SessionError::Ok);
    const std::string after = digest(session);
    CHECK(session.disconnect(stale) == SessionError::InvalidHandle);
    CHECK(digest(session) == after);
    return true;
}

bool navigationAndLifecycle()
{
    GameSession session;
    CHECK(session.initialize(12345) == SessionError::Ok);
    PlayerHandle fighter, wizard, rogue;
    CHECK(session.join("Fighter", fighter) == SessionError::Ok);
    CHECK(session.join("Wizard", wizard) == SessionError::Ok);
    CHECK(session.join("Rogue", rogue) == SessionError::Ok);
    SessionPlayer f, w, r;
    CHECK(session.player(fighter, f) == SessionError::Ok);
    CHECK(session.player(wizard, w) == SessionError::Ok);
    CHECK(session.player(rogue, r) == SessionError::Ok);
    CHECK(f.id != w.id && w.id != r.id && f.id != r.id);
    Direction exit = DIR_NORTH;
    Direction absent = DIR_NORTH;
    uint8_t first = NO_ROOM;
    bool hasAbsent = false;
    for (Direction d : DIRECTIONS)
    {
        const uint8_t destination = getRoomNeighbor(session.dungeon().rooms[0], d);
        if (destination != NO_ROOM) { first = destination; exit = d; }
        else { absent = d; hasAbsent = true; }
    }
    CHECK(first != NO_ROOM && hasAbsent);
    const std::string before = digest(session);
    CHECK(session.move(fighter, absent) == SessionError::NoExit);
    CHECK(session.move(fighter, DIR_NORTHEAST) == SessionError::InvalidDirection);
    CHECK(session.move(fighter, static_cast<Direction>(99)) == SessionError::InvalidDirection);
    CHECK(session.transition(fighter, NO_ROOM) == SessionError::InvalidRoom);
    CHECK(session.transition(fighter, 0) == SessionError::NotAdjacent);
    CHECK(digest(session) == before);
    uint8_t nonadjacent = NO_ROOM;
    for (uint8_t room = 1; room < session.dungeon().roomCount; ++room)
    {
        bool adjacent = false;
        for (Direction d : DIRECTIONS)
            if (getRoomNeighbor(session.dungeon().rooms[0], d) == room) adjacent = true;
        if (!adjacent) { nonadjacent = room; break; }
    }
    CHECK(nonadjacent != NO_ROOM);
    CHECK(session.transition(fighter, nonadjacent) == SessionError::NotAdjacent);
    CHECK(digest(session) == before);
    CHECK(session.move(fighter, exit) == SessionError::Ok);
    CHECK(session.player(fighter, f) == SessionError::Ok && f.room == first);
    CHECK(session.player(wizard, w) == SessionError::Ok && w.room == 0);
    CHECK(session.transition(rogue, first) == SessionError::Ok);
    uint8_t second = NO_ROOM;
    for (Direction d : DIRECTIONS)
    {
        const uint8_t destination = getRoomNeighbor(session.dungeon().rooms[first], d);
        if (destination != NO_ROOM && destination != 0) { second = destination; break; }
    }
    CHECK(second != NO_ROOM);
    CHECK(session.transition(rogue, second) == SessionError::Ok);
    CHECK(session.player(fighter, f) == SessionError::Ok && f.room == first);
    CHECK(session.player(wizard, w) == SessionError::Ok && w.room == 0);
    CHECK(session.player(rogue, r) == SessionError::Ok && r.room == second);
    CHECK(f.room != w.room && w.room != r.room && r.room != f.room);
    SessionEvents history;
    CHECK(session.eventsSince(0, history) == SessionError::Ok && history.count == 7);
    CHECK(history.events[4].type == SessionEventType::Moved);
    CHECK(history.events[4].player == f.id && history.events[4].fromRoom == 0 &&
          history.events[4].room == first);
    CHECK(history.events[6].player == r.id && history.events[6].fromRoom == first &&
          history.events[6].room == second);
    CHECK(session.disconnect(fighter) == SessionError::Ok);
    CHECK(session.playerCount() == 3 && session.connectedCount() == 2);
    const std::string disconnected = digest(session);
    CHECK(session.move(fighter, exit) == SessionError::Disconnected);
    CHECK(session.transition(fighter, 0) == SessionError::Disconnected);
    CHECK(session.disconnect(fighter) == SessionError::Disconnected);
    CHECK(digest(session) == disconnected);
    CHECK(session.reconnect(fighter) == SessionError::Ok);
    CHECK(session.player(fighter, f) == SessionError::Ok && f.room == first);
    const std::string connected = digest(session);
    CHECK(session.reconnect(fighter) == SessionError::AlreadyConnected);
    CHECK(digest(session) == connected);
    CHECK(session.disconnect(rogue) == SessionError::Ok);
    CHECK(session.leave(rogue) == SessionError::Ok);
    CHECK(session.player(r.id, r) == SessionError::InvalidPlayer);
    CHECK(session.reconnect(rogue) == SessionError::InvalidHandle);
    return true;
}

bool capacityAndEvents()
{
    GameSession session;
    CHECK(session.initialize(1) == SessionError::Ok);
    PlayerHandle handles[MAX_MULTIPLAYER_PLAYERS], replacement;
    for (PlayerID i = 0; i < MAX_MULTIPLAYER_PLAYERS; ++i)
    {
        CHECK(session.join("Player", handles[i]) == SessionError::Ok);
        SessionPlayer p;
        CHECK(session.player(handles[i], p) == SessionError::Ok && p.id == i);
    }
    CHECK(session.disconnect(handles[0]) == SessionError::Ok);
    const std::string full = digest(session);
    CHECK(session.join("Extra", replacement) == SessionError::Full);
    CHECK(digest(session) == full); // Disconnected records retain their slots.
    CHECK(session.leave(handles[0]) == SessionError::Ok);
    CHECK(session.join("Replacement", replacement) == SessionError::Ok);
    CHECK(session.reconnect(handles[0]) == SessionError::InvalidHandle);
    SessionPlayer p;
    CHECK(session.player(replacement, p) == SessionError::Ok && p.id == 0);
    SessionEvents events;
    CHECK(session.eventsSince(0, events) == SessionError::Ok);
    CHECK(events.events[0].type == SessionEventType::Status);
    CHECK(events.events[1].type == SessionEventType::Joined);
    CHECK(events.events[5].type == SessionEventType::Disconnected);
    CHECK(events.events[6].type == SessionEventType::Left);
    const uint64_t cursor = session.latestSequence();
    CHECK(session.disconnect(replacement) == SessionError::Ok);
    CHECK(session.reconnect(replacement) == SessionError::Ok);
    CHECK(session.eventsSince(cursor, events) == SessionError::Ok);
    CHECK(events.count == 2 && events.events[0].sequence == cursor + 1);
    CHECK(events.events[1].type == SessionEventType::Reconnected);
    for (size_t i = 0; i < SESSION_EVENT_CAPACITY; ++i)
    {
        CHECK(session.disconnect(replacement) == SessionError::Ok);
        CHECK(session.reconnect(replacement) == SessionError::Ok);
    }
    CHECK(session.eventsSince(0, events) == SessionError::HistoryGap);
    CHECK(events.gap && events.count == SESSION_EVENT_CAPACITY);
    CHECK(events.oldest == session.latestSequence() - SESSION_EVENT_CAPACITY + 1);
    for (size_t i = 0; i < events.count; ++i)
    {
        CHECK(events.events[i].sequence == events.oldest + i);
        CHECK(events.events[i].text[SHARED_MESSAGE_TEXT_SIZE - 1] == '\0');
    }
    CHECK(session.eventsSince(events.oldest - 1, events) == SessionError::Ok);
    CHECK(!events.gap && events.count == SESSION_EVENT_CAPACITY);
    CHECK(session.eventsSince(session.latestSequence(), events) == SessionError::Ok);
    CHECK(events.count == 0 && !events.gap);
    CHECK(session.eventsSince(session.latestSequence() + 1, events) == SessionError::InvalidCursor);
    return true;
}

bool cliBehavior()
{
    std::istringstream eof;
    std::ostringstream out;
    CHECK(runInteractive(eof, out) == 0);
    CHECK(out.str().find("bye\n") != std::string::npos);
    std::ostringstream script;
    script << "join Before\nnew -1\nnew 4294967296\nnew 12345 extra\n"
           << "new 12345\njoin Fighter\njoin Wizard\njoin Rogue\n"
           << "move 0 northeast\nwhere 255\nmove 256 north\n"
           << "events 18446744073709551616\nevents 999\nquit extra\n"
           << "disconnect 0\nmove 0 north\nreconnect 0\nreconnect 0\n"
           << std::string(500, 'x') << "\nplayers\nstatus\nquit\njoin Ignored\n";
    std::istringstream input(script.str());
    out.str(""); out.clear();
    CHECK(runInteractive(input, out) == 0);
    const std::string text = out.str();
    for (const char* expected : {"NotInitialized", "MalformedCommand", "InvalidDirection",
         "InvalidPlayer", "InvalidCursor", "Disconnected", "AlreadyConnected", "LineTooLong",
         "players=3 connected=3", "player=1 name=Wizard room=0", "topology-only"})
        CHECK(text.find(expected) != std::string::npos);
    CHECK(text.find("Ignored") == std::string::npos);
    std::istringstream partial("new 12345\njoin Final");
    out.str(""); out.clear();
    CHECK(runInteractive(partial, out) == 0);
    CHECK(out.str().find("name=Final") != std::string::npos);
    const char embeddedNull[] = "new 12345\0hidden\nstatus\nquit\n";
    std::istringstream binary(std::string(embeddedNull, sizeof(embeddedNull) - 1));
    out.str(""); out.clear();
    CHECK(runInteractive(binary, out) == 0);
    CHECK(out.str().find("control character") != std::string::npos);
    CHECK(out.str().find("initialized=false") != std::string::npos);
    std::istringstream bad;
    bad.setstate(std::ios::badbit);
    CHECK(runInteractive(bad, out) == 1);
    return true;
}

bool threePlayerCli()
{
    GameSession graph;
    CHECK(graph.initialize(12345) == SessionError::Ok);
    size_t firstDirection = 0, secondDirection = 0;
    uint8_t first = NO_ROOM, second = NO_ROOM;
    for (size_t i = 0; i < 4; ++i)
        if (getRoomNeighbor(graph.dungeon().rooms[0], DIRECTIONS[i]) != NO_ROOM)
        { firstDirection = i; first = getRoomNeighbor(graph.dungeon().rooms[0], DIRECTIONS[i]); break; }
    CHECK(first != NO_ROOM);
    for (size_t i = 0; i < 4; ++i)
    {
        const uint8_t destination = getRoomNeighbor(graph.dungeon().rooms[first], DIRECTIONS[i]);
        if (destination != NO_ROOM && destination != 0)
        { second = destination; secondDirection = i; break; }
    }
    CHECK(second != NO_ROOM);
    std::ostringstream commands;
    commands << "new 12345\njoin Fighter\njoin Wizard\njoin Rogue\n"
             << "move 0 " << NAMES[firstDirection] << "\nmove 2 " << NAMES[firstDirection]
             << "\nmove 2 " << NAMES[secondDirection] << "\nplayers\nevents\nquit\n";
    std::istringstream input(commands.str());
    std::ostringstream output;
    CHECK(runInteractive(input, output) == 0);
    CHECK(output.str().find("player=0 name=Fighter room=" + std::to_string(first)) != std::string::npos);
    CHECK(output.str().find("player=1 name=Wizard room=0") != std::string::npos);
    CHECK(output.str().find("player=2 name=Rogue room=" + std::to_string(second)) != std::string::npos);
    CHECK(output.str().find("seq=7 type=moved") != std::string::npos);
    CHECK(output.str().find("error=") == std::string::npos);
    return true;
}
}

int main()
{
    if (!initializationAndAuthority() || !navigationAndLifecycle() ||
        !capacityAndEvents() || !cliBehavior() || !threePlayerCli()) return 1;
    std::cout << "GameSession and interactive CLI checks passed\n";
    return 0;
}
