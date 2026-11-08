#pragma once
// Native-only, opt-in aggregate profiling. No identifiers, game state or secrets.
// Fixed storage; aggregate snapshots every five seconds and at shutdown, never per-frame logs.
#include <array>
#include "controller_capacity.h"
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
namespace pfme::perf {
inline bool enabled(){static const bool on=std::getenv("PFME_PROFILE_FILE")!=nullptr;return on;}
struct Histogram {
    uint64_t count=0,total=0,max=0;std::array<uint64_t,24> buckets{};
    void add(uint64_t us){++count;total+=us;if(us>max)max=us;unsigned b=0;while(b<23 && (uint64_t(1)<<b)<us)++b;++buckets[b];}
    uint64_t percentile(unsigned p)const {if(!count)return 0;uint64_t n=0;for(unsigned b=0;b<24;++b){n+=buckets[b];if(n*100>=count*p)return uint64_t(1)<<b;}return max;}
};
struct Metrics {
    Histogram tick,timerLate,action,serialize,write;
    std::array<Histogram,pfme::runtime::NATIVE_CONTROLLER_CAPACITY> render{},encode{};
    std::array<uint64_t,pfme::runtime::NATIVE_CONTROLLER_CAPACITY> changed{},unchanged{},chunks{},patchBytes{},wireBytes{};
    uint64_t maxOutstandingWrites=0;
};
inline Metrics metrics;
using Clock=std::chrono::steady_clock;
inline uint64_t micros(Clock::duration d){return std::chrono::duration_cast<std::chrono::microseconds>(d).count();}
struct Scope {Histogram* target;Clock::time_point start;explicit Scope(Histogram& h):target(enabled()?&h:nullptr){if(target)start=Clock::now();}~Scope(){if(target)target->add(micros(Clock::now()-start));}};
inline void report(){
    const char* path=std::getenv("PFME_PROFILE_FILE");if(!path)return;
    std::ofstream o(path);o<<"{";bool first=true;
    auto hist=[&](const char* name,const Histogram& h){if(!first)o<<",";first=false;o<<'"'<<name<<"\":{\"count\":"<<h.count<<",\"mean_us\":"<<(h.count?h.total/h.count:0)<<",\"p50_us_upper\":"<<h.percentile(50)<<",\"p95_us_upper\":"<<h.percentile(95)<<",\"max_us\":"<<h.max<<"}";};
    hist("simulation_tick",metrics.tick);hist("timer_lateness",metrics.timerLate);hist("input_to_accepted",metrics.action);hist("json_serialize",metrics.serialize);hist("socket_write",metrics.write);
    o<<",\"players\":[";
    for(unsigned i=0;i<pfme::runtime::NATIVE_CONTROLLER_CAPACITY;++i){if(i)o<<",";o<<"{";first=true;hist("render",metrics.render[i]);hist("encode",metrics.encode[i]);o<<",\"changed_frames\":"<<metrics.changed[i]<<",\"unchanged_polls\":"<<metrics.unchanged[i]<<",\"chunks\":"<<metrics.chunks[i]<<",\"patch_bytes\":"<<metrics.patchBytes[i]<<",\"wire_bytes\":"<<metrics.wireBytes[i]<<"}";}
    o<<"],\"max_outstanding_writes_per_peer\":"<<metrics.maxOutstandingWrites<<"}\n";
}
}
