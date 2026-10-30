#include "interactive.h"

#include <istream>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <string>

#include "session/game_session.h"
#include "session/party_registry.h"

namespace
{
using namespace pfme;
const Direction DIRECTIONS[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
const char* const DIRECTION_NAMES[] = {"north", "east", "south", "west"};

class CliPartyEntropy : public PartyEntropy
{
public:
    bool next(uint64_t& value) override
    {
        value = 0;
        for (unsigned index = 0; index < 4; ++index)
            value = (value << 16) ^ static_cast<uint64_t>(random_() & 0xffffu);
        return true;
    }
private:
    std::random_device random_;
};

struct CliPartyRecord
{
    char code[PARTY_JOIN_CODE_SIZE] = {};
    PartyHandle party{};
    PartyMembership members[MAX_MULTIPLAYER_PLAYERS] = {};
    bool active = false;
    bool used = false;
};

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

void partyError(std::ostream& out, PartyError result)
{
    out << "party-error=" << partyErrorName(result) << '\n';
}

bool characterClass(const std::string& text, CharacterClass& result)
{
    if (text == "fighter") result = CLASS_FIGHTER;
    else if (text == "rogue") result = CLASS_ROGUE;
    else if (text == "wizard") result = CLASS_WIZARD;
    else if (text == "cleric") result = CLASS_CLERIC;
    else return false;
    return true;
}

bool partyProfile(const std::string words[], size_t count, size_t nameIndex,
                  PartyPlayerProfile& result)
{
    if (count != nameIndex + 1 && count != nameIndex + 3) return false;
    if (words[nameIndex].empty() || words[nameIndex].size() >= NETWORK_PLAYER_NAME_SIZE)
        return false;
    result = PartyPlayerProfile{};
    for (size_t index = 0; index <= words[nameIndex].size(); ++index)
        result.displayName[index] = words[nameIndex][index];
    if (count == nameIndex + 1) return true;
    uint64_t level = 0;
    return characterClass(words[nameIndex + 1], result.characterClass) &&
           number(words[nameIndex + 2], MAX_CHARACTER_LEVEL, level) && level > 0 &&
           (result.level = static_cast<uint8_t>(level), true);
}

void showParty(std::ostream& out, const PartySnapshot& party)
{
    out << "party=" << party.joinCode << '\n'
        << "leader=" << static_cast<unsigned>(party.leader) << '\n'
        << "activity=" << partyActivityName(party.activity) << '\n'
        << "locked=" << (party.locked ? "yes" : "no") << '\n'
        << "members=" << static_cast<unsigned>(party.memberCount) << '/'
        << static_cast<unsigned>(MAX_MULTIPLAYER_PLAYERS) << '\n'
        << "connected=" << static_cast<unsigned>(party.connectedCount) << '\n'
        << "open=" << static_cast<unsigned>(party.openSlotCount) << '\n';
    for (PlayerID player = 0; player < MAX_MULTIPLAYER_PLAYERS; ++player)
    {
        const PartyMemberSnapshot& member = party.members[player];
        out << '[' << static_cast<unsigned>(player) << "] ";
        if (!member.occupied) { out << "open\n"; continue; }
        out << "name=" << member.profile.displayName
            << " class=" << partyCharacterClassName(member.profile.characterClass)
            << " level=" << static_cast<unsigned>(member.profile.level)
            << " leader=" << (member.leader ? "yes" : "no")
            << " connected=" << (member.connected ? "yes" : "no")
            << " location=" << partyLocationName(member.location)
            << " participating=" << (member.participating ? "yes" : "no") << '\n';
    }
}

CliPartyRecord* findPartyRecord(CliPartyRecord records[], const char* code)
{
    char normalized[PARTY_JOIN_CODE_SIZE] = {};
    if (!PartyRegistry::normalizeJoinCode(code, normalized)) return nullptr;
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
        if (records[index].used && normalized == std::string(records[index].code))
            return &records[index];
    return nullptr;
}

CliPartyRecord* allocatePartyRecord(CliPartyRecord records[])
{
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
        if (!records[index].active) return &records[index];
    return nullptr;
}
}

int runInteractive(std::istream& input, std::ostream& output)
{
    using namespace pfme;
    GameSession session;
    PlayerHandle handles[MAX_MULTIPLAYER_PLAYERS];
    CliPartyEntropy partyEntropy;
    PartyRegistry partyRegistry(partyEntropy);
    CliPartyRecord partyRecords[PARTY_REGISTRY_CAPACITY];
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
        std::string words[6];
        size_t count = 0;
        while (count < 6 && parser >> words[count]) ++count;
        std::string excess;
        if (parser >> excess)
        { output << "error=MalformedCommand (help lists syntax)\n"; continue; }
        if (count == 0) { if (input.eof()) break; continue; }
        const std::string& command = words[0];
        uint64_t value = 0;
        if (command == "quit" && count == 1) break;
        if (command == "help" && count == 1)
        {
            output << "help | new <seed> | join <name> | players | map | where <id>\n"
                   << "move <id> <north|east|south|west> | disconnect <id>\n"
                   << "reconnect <id> | leave <id> | events [since] | status | quit\n"
                   << "party-create <name> [class level] | party-list\n"
                   << "party-status <code> | party-join <code> <name> [class level]\n"
                   << "party-disconnect <code> <id> | party-reconnect <code> <id>\n"
                   << "party-leave <code> <id> | party-lock <code> | party-unlock <code>\n"
                   << "party-start-forest <code> | party-start-dungeon <code>\n"
                   << "party-close <code> | party-events <code> [since]\n"
                   << "Names: 1-15 printable non-space ASCII characters.\n";
        }
        else if (command == "party-create")
        {
            PartyPlayerProfile profile;
            if (!partyProfile(words, count, 1, profile))
            { output << "error=MalformedCommand (help lists syntax)\n"; continue; }
            CliPartyRecord* record = allocatePartyRecord(partyRecords);
            if (record == nullptr) { partyError(output, PartyError::RegistryFull); continue; }
            PartyHandle party;
            PartyMembership host;
            const PartyError result = partyRegistry.createParty(profile, party, host);
            if (result != PartyError::Ok) { partyError(output, result); continue; }
            PartySnapshot snapshot;
            partyRegistry.snapshot(party, snapshot);
            *record = CliPartyRecord{};
            record->used = record->active = true;
            record->party = party;
            record->members[HOST_PLAYER_ID] = host;
            for (size_t index = 0; index < sizeof(record->code); ++index)
                record->code[index] = snapshot.joinCode[index];
            output << "party-created code=" << record->code
                   << " player=0 reconnect=retained\n";
        }
        else if (command == "party-list" && count == 1)
        {
            PartyList parties;
            const PartyError result = partyRegistry.list(parties);
            if (result != PartyError::Ok) { partyError(output, result); continue; }
            output << "parties=" << parties.count << '\n';
            for (size_t index = 0; index < parties.count; ++index)
            {
                const PartySnapshot& party = parties.parties[index];
                output << "party=" << party.joinCode
                       << " activity=" << partyActivityName(party.activity)
                       << " members=" << static_cast<unsigned>(party.memberCount) << '/'
                       << static_cast<unsigned>(MAX_MULTIPLAYER_PLAYERS)
                       << " connected=" << static_cast<unsigned>(party.connectedCount)
                       << " locked=" << (party.locked ? "yes" : "no") << '\n';
            }
        }
        else if (command == "party-status" && count == 2)
        {
            PartySnapshot snapshot;
            const PartyError result = partyRegistry.snapshot(words[1].c_str(), snapshot);
            if (result != PartyError::Ok) partyError(output, result);
            else showParty(output, snapshot);
        }
        else if (command == "party-join")
        {
            PartyPlayerProfile profile;
            if (!partyProfile(words, count, 2, profile))
            { output << "error=MalformedCommand (help lists syntax)\n"; continue; }
            CliPartyRecord* record = findPartyRecord(partyRecords, words[1].c_str());
            PartyMembership member;
            const PartyError result = partyRegistry.joinParty(
                words[1].c_str(), profile, member);
            if (result != PartyError::Ok) { partyError(output, result); continue; }
            if (record != nullptr) record->members[member.player] = member;
            output << "party-joined code=" << words[1]
                   << " player=" << static_cast<unsigned>(member.player)
                   << " reconnect=retained\n";
        }
        else if ((command == "party-disconnect" || command == "party-reconnect" ||
                  command == "party-leave") && count == 3 &&
                 number(words[2], UINT8_MAX, value))
        {
            const PlayerID player = static_cast<PlayerID>(value);
            CliPartyRecord* record = findPartyRecord(partyRecords, words[1].c_str());
            if (record == nullptr || !isValidPlayerID(player))
            { partyError(output, PartyError::InvalidMember); continue; }
            PartyError result = PartyError::InvalidMember;
            if (command == "party-disconnect")
                result = partyRegistry.disconnect(record->members[player].handle);
            else if (command == "party-reconnect")
            {
                PartyMembership restored;
                result = partyRegistry.reconnect(record->party, player,
                    record->members[player].reconnectToken, restored);
                if (result == PartyError::Ok) record->members[player] = restored;
            }
            else
            {
                result = partyRegistry.leave(record->members[player].handle);
                if (result == PartyError::Ok)
                {
                    record->members[player].handle = PartyMemberHandle{};
                    if (player == HOST_PLAYER_ID) record->active = false;
                }
            }
            if (result != PartyError::Ok) partyError(output, result);
            else output << command << " ok code=" << record->code
                        << " player=" << static_cast<unsigned>(player) << '\n';
        }
        else if ((command == "party-lock" || command == "party-unlock" ||
                  command == "party-start-forest" || command == "party-start-dungeon" ||
                  command == "party-close") && count == 2)
        {
            CliPartyRecord* record = findPartyRecord(partyRecords, words[1].c_str());
            if (record == nullptr)
            { partyError(output, PartyError::PartyNotFound); continue; }
            PartyError result = PartyError::InvalidMember;
            if (command == "party-lock")
                result = partyRegistry.setLocked(record->members[0].handle, true);
            else if (command == "party-unlock")
                result = partyRegistry.setLocked(record->members[0].handle, false);
            else if (command == "party-start-forest")
                result = partyRegistry.startForest(record->members[0].handle);
            else if (command == "party-start-dungeon")
                result = partyRegistry.startDungeon(record->members[0].handle);
            else result = partyRegistry.closeParty(record->members[0].handle);
            if (result != PartyError::Ok) partyError(output, result);
            else
            {
                if (command == "party-close") record->active = false;
                output << command << " ok code=" << record->code << '\n';
            }
        }
        else if (command == "party-events" && (count == 2 ||
                 (count == 3 && number(words[2], UINT64_MAX, value))))
        {
            CliPartyRecord* record = findPartyRecord(partyRecords, words[1].c_str());
            if (record == nullptr)
            { partyError(output, PartyError::PartyNotFound); continue; }
            PartyEvents events;
            const PartyError result = partyRegistry.eventsSince(record->party, value, events);
            if (result != PartyError::Ok && result != PartyError::HistoryGap)
            { partyError(output, result); continue; }
            output << "party-events oldest=" << events.oldest
                   << " newest=" << events.newest
                   << " gap=" << (events.gap ? "true" : "false") << '\n';
            for (size_t index = 0; index < events.count; ++index)
            {
                const PartyEvent& event = events.events[index];
                output << "seq=" << event.sequence
                       << " type=" << partyEventName(event.type)
                       << " player=" << static_cast<unsigned>(event.player)
                       << " activity=" << partyActivityName(event.activity) << '\n';
            }
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
