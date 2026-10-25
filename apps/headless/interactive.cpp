#include "interactive.h"

#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

#include "session/game_session.h"

namespace
{
using namespace pfme;
const Direction DIRECTIONS[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
const char* const DIRECTION_NAMES[] = {"north", "east", "south", "west"};

bool number(const std::string& text, uint64_t maximum, uint64_t& result)
{
    if (text.empty()) return false;
    result = 0;
    for (char c : text)
    {
        if (c < '0' || c > '9') return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (result > maximum / 10 ||
            (result == maximum / 10 && digit > maximum % 10)) return false;
        result = result * 10 + digit;
    }
    return true;
}

void showPlayer(std::ostream& out, const SessionPlayer& player)
{
    out << "player=" << static_cast<unsigned>(player.id)
        << " name=" << player.name << " room=" << static_cast<unsigned>(player.room)
        << " connected=" << (player.connected ? "true" : "false") << '\n';
}

void error(std::ostream& out, SessionError result)
{
    out << "error=" << sessionErrorName(result) << '\n';
}
}

int runInteractive(std::istream& input, std::ostream& output)
{
    using namespace pfme;
    GameSession session;
    PlayerHandle handles[MAX_MULTIPLAYER_PLAYERS];
    output << "Pathfinder interactive: trusted local administrator.\n"
           << "Player IDs are not authentication credentials.\n"
           << "Topology-only navigation: no tiles, locks, puzzle/boss completion, or combat.\n"
           << "Use new <seed> to create a session; help lists commands.\n";
    for (;;)
    {
        output << "> " << std::flush;
        char line[161] = {};
        input.getline(line, sizeof(line));
        if (input.bad()) return 1;
        if (input.fail() && !input.eof())
        {
            input.clear();
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            output << "error=LineTooLong maximum=160\n";
            continue;
        }
        if (input.gcount() == 0 && input.eof()) break;
        const size_t length = static_cast<size_t>(input.gcount()) - (input.eof() ? 0u : 1u);
        bool malformed = false;
        for (size_t i = 0; i < length; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(line[i]);
            if ((c < 32 && c != '\t' && c != '\r') || c == 127) malformed = true;
        }
        if (malformed)
        { output << "error=MalformedCommand (control character)\n"; continue; }
        std::istringstream parser(line);
        std::string words[4];
        size_t count = 0;
        while (count < 4 && parser >> words[count]) ++count;
        if (count == 0) { if (input.eof()) break; continue; }
        const std::string& command = words[0];
        uint64_t value = 0;
        if (command == "quit" && count == 1) break;
        if (command == "help" && count == 1)
        {
            output << "help | new <seed> | join <name> | players | map | where <id>\n"
                   << "move <id> <north|east|south|west> | disconnect <id>\n"
                   << "reconnect <id> | leave <id> | events [since] | status | quit\n"
                   << "Names: 1-15 printable non-space ASCII characters.\n";
        }
        else if (command == "new" && count == 2 && number(words[1], UINT32_MAX, value))
        {
            const SessionError result = session.initialize(static_cast<uint32_t>(value));
            if (result != SessionError::Ok) error(output, result);
            else
            {
                for (PlayerHandle& handle : handles) handle = PlayerHandle{};
                output << "new seed=" << value << " rooms="
                       << static_cast<unsigned>(session.dungeon().roomCount)
                       << " entrance=0 event_cursor_reset=true\n";
            }
        }
        else if (command == "join" && count == 2)
        {
            PlayerHandle handle;
            const SessionError result = session.join(words[1].c_str(), handle);
            if (result != SessionError::Ok) error(output, result);
            else
            {
                SessionPlayer player;
                session.player(handle, player);
                handles[player.id] = handle;
                output << "joined "; showPlayer(output, player);
            }
        }
        else if (command == "status" && count == 1)
        {
            output << "initialized=" << (session.initialized() ? "true" : "false")
                   << " seed=" << session.seed() << " players=" << session.playerCount()
                   << " connected=" << session.connectedCount()
                   << " latest_event=" << session.latestSequence()
                   << " navigation=topology-only\n";
        }
        else if ((command == "players" || command == "map") && count == 1)
        {
            if (!session.initialized()) { error(output, SessionError::NotInitialized); continue; }
            if (command == "players")
            {
                for (PlayerID id = 0; id < MAX_MULTIPLAYER_PLAYERS; ++id)
                {
                    SessionPlayer player;
                    if (session.player(id, player) == SessionError::Ok) showPlayer(output, player);
                }
            }
            else
            {
                const DungeonTopology& dungeon = session.dungeon();
                for (uint8_t room = 0; room < dungeon.roomCount; ++room)
                {
                    output << "room=" << static_cast<unsigned>(room);
                    for (size_t d = 0; d < 4; ++d)
                    {
                        output << ' ' << DIRECTION_NAMES[d] << '=';
                        const uint8_t neighbor = getRoomNeighbor(dungeon.rooms[room], DIRECTIONS[d]);
                        if (neighbor == NO_ROOM) output << '-';
                        else output << static_cast<unsigned>(neighbor);
                    }
                    output << '\n';
                }
            }
        }
        else if (command == "events" && (count == 1 ||
                 (count == 2 && number(words[1], UINT64_MAX, value))))
        {
            SessionEvents events;
            const SessionError result = session.eventsSince(value, events);
            if (result != SessionError::Ok && result != SessionError::HistoryGap)
            { error(output, result); continue; }
            output << "events oldest=" << events.oldest << " newest=" << events.newest
                   << " gap=" << (events.gap ? "true" : "false") << '\n';
            if (events.gap) output << "history_gap: refresh full session state before resuming events.\n";
            for (size_t i = 0; i < events.count; ++i)
            {
                const SessionEvent& event = events.events[i];
                output << "seq=" << event.sequence << " type=" << sessionEventName(event.type)
                       << " player=" << static_cast<unsigned>(event.player)
                       << " from=" << static_cast<unsigned>(event.fromRoom)
                       << " room=" << static_cast<unsigned>(event.room)
                       << " text=" << event.text << '\n';
            }
        }
        else if ((command == "where" || command == "disconnect" ||
                  command == "reconnect" || command == "leave" || command == "move") &&
                 count == (command == "move" ? 3u : 2u) &&
                 number(words[1], UINT8_MAX, value))
        {
            const PlayerID id = static_cast<PlayerID>(value);
            SessionPlayer player;
            SessionError result = session.player(id, player);
            if (result != SessionError::Ok) { error(output, result); continue; }
            if (command == "where") { showPlayer(output, player); continue; }
            if (command == "disconnect") result = session.disconnect(handles[id]);
            else if (command == "reconnect") result = session.reconnect(handles[id]);
            else if (command == "leave") result = session.leave(handles[id]);
            else
            {
                result = SessionError::InvalidDirection;
                for (size_t d = 0; d < 4; ++d)
                    if (words[2] == DIRECTION_NAMES[d])
                    { result = session.move(handles[id], DIRECTIONS[d]); break; }
            }
            if (result != SessionError::Ok) error(output, result);
            else if (command == "leave")
            { handles[id] = PlayerHandle{}; output << "left player=" << value << '\n'; }
            else
            {
                session.player(id, player);
                output << command << " ok "; showPlayer(output, player);
            }
        }
        else output << "error=MalformedCommand (help lists syntax)\n";
        if (input.eof()) break;
    }
    output << "bye\n";
    return 0;
}
