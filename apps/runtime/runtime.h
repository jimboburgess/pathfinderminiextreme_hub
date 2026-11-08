#pragma once
#include <cstdint>
#include "controller_capacity.h"
#include <string>
namespace pfme { namespace runtime {
struct ControllerState;
// Internal migration API. The authenticated Service still retains its guard
// until gameplay, visibility and combat routing are verified end to end.
void initializeControllers(uint32_t seed,bool socialParties=false);
bool bindController(uint8_t id,ControllerState& state);
void setControllerConnected(uint8_t id,bool connected);
bool unbindController(uint8_t id);
bool selectController(uint8_t id);
bool controllerInput(uint8_t id,const std::string& action,bool down);
const uint16_t* controllerPixels(uint8_t id);
uint32_t worldTime();
uint8_t nativePartyPlayerID(uint8_t);

// Original handheld globals permit one runtime per process, one owning session.
void initialize(uint32_t seed);
void advance(uint32_t milliseconds);
bool input(const std::string& action, bool down);
bool textInputActive();
void releaseInputs();
const uint16_t* pixels();
std::string diagnostic(); // Local tests only; never sent to other participants.
}}
