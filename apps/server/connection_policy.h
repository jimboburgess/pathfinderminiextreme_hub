#pragma once
#include <array>
#include <chrono>
#include <string>
#include "../runtime/controller_capacity.h"
namespace pfme::server {
using ConnectionClock=std::chrono::steady_clock;
constexpr unsigned MAX_CONNECTED_ACCOUNTS=20,HEARTBEAT_INTERVAL_SECONDS=15,STALE_SECONDS=45,RECONNECT_GRACE_SECONDS=120;
struct Liveness {
 ConnectionClock::time_point trafficAt{},pingAt{};
 void traffic(ConnectionClock::time_point now){trafficAt=now;}
 bool stale(ConnectionClock::time_point now)const{return now-trafficAt>=std::chrono::seconds(STALE_SECONDS);}
 bool pingDue(ConnectionClock::time_point now)const{return now-trafficAt>=std::chrono::seconds(HEARTBEAT_INTERVAL_SECONDS)&&now-pingAt>=std::chrono::seconds(HEARTBEAT_INTERVAL_SECONDS);}
};
class AccountLeases {
public:
 struct Entry {std::string principal;uint64_t serial=0;ConnectionClock::time_point until{};};
 std::array<Entry,MAX_CONNECTED_ACCOUNTS> entries{};
 Entry* find(const std::string& principal){for(auto& e:entries)if(!e.principal.empty()&&e.principal==principal)return &e;return nullptr;}
 bool admit(const std::string& principal,uint64_t serial){if(auto* e=find(principal)){e->serial=serial;e->until={};return true;}for(auto& e:entries)if(e.principal.empty()){e.principal=principal;e.serial=serial;return true;}return false;}
 void detach(const std::string& principal,uint64_t serial,bool reserved,ConnectionClock::time_point now){if(auto* e=find(principal))if(e->serial==serial){if(reserved){e->serial=0;e->until=now+std::chrono::seconds(RECONNECT_GRACE_SECONDS);}else *e=Entry{};}}
 bool expired(const Entry& e,ConnectionClock::time_point now)const{return !e.principal.empty()&&!e.serial&&now>=e.until;}
 bool replaced(const std::string& principal,uint64_t serial){auto* e=find(principal);return e&&e->serial&&e->serial!=serial;}
 size_t size()const{size_t n=0;for(const auto& e:entries)n+=!e.principal.empty();return n;}
};
}
