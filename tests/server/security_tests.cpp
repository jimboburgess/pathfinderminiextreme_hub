#include "credentials.h"
#include "protocol.h"
#include "server_limits.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ws = beast::websocket;
using tcp = asio::ip::tcp;
using namespace pfme::server;
using namespace std::chrono_literals;
unsigned checks = 0;
void check(bool condition, const char* label)
{ if (!condition) throw std::runtime_error(label); ++checks; }
std::string code(const json::value& v)
{
    if (!v.is_object() || !v.as_object().contains("error")) return "";
    return std::string(v.at("error").at("code").as_string());
}
bool good(const json::value& v) { return v.is_object() && v.at("ok").as_bool(); }
int number(const json::value& v)
{ return v.is_int64() ? static_cast<int>(v.as_int64()) : static_cast<int>(v.as_uint64()); }

class Client
{
public:
    explicit Client(unsigned port, const char* origin = nullptr,
                    const char* target = "/", const char* host = nullptr,
                    const char* claimedHeader = nullptr) : socket(io)
    {
        socket.next_layer().connect({asio::ip::address_v4::loopback(), static_cast<unsigned short>(port)});
        socket.read_message_max(MAX_RESPONSE_BYTES);
        if (origin || claimedHeader) socket.set_option(ws::stream_base::decorator([origin,claimedHeader](ws::request_type& req) {
            if(origin)req.set(beast::http::field::origin,origin);
            if(claimedHeader)req.set(claimedHeader,"claimed-user@example.com");
        }));
        socket.handshake(host ? std::string(host) : "127.0.0.1:" + std::to_string(port), target);
        check(good(read()), "hello response");
    }
    ~Client() { abort(); }
    void abort()
    { boost::system::error_code ignored; socket.next_layer().close(ignored); }
    void graceful()
    {
        boost::system::error_code ec;
        socket.close(ws::close_code::normal, ec);
        check(!ec, "client closing handshake");
    }
    json::value read()
    {
        beast::flat_buffer buffer(MAX_RESPONSE_BYTES);
        boost::system::error_code ec;
        socket.read(buffer, ec);
        if (ec) { closed = true; closeCode = socket.reason().code; return nullptr; }
        return json::parse(beast::buffers_to_string(buffer.data()));
    }
    json::value raw(const std::string& text)
    {
        if (pace && ++messages % 12 == 0) std::this_thread::sleep_for(1100ms);
        socket.text(true);
        socket.write(asio::buffer(text));
        return read();
    }
    json::value call(const char* type, json::object extra = {})
    {
        extra["v"] = 1; extra["type"] = type; extra["id"] = std::to_string(++ids);
        return raw(json::serialize(extra));
    }
    json::value auth(const std::string& secret) { return call("authenticate", {{"secret", secret}}); }
    asio::io_context io;
    ws::stream<tcp::socket> socket;
    bool closed = false, pace = true;
    unsigned closeCode = 0, ids = 0, messages = 0;
};

void authentication(unsigned port, const std::string& secret)
{
    {
        Client claims(port);
        check(code(claims.call("authenticate",{{"secret",secret},{"identity","claimed"}}))=="AuthenticationFailed", "shared gate plus identity is not verified identity");
        check(code(claims.call("authenticate",{{"Tailscale-User-Login","claimed-user@example.com"}}))=="AuthenticationFailed", "JSON identity cannot authenticate");
        check(good(claims.auth(secret)),"unsupported claims do not disable valid development admission");
    }
    for(const char* header : {"Tailscale-User-Login","tAiLsCaLe-UsEr-LoGiN","Tailscale-User-Name",
        "Tailscale-User-Profile-Pic","Tailscale-App-Capabilities","X-Forwarded-User","Forwarded","X-Real-IP"}) {
        bool rejected=false;try {Client forged(port,nullptr,"/",nullptr,header);}catch(...){rejected=true;}
        check(rejected,"untrusted proxy identity rejected before WebSocket upgrade");
    }
    Client client(port);
    check(code(client.call("players")) == "NotAuthenticated", "unauthenticated gameplay rejected");
    check(code(client.call("map")) == "NotAuthenticated", "unauthenticated map denied");
    check(code(client.call("authenticate")) == "AuthenticationFailed", "missing credential rejected");
    auto wrong = client.auth(randomCredential());
    check(code(wrong) == "AuthenticationFailed", "wrong credential rejected");
    auto accepted = client.auth(secret);
    check(good(accepted), "correct credential accepted");
    check(accepted.at("data").at("connection_id").as_string().size() == 64, "server connection identity");
    check(json::serialize(accepted).find(secret) == std::string::npos, "authentication does not echo secret");
    check(code(client.auth(secret)) == "AlreadyAuthenticated", "duplicate authentication rejected");
    client.graceful();
    {
        Client abusive(port);
        for (unsigned i = 0; i < AUTH_FAILURE_LIMIT; ++i)
            check(code(abusive.auth(randomCredential())) == "AuthenticationFailed", "failed auth limit response");
        abusive.read();
        check(abusive.closed, "failed auth connection closed");
    }
    {
        Client silent(port);
        silent.read();
        check(silent.closed, "authentication deadline enforced");
    }
}

void protocol(unsigned port, const std::string& secret)
{
    Client client(port);
    check(good(client.auth(secret)), "protocol client auth");
    check(code(client.raw("{")) == "MalformedJson", "malformed JSON");
    check(code(client.raw("[]")) == "MalformedJson", "top-level array rejected");
    check(code(client.raw("{\"v\":2,\"id\":\"version\",\"type\":\"ping\"}")) == "UnsupportedVersion", "unsupported version");
    check(code(client.raw("{\"v\":true,\"id\":\"type\",\"type\":\"ping\"}")) == "UnsupportedVersion", "invalid version type");
    check(code(client.raw("{\"v\":1.0,\"id\":\"float\",\"type\":\"ping\"}")) == "UnsupportedVersion", "version must be integer");
    check(code(client.call("unknown")) == "UnknownCommand", "unknown command");
    check(code(client.call("create_session", {{"seed", 9}})) == "UnknownCommand", "no remote administration");
    check(code(client.call("ping", {{"player_id", 0}})) == "InvalidRequest", "unknown field rejected");
    check(code(client.call("join", {{"name", 123}})) == "InvalidRequest", "invalid name type");
    check(code(client.call("join", {{"name", "1234567890123456"}})) == "InvalidRequest", "bounded name");
    check(code(client.call("events", {{"since", 0}})) == "InvalidRequest", "cursor type is decimal string");
    check(code(client.call("events", {{"since", "18446744073709551616"}})) == "InvalidRequest", "cursor overflow");
    std::string deep = "{\"v\":1,\"id\":\"depth\",\"type\":\"ping\",\"x\":";
    deep += std::string(12, '[') + "0" + std::string(12, ']') + "}";
    check(code(client.raw(deep)) == "MalformedJson", "bounded JSON depth");
    auto correlation = client.raw("{\"v\":1,\"id\":\"correlation\",\"type\":\"ping\"}");
    check(good(correlation) && correlation.at("id") == "correlation", "request correlation");
    const std::string fragmented = "{\"v\":1,\"id\":\"fragments\",\"type\":\"ping\"}";
    client.socket.text(true);
    client.socket.write_some(false, asio::buffer(fragmented.data(), 15));
    client.socket.ping({});
    client.socket.write_some(true, asio::buffer(fragmented.data() + 15, fragmented.size() - 15));
    check(good(client.read()), "fragmented message with interleaved control frame");
    check(client.call("players").at("data").at("players").as_array().empty(), "invalid requests did not join players");
    client.graceful();
    {
        Client large(port); check(good(large.auth(secret)), "oversized client auth");
        large.raw(std::string(MAX_REQUEST_BYTES + 1, 'x'));
        if (!large.closed || large.closeCode != 1009)
            std::cerr << "oversized_close_code=" << large.closeCode << " closed=" << large.closed << '\n';
        check(large.closed && large.closeCode == 1009, "oversized WebSocket message closed with 1009");
    }
    {
        Client binary(port); check(good(binary.auth(secret)), "binary client auth");
        binary.socket.binary(true); binary.socket.write(asio::buffer("binary", 6));
        check(code(binary.read()) == "TextRequired", "binary frames rejected");
        binary.read(); check(binary.closed, "binary violation closes peer");
    }
    {
        Client invalidUtf8(port); check(good(invalidUtf8.auth(secret)), "UTF-8 client auth");
        invalidUtf8.raw(std::string(1, static_cast<char>(0xff)));
        check(invalidUtf8.closed && invalidUtf8.closeCode == 1007, "invalid UTF-8 closes with 1007");
    }
    bool originRejected = false;
    try { Client browser(port, "https://untrusted.invalid"); }
    catch (...) { originRejected = true; }
    check(originRejected, "browser origin rejected in local CLI phase");
    bool queryRejected = false;
    try { Client query(port, nullptr, "/?credential=forbidden"); }
    catch (...) { queryRejected = true; }
    check(queryRejected, "query strings rejected");
    bool hostRejected = false;
    try { Client otherHost(port, nullptr, "/", "untrusted.invalid"); }
    catch (...) { hostRejected = true; }
    check(hostRejected, "foreign Host rejected without DNS lookup");
}

void multiplayer(unsigned port, const std::string& secret)
{
    Client fighter(port), wizard(port), rogue(port), observer(port);
    for (Client* client : {&fighter, &wizard, &rogue, &observer})
        check(good(client->auth(secret)), "multi-client authentication");
    const auto f = fighter.call("join", {{"name", "Fighter"}});
    const auto w = wizard.call("join", {{"name", "Wizard"}});
    const auto r = rogue.call("join", {{"name", "Rogue"}});
    check(good(f) && good(w) && good(r), "three independent joins");
    const std::string token(f.at("data").at("reconnect_token").as_string());
    const std::string rogueToken(r.at("data").at("reconnect_token").as_string());
    check(token.size() == 64 && token != secret && token != rogueToken &&
          token != w.at("data").at("reconnect_token").as_string(), "distinct unpredictable ownership credentials");
    check(number(f.at("data").at("player").at("player_id")) == 0 &&
          number(w.at("data").at("player").at("player_id")) == 1 &&
          number(r.at("data").at("player").at("player_id")) == 2, "distinct participant ownership");
    check(code(fighter.call("join", {{"name", "Duplicate"}})) == "AlreadyBound", "one player per connection");
    check(good(fighter.call("move", {{"direction", "east"}})), "Fighter moves on production graph");
    check(good(rogue.call("move", {{"direction", "east"}})) &&
          good(rogue.call("move", {{"direction", "north"}})), "Rogue moves independently");
    auto players = observer.call("players").at("data").at("players").as_array();
    check(number(players[0].at("room")) == 1 && number(players[1].at("room")) == 0 &&
          number(players[2].at("room")) == 2, "three different rooms, no group teleport");
    std::cout << "multiplayer_rooms=Fighter:" << number(players[0].at("room"))
              << ",Wizard:" << number(players[1].at("room"))
              << ",Rogue:" << number(players[2].at("room")) << '\n';
    const auto before = observer.call("events", {{"since", "0"}}).at("data");
    check(code(wizard.call("move", {{"direction", "east"}, {"player_id", 0}})) == "InvalidRequest", "cannot select another player");
    check(code(observer.call("reconnect", {{"player_id", 0}})) == "OwnershipRequired", "numeric ID is not proof");
    check(code(observer.call("reconnect", {{"token", randomCredential()}})) == "OwnershipRequired", "incorrect ownership proof");
    check(code(observer.call("reconnect", {{"token", token}})) == "AlreadyConnected", "active connection takeover rejected");
    check(code(fighter.call("move", {{"direction", "east"}})) == "NoExit", "nonexistent exit rejected");
    check(code(fighter.call("move", {{"direction", 5}})) == "InvalidDirection", "invalid movement type");
    check(code(fighter.call("move", {{"direction", "up"}})) == "InvalidDirection", "invalid direction rejected");
    const auto after = observer.call("events", {{"since", "0"}}).at("data");
    check(before == after, "invalid commands preserve session and event history");
    check(wizard.call("events", {{"since", "0"}}).at("data") == after, "events shared by authorized clients");
    check(json::serialize(observer.call("players")).find(token) == std::string::npos &&
          json::serialize(after).find(token) == std::string::npos, "queries do not leak ownership credentials");
    fighter.abort(); std::this_thread::sleep_for(100ms);
    check(code(observer.call("reconnect",{{"token",secret}}))=="OwnershipRequired","development gate cannot take over disconnected character");
    check(code(observer.call("reconnect",{{"token",token},{"identity","Fighter"}}))=="OwnershipRequired","client identity cannot augment reconnect proof");
    auto resumed = observer.call("reconnect", {{"token", token}});
    check(good(resumed) && number(resumed.at("data").at("player").at("room")) == 1,
          "abrupt disconnect and verified reconnect preserve room");
    std::cout << "verified_reconnect_room=" << number(resumed.at("data").at("player").at("room")) << '\n';
    check(good(observer.call("disconnect")), "explicit owned disconnect");
    check(code(observer.call("move", {{"direction", "west"}})) == "NotBound", "disconnected peer cannot move");
    check(good(observer.call("reconnect", {{"token", token}})), "retained proof reconnect");
    for (unsigned i = 0; i < 18; ++i)
    {
        check(good(observer.call("disconnect")), "history disconnect");
        check(good(observer.call("reconnect", {{"token", token}})), "history reconnect");
    }
    const auto gap = wizard.call("events", {{"since", "0"}});
    check(code(gap) == "HistoryGap" && gap.at("data").at("gap").as_bool(), "history gap explicit");
    check(gap.at("data").at("events").as_array().size() == 32, "history retrieval bounded");
    const auto newest = gap.at("data").at("newest");
    check(wizard.call("events", {{"since", newest}}).at("data").at("events").as_array().empty(), "events since newest empty");
    check(code(wizard.call("events", {{"since", "18446744073709551615"}})) == "InvalidCursor", "future cursor rejected");
    check(good(rogue.call("leave")), "explicit leave releases participant");
    check(code(rogue.call("reconnect", {{"token", rogueToken}})) == "OwnershipRequired", "leave revokes proof");
    check(good(rogue.call("join", {{"name", "NewRogue"}})), "slot reuse through authorized join");
    Client fourth(port), excess(port);
    check(good(fourth.auth(secret)) && good(excess.auth(secret)), "capacity clients authenticated");
    check(good(fourth.call("join", {{"name", "Cleric"}})), "fourth participant joins");
    check(good(fourth.call("disconnect")), "fourth player retains disconnected record");
    check(code(excess.call("join", {{"name", "Extra"}})) == "Full", "disconnected records reserve capacity");
    check(good(excess.call("ping")), "unbound observer remains usable");
}

void networking(unsigned port, const std::string& secret)
{
    std::this_thread::sleep_for(1100ms);
    {
        Client limited(port); check(good(limited.auth(secret)), "rate client auth");
        limited.pace = false;
        bool rejected = false;
        for (unsigned i = 0; i < COMMANDS_PER_SECOND + 2; ++i)
        {
            if (code(limited.call("ping")) == "RateLimited") { rejected = true; break; }
        }
        check(rejected, "command rate limit");
        limited.read(); check(limited.closed, "rate limited connection closed");
    }
    {
        Client slow(port), healthy(port);
        check(good(slow.auth(secret)) && good(healthy.auth(secret)), "slow-reader clients authenticated");
        for (unsigned i = 0; i < 24; ++i)
        {
            boost::system::error_code ec;
            slow.socket.write(asio::buffer("{\"v\":1,\"id\":\"slow\",\"type\":\"map\"}", 32), ec);
            if (ec) break;
        }
        check(good(healthy.call("ping")), "slow reader does not block other client");
    }
    {
        Client controlFlood(port); check(good(controlFlood.auth(secret)), "control flood client auth");
        for (unsigned i = 0; i < 48; ++i)
        {
            boost::system::error_code ec;
            controlFlood.socket.ping({}, ec);
            if (ec) break;
        }
        controlFlood.read();
        check(controlFlood.closed, "control-frame flood bounded");
    }
    std::this_thread::sleep_for(1100ms);
    {
        asio::io_context io;
        std::vector<std::unique_ptr<tcp::socket>> held;
        for (std::size_t i = 0; i < MAX_CONNECTIONS; ++i)
        {
            auto socket = std::make_unique<tcp::socket>(io);
            socket->connect({asio::ip::address_v4::loopback(), static_cast<unsigned short>(port)});
            held.push_back(std::move(socket));
        }
        std::this_thread::sleep_for(100ms);
        tcp::socket extra(io);
        extra.connect({asio::ip::address_v4::loopback(), static_cast<unsigned short>(port)});
        char byte;
        boost::system::error_code ec;
        extra.read_some(asio::buffer(&byte, 1), ec);
        check(bool(ec), "connection cap enforced before HTTP handshake");
        held.front()->read_some(asio::buffer(&byte, 1), ec);
        check(bool(ec), "stalled HTTP handshake timed out");
    }
    std::this_thread::sleep_for(1100ms);
    Client healthy(port); check(good(healthy.auth(secret)) && good(healthy.call("ping")), "server recovers after abusive peers");
    healthy.graceful();
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2 || !std::getenv("PFME_TEST_SECRET")) return 2;
        const unsigned port = static_cast<unsigned>(std::stoul(argv[1]));
        const std::string secret(std::getenv("PFME_TEST_SECRET"));
        if (argc == 3)
        {
            Client client(port); check(good(client.auth(secret)), "shutdown client authentication");
            std::cout << "shutdown_client_ready\n" << std::flush;
            client.read();
            check(client.closed && client.closeCode == 1000, "server closing handshake");
            std::cout << "shutdown_client_closed\n";
            return 0;
        }
        authentication(port, secret);
        protocol(port, secret);
        multiplayer(port, secret);
        networking(port, secret);
        std::cout << "security_checks=" << checks << " passed\n";
        return 0;
    }
    catch (const std::runtime_error& e) { std::cerr << "security check failed: " << e.what() << '\n'; return 1; }
    catch (...) { std::cerr << "security integration failed\n"; return 1; }
}
