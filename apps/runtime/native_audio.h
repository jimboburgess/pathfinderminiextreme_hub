#pragma once
#include "audio_stream.h"
enum class SoundEffect : uint8_t;
enum class AudioDuty : uint8_t;
namespace pfme { namespace runtime {
void selectAudioController(PlayerID);
PlayerID audioController();
void updateControllerAudio(PlayerID,uint32_t now);
void resetControllerAudio(PlayerID);
bool broadcastNativeSound(SoundEffect);
bool broadcastNativeTones(const uint16_t*,uint8_t,uint16_t,uint16_t,AudioDuty,bool& result);
}}
