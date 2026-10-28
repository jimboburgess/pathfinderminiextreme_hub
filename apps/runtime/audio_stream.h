#pragma once
#include "multiplayer/multiplayer_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace pfme { namespace runtime {
constexpr std::size_t MAX_AUDIO_EVENTS=32;
struct AudioEvent {uint64_t sequence=0;uint32_t at=0;uint16_t frequency=0,duty=512;};
struct AudioBatch {
    uint64_t cursor=0;uint32_t now=0;uint16_t frequency=0,duty=512;bool reset=true;
    std::size_t count=0;std::array<AudioEvent,MAX_AUDIO_EVENTS> events{};
};
uint32_t audioTime();
void emitNativeAudio(PlayerID,uint16_t frequency,uint16_t duty);
void resetAudioStream(PlayerID);
AudioBatch readAudioStream(PlayerID,uint64_t since,bool fresh);
}}
