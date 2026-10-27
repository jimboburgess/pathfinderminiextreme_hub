#pragma once
#include <cstdint>
#include <string>
namespace pfme { namespace runtime {
// Original handheld globals permit one runtime per process, one owning session.
void initialize(uint32_t seed);
void advance(uint32_t milliseconds);
bool input(const std::string& action, bool down);
void releaseInputs();
const uint16_t* pixels();
std::string diagnostic(); // Local tests only; never sent to other participants.
}}
