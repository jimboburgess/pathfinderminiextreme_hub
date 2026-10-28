#pragma once
#include "session/game_session.h"
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
    void detach(const PlayerHandle&,uint64_t serial);
    bool leave(const PlayerHandle&,uint64_t serial);
    void tick(uint32_t milliseconds);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}}
