#ifndef PFME_SERVER_SERVICE_H
#define PFME_SERVER_SERVICE_H
#include "credentials.h"
#include "protocol.h"
#include "session/game_session.h"
#include <chrono>

namespace pfme::server
{
struct ConnectionState
{
    uint64_t serial = 0;
    bool authenticated = false;
    std::string identity; // Issued after authentication; separate from socket serial/player ID.
    PlayerID player = INVALID_PLAYER_ID;
    unsigned authFailures = 0;
    unsigned commands = 0;
    std::chrono::steady_clock::time_point window = std::chrono::steady_clock::now();
};
struct Reply { std::string text; bool close = false; };

// Called only from the single Asio event-loop thread. No network code in GameSession.
class Service
{
public:
    Service(const SecretDigest& gate, uint32_t seed);
    Reply handle(ConnectionState& connection, std::string_view message);
    void detach(ConnectionState& connection);
    static std::string hello();
private:
    struct Participant
    {
        bool occupied = false;
        uint64_t activeConnection = 0;
        PlayerHandle handle;
        SecretDigest proof{};
    };
    json::object dispatch(ConnectionState& connection, const Request& request);
    GameSession game_;
    SecretDigest gate_;
    Participant participants_[MAX_MULTIPLAYER_PLAYERS];
};
}
#endif
