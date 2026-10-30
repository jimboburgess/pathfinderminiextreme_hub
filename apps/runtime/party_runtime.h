#pragma once
#include "session/game_session.h"
#include "session/party_registry.h"
#include "audio_stream.h"
#include <memory>
#include <string>
namespace pfme { namespace runtime {
// Authentication and capability validation stay in Service/GameSession. This
// adapter leases production controllers to already verified capabilities.
class PartyRuntime {
public:
    PartyRuntime(GameSession&,uint32_t seed);
    ~PartyRuntime();
    bool start(const PlayerHandle&,uint64_t serial);
    bool input(const PlayerHandle&,uint64_t serial,const std::string&,bool);
    const uint16_t* pixels(const PlayerHandle&,uint64_t serial);
    bool audio(const PlayerHandle&,uint64_t serial,uint64_t since,bool fresh,AudioBatch&);
    void detach(const PlayerHandle&,uint64_t serial);
    bool leave(const PlayerHandle&,uint64_t serial);
    void tick(uint32_t milliseconds);
    bool status(const PlayerHandle&,uint64_t serial,PartySnapshot&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}}
