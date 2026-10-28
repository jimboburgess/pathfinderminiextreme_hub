#ifndef PFME_SERVER_SERVICE_H
#define PFME_SERVER_SERVICE_H
#include "credentials.h"
#include "protocol.h"
#include "session/game_session.h"
#include <chrono>
#ifdef PFME_HAS_PLAYABLE
#include "screen_stream.h"
#include "party_runtime.h"
#endif

namespace pfme::server
{
struct ConnectionState
{
    uint64_t serial = 0;
    bool authenticated = false;
    std::string identity; // Issued after authentication; separate from socket serial/player ID.
    PlayerID player = INVALID_PLAYER_ID;
    uint64_t audioCursor=0;
    bool audioReady=false;
    unsigned authFailures = 0;
    unsigned commands = 0;
    std::chrono::steady_clock::time_point window = std::chrono::steady_clock::now();
};
struct Reply { std::string text; bool close = false; };

// Called only from the single Asio event-loop thread. No network code in GameSession.
class Service
{
public:
    Service(const SecretDigest& gate, uint32_t seed, bool partyMode=false);
    Reply handle(ConnectionState& connection, std::string_view message);
    void detach(ConnectionState& connection);
    static std::string hello();
    void tick();
private:
    struct Participant
    {
        bool occupied = false;
        uint64_t activeConnection = 0;
        PlayerHandle handle;
        SecretDigest proof{};
    };
    json::object dispatch(ConnectionState& connection, const Request& request);
#ifdef PFME_HAS_PLAYABLE
    uint32_t seed_ = 0;
    bool partyMode_=false;
    std::unique_ptr<runtime::PartyRuntime> party_;
    ScreenStream partyScreens_[MAX_MULTIPLAYER_PLAYERS];
    bool partyStarted_[MAX_MULTIPLAYER_PLAYERS]{};
    std::chrono::steady_clock::time_point partyLastInput_[MAX_MULTIPLAYER_PLAYERS];
    PlayerID playOwner_ = INVALID_PLAYER_ID;
    bool playRetired_ = false;
    ScreenStream screen_;
    std::chrono::steady_clock::time_point lastInput_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastTick_ = std::chrono::steady_clock::now();
#endif
    GameSession game_;
    SecretDigest gate_;
    Participant participants_[MAX_MULTIPLAYER_PLAYERS];
};
}
#endif
