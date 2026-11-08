#pragma once
#include "controller_capacity.h"
#include "session/game_session.h"
#include <array>
namespace pfme { namespace runtime {
// Bounded server admission ledger. Each shared GameSession retains exactly four
// local PlayerIDs; composite controller slots never enter PartyRegistry.
class SessionBank {
 static constexpr unsigned BANKS=NATIVE_CONTROLLER_CAPACITY/MAX_MULTIPLAYER_PLAYERS;
 std::array<GameSession,BANKS> sessions{};
 unsigned count_;
 int locate(const PlayerHandle& h)const{SessionPlayer p;for(unsigned i=0;i<count_;++i)if(sessions[i].player(h,p)==SessionError::Ok)return int(i);return -1;}
public:
 explicit SessionBank(bool multi=false):count_(multi?BANKS:1){}
 unsigned banks()const{return count_;}
 GameSession& bank(unsigned i){return sessions.at(i);}
 SessionError initialize(uint32_t seed){for(unsigned i=0;i<count_;++i){auto e=sessions[i].initialize(seed+i);if(e!=SessionError::Ok)return e;}return SessionError::Ok;}
 SessionError join(const char* name,PlayerHandle& h){for(unsigned i=0;i<count_;++i){auto e=sessions[i].join(name,h);if(e!=SessionError::Full)return e;}return SessionError::Full;}
 SessionError player(const PlayerHandle& h,SessionPlayer& p)const{int i=locate(h);if(i<0)return SessionError::InvalidHandle;auto e=sessions[i].player(h,p);if(e==SessionError::Ok)p.id=static_cast<PlayerID>(i*MAX_MULTIPLAYER_PLAYERS+p.id);return e;}
 SessionError player(PlayerID slot,SessionPlayer& p)const{unsigned i=slot/MAX_MULTIPLAYER_PLAYERS;if(i>=count_)return SessionError::InvalidPlayer;auto e=sessions[i].player(slot%MAX_MULTIPLAYER_PLAYERS,p);if(e==SessionError::Ok)p.id=slot;return e;}
 SessionError disconnect(const PlayerHandle& h){int i=locate(h);return i<0?SessionError::InvalidHandle:sessions[i].disconnect(h);}
 SessionError reconnect(const PlayerHandle& h){int i=locate(h);return i<0?SessionError::InvalidHandle:sessions[i].reconnect(h);}
 SessionError leave(const PlayerHandle& h){int i=locate(h);return i<0?SessionError::InvalidHandle:sessions[i].leave(h);}
 SessionError move(const PlayerHandle& h,Direction d){int i=locate(h);return i<0?SessionError::InvalidHandle:sessions[i].move(h,d);}
 size_t playerCount()const{size_t n=0;for(unsigned i=0;i<count_;++i)n+=sessions[i].playerCount();return n;}
 size_t connectedCount()const{size_t n=0;for(unsigned i=0;i<count_;++i)n+=sessions[i].connectedCount();return n;}
 uint64_t latestSequence()const{return sessions[0].latestSequence();}
 const DungeonTopology& dungeon()const{return sessions[0].dungeon();}
 SessionError eventsSince(uint64_t sequence,SessionEvents& events)const{return sessions[0].eventsSince(sequence,events);}
};
}}
