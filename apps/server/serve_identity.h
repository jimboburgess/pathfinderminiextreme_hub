#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
namespace pfme::server {
using ServeClock=std::chrono::steady_clock;
struct ServeOptions {std::string socket,origin,policy;};
struct ServeSnapshot {uint64_t revision=0;std::map<std::string,std::string> users;};
struct ServeLease {
    std::string login,principal;uint64_t revision=0;
    ServeClock::time_point expires{};
};
bool parseServePolicy(std::string_view,ServeSnapshot&);
bool serveLeaseValid(const ServeSnapshot&,const ServeLease&,ServeClock::time_point);
bool validServeOrigin(std::string_view);
class ServePolicy {
public:
    explicit ServePolicy(std::string path);
    void refresh();
    bool admit(std::string_view login,ServeLease&);
    bool valid(const ServeLease&) const;
private:
    std::string path_,lastText_;ServeSnapshot snapshot_;
    ServeClock::time_point nextRead_{};bool healthy_=false;
};
}
