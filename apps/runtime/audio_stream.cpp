#include "audio_stream.h"
#include "runtime.h"
#include <limits>
namespace pfme { namespace runtime {
namespace {
constexpr std::size_t AUDIO_HISTORY=512;
struct Ring {uint64_t sequence=0;uint16_t frequency=0,duty=512;std::array<AudioEvent,AUDIO_HISTORY> events{};};
Ring rings[NATIVE_CONTROLLER_CAPACITY];
}
void emitNativeAudio(PlayerID id,uint16_t frequency,uint16_t duty) {
    if(!isValidController(id))return;
    auto& ring=rings[id];
    const bool unchanged=ring.frequency==frequency && (!frequency || ring.duty==duty);
    ring.frequency=frequency;ring.duty=duty;
    if(unchanged)return;
    if(ring.sequence==std::numeric_limits<uint64_t>::max())return;
    const auto sequence=++ring.sequence;
    ring.events[(sequence-1)%AUDIO_HISTORY]={sequence,audioTime(),frequency,duty};
}
void resetAudioStream(PlayerID id) {if(isValidController(id))rings[id]=Ring{};}
AudioBatch readAudioStream(PlayerID id,uint64_t since,bool fresh) {
    AudioBatch out;out.now=audioTime();if(!isValidController(id))return out;
    const auto& ring=rings[id];out.cursor=ring.sequence;out.frequency=ring.frequency;out.duty=ring.duty;
    // Never build an outgoing queue of stale notes. A delayed/reconnected
    // listener resynchronizes to the current oscillator state instead.
    if(fresh || since>ring.sequence || ring.sequence-since>MAX_AUDIO_EVENTS)return out;
    out.reset=false;
    for(uint64_t sequence=since;sequence<ring.sequence;) {
        ++sequence;out.events[out.count++]=ring.events[(sequence-1)%AUDIO_HISTORY];
    }
    return out;
}
}}
