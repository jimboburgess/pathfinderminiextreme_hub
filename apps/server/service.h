#ifndef PFME_SERVER_SERVICE_H
#define PFME_SERVER_SERVICE_H
#include "credentials.h"
#include "protocol.h"
#include "session/game_session.h"
#include "../runtime/session_bank.h"
#include <chrono>
#include <map>
#include <functional>
#include "connection_policy.h"
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
    std::string verifiedPrincipal; // Only the kernel-authenticated local transport sets this.
    uint64_t lastRequest=0;
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
    using Now=std::function<ConnectionClock::time_point()>;
    Service(const SecretDigest& gate, uint32_t seed, bool partyMode=false, bool serveMode=false, Now now=[] {return ConnectionClock::now();});
    Reply handle(ConnectionState& connection, std::string_view message);
    void detach(ConnectionState& connection);
    static std::string hello(bool serveMode=false);
    void tick();
    bool replaced(const ConnectionState& c){return c.authenticated&&leases_.replaced(c.verifiedPrincipal,c.serial);}
    json::object describe(ConnectionState&);
    json::object metrics()const;
private:
    Now now_;AccountLeases leases_;
    uint64_t requests_=0,reconnects_=0,expirations_=0,full_=0,authFailures_=0,frames_=0,unchanged_=0,bytes_=0;
    void expireReservations();
    void lifecycle(const char*,const ConnectionState&,const char* reason="");
    struct Participant
    {
        bool occupied = false;
        uint64_t activeConnection = 0;
        PlayerHandle handle;
        SecretDigest proof{};
        std::string ownerPrincipal;
    };
    json::object dispatch(ConnectionState& connection, const Request& request);
#ifdef PFME_HAS_PLAYABLE
    uint32_t seed_ = 0;
    bool partyMode_=false;
    std::unique_ptr<runtime::PartyRuntime> party_;
    std::unique_ptr<ScreenStream> partyScreens_[runtime::NATIVE_CONTROLLER_CAPACITY];
    ScreenStream& partyScreen(unsigned id){auto& slot=partyScreens_[id];if(!slot)slot=std::make_unique<ScreenStream>();return *slot;}
    bool partyStarted_[runtime::NATIVE_CONTROLLER_CAPACITY]{};
    std::chrono::steady_clock::time_point partyLastInput_[runtime::NATIVE_CONTROLLER_CAPACITY];
    PlayerID playOwner_ = INVALID_PLAYER_ID;
    bool playRetired_ = false;
    ScreenStream screen_;
    std::chrono::steady_clock::time_point lastInput_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastTick_ = std::chrono::steady_clock::now();
#endif
    runtime::SessionBank game_;
    SecretDigest gate_;
    bool serveMode_=false;
    struct PrincipalBudget {std::chrono::steady_clock::time_point window{};unsigned requests=0;};
    std::map<std::string,PrincipalBudget> principalBudgets_;
    Participant participants_[runtime::NATIVE_CONTROLLER_CAPACITY];
};
}
#endif
