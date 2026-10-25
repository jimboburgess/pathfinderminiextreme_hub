#include "service.h"
#include "server_limits.h"
#include <stdexcept>

namespace pfme::server
{
namespace
{
json::object playerJson(const SessionPlayer& player)
{ return {{"player_id", player.id}, {"name", player.name}, {"room", player.room}, {"connected", player.connected}}; }
json::value roomJson(uint8_t room)
{ return room == NO_ROOM ? json::value(nullptr) : json::value(room); }
}
Service::Service(const SecretDigest& gate, uint32_t seed) : gate_(gate)
{
    if (game_.initialize(seed) != SessionError::Ok) throw std::runtime_error("session initialization failed");
}
std::string Service::hello()
{ return json::serialize(response("", "hello", {{"authentication_required", true}, {"navigation", "topology-only"}})); }
void Service::detach(ConnectionState& connection)
{
    if (isValidPlayerID(connection.player))
    {
        auto& participant = participants_[connection.player];
        if (participant.occupied && participant.activeConnection == connection.serial)
        {
            game_.disconnect(participant.handle);
            participant.activeConnection = 0;
        }
    }
    connection.player = INVALID_PLAYER_ID;
}
Reply Service::handle(ConnectionState& connection, std::string_view message)
{
    try
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - connection.window >= std::chrono::seconds(1))
        { connection.window = now; connection.commands = 0; }
        if (++connection.commands > COMMANDS_PER_SECOND)
            return {json::serialize(failure("", "RateLimited")), true};
        Request request;
        std::string error;
        if (!parseRequest(message, request, error)) return {json::serialize(failure(request.id, error)), false};
        auto result = dispatch(connection, request);
        auto text = json::serialize(result);
        if (text.size() > MAX_RESPONSE_BYTES) return {json::serialize(failure(request.id, "ResponseLimit")), true};
        return {std::move(text), connection.authFailures >= AUTH_FAILURE_LIMIT};
    }
    catch (...)
    { return {json::serialize(failure("", "InternalError")), true}; }
}
json::object Service::dispatch(ConnectionState& connection, const Request& request)
{
    auto fail = [&](const std::string& code) { return failure(request.id, code); };
    auto ok = [&](json::object data = {}) { return response(request.id, request.type, std::move(data)); };
    if (request.type == "hello")
        return fieldsAre(request, {}) ? ok({{"authentication_required", !connection.authenticated}}) : fail("InvalidRequest");
    if (request.type == "authenticate")
    {
        if (connection.authenticated) return fail("AlreadyAuthenticated");
        std::string secret;
        SecretDigest candidate{};
        if (!fieldsAre(request, {"secret"}) || !textField(request, "secret", 64, secret) ||
            !credentialDigest(secret, candidate) || !sameDigest(candidate, gate_))
        { ++connection.authFailures; return fail("AuthenticationFailed"); }
        connection.identity = randomCredential();
        connection.authenticated = true;
        return ok({{"connection_id", connection.identity}});
    }
    if (!connection.authenticated) return fail("NotAuthenticated");
    if (request.type == "join")
    {
        if (isValidPlayerID(connection.player)) return fail("AlreadyBound");
        std::string name;
        if (!fieldsAre(request, {"name"}) || !textField(request, "name", NETWORK_PLAYER_NAME_SIZE - 1, name))
            return fail("InvalidRequest");
        const std::string token = randomCredential();
        SecretDigest proof{};
        if (!credentialDigest(token, proof)) return fail("InternalError");
        PlayerHandle handle;
        const auto result = game_.join(name.c_str(), handle);
        if (result != SessionError::Ok) return fail(sessionErrorName(result));
        SessionPlayer player;
        game_.player(handle, player);
        auto& participant = participants_[player.id];
        participant.occupied = true;
        participant.activeConnection = connection.serial;
        participant.handle = handle;
        participant.proof = proof;
        connection.player = player.id;
        return ok({{"player", playerJson(player)}, {"reconnect_token", token}});
    }
    if (request.type == "reconnect")
    {
        if (isValidPlayerID(connection.player)) return fail("AlreadyBound");
        std::string token;
        SecretDigest proof{};
        if (!fieldsAre(request, {"token"}) || !textField(request, "token", 64, token) ||
            !credentialDigest(token, proof)) return fail("OwnershipRequired");
        PlayerID found = INVALID_PLAYER_ID;
        for (PlayerID i = 0; i < MAX_MULTIPLAYER_PLAYERS; ++i)
        {
            const bool match = sameDigest(participants_[i].proof, proof);
            if (match && participants_[i].occupied) found = i;
        }
        if (!isValidPlayerID(found)) return fail("OwnershipRequired");
        auto& participant = participants_[found];
        if (participant.activeConnection != 0) return fail("AlreadyConnected");
        const auto result = game_.reconnect(participant.handle);
        if (result != SessionError::Ok) return fail(sessionErrorName(result));
        participant.activeConnection = connection.serial;
        connection.player = found;
        SessionPlayer player;
        game_.player(participant.handle, player);
        return ok({{"player", playerJson(player)}});
    }
    if (request.type == "move" || request.type == "disconnect" || request.type == "leave")
    {
        if (!fieldsAre(request, request.type == "move" ?
                       std::initializer_list<std::string_view>{"direction"} : std::initializer_list<std::string_view>{}))
            return fail("InvalidRequest");
        if (!isValidPlayerID(connection.player)) return fail("NotBound");
        auto& participant = participants_[connection.player];
        if (participant.activeConnection != connection.serial) return fail("OwnershipRequired");
        if (request.type == "disconnect") { detach(connection); return ok(); }
        if (request.type == "leave")
        {
            const auto result = game_.leave(participant.handle);
            if (result != SessionError::Ok) return fail(sessionErrorName(result));
            participant = Participant{};
            connection.player = INVALID_PLAYER_ID;
            return ok();
        }
        std::string direction;
        if (!textField(request, "direction", 5, direction)) return fail("InvalidDirection");
        Direction d;
        if (direction == "north") d = DIR_NORTH;
        else if (direction == "east") d = DIR_EAST;
        else if (direction == "south") d = DIR_SOUTH;
        else if (direction == "west") d = DIR_WEST;
        else return fail("InvalidDirection");
        const auto result = game_.move(participant.handle, d);
        if (result != SessionError::Ok) return fail(sessionErrorName(result));
        SessionPlayer player;
        game_.player(participant.handle, player);
        return ok({{"player", playerJson(player)}});
    }
    if (request.type == "players")
    {
        if (!fieldsAre(request, {})) return fail("InvalidRequest");
        json::array players;
        for (PlayerID i = 0; i < MAX_MULTIPLAYER_PLAYERS; ++i)
        { SessionPlayer player; if (game_.player(i, player) == SessionError::Ok) players.push_back(playerJson(player)); }
        return ok({{"players", std::move(players)}, {"event_cursor", std::to_string(game_.latestSequence())}});
    }
    if (request.type == "map")
    {
        if (!fieldsAre(request, {})) return fail("InvalidRequest");
        json::array rooms;
        const auto& topology = game_.dungeon();
        for (uint8_t i = 0; i < topology.roomCount; ++i)
        {
            const auto& room = topology.rooms[i];
            rooms.push_back(json::object{{"room", i}, {"north", roomJson(room.north)},
                {"east", roomJson(room.east)}, {"south", roomJson(room.south)}, {"west", roomJson(room.west)}});
        }
        return ok({{"navigation", "topology-only"}, {"rooms", std::move(rooms)}});
    }
    if (request.type == "events")
    {
        uint64_t cursor;
        if (!fieldsAre(request, {"since"}) || !cursorField(request, cursor)) return fail("InvalidRequest");
        SessionEvents events;
        const auto result = game_.eventsSince(cursor, events);
        if (result != SessionError::Ok && result != SessionError::HistoryGap) return fail(sessionErrorName(result));
        json::array items;
        for (std::size_t i = 0; i < events.count; ++i)
        {
            const auto& e = events.events[i];
            items.push_back(json::object{{"sequence", std::to_string(e.sequence)},
                {"type", sessionEventName(e.type)},
                {"player_id", isValidPlayerID(e.player) ? json::value(e.player) : json::value(nullptr)},
                {"from", roomJson(e.fromRoom)}, {"room", roomJson(e.room)}, {"text", e.text}});
        }
        auto reply = ok({{"oldest", std::to_string(events.oldest)}, {"newest", std::to_string(events.newest)},
            {"gap", events.gap}, {"events", std::move(items)}});
        if (events.gap) { reply["ok"] = false; reply["error"] = json::object{{"code", "HistoryGap"}}; }
        return reply;
    }
    if (request.type == "ping") return fieldsAre(request, {}) ? ok() : fail("InvalidRequest");
    return fail("UnknownCommand");
}
}
