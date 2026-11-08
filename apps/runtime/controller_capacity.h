#pragma once
#include <cstdint>
namespace pfme { namespace runtime {
// Server account/controller slots are NOT party-local PlayerIDs (which stay 0..3).
using ControllerID=uint8_t;
constexpr unsigned NATIVE_CONTROLLER_CAPACITY=20;
constexpr ControllerID INVALID_CONTROLLER=255;
inline bool isValidController(unsigned id){return id<NATIVE_CONTROLLER_CAPACITY;}
}}
