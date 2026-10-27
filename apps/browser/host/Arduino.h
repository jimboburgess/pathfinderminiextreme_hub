#pragma once
// Native-only compatibility surface. Never added to PlatformIO include paths.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
using std::sin;
using std::cos;
inline float radians(float degrees) { return degrees * 0.01745329251994329577f; }
#define ARDUINO 100
#define PROGMEM
#define pgm_read_word(p) (*(const uint16_t*)(p))
using String = std::string;
class __FlashStringHelper;
inline void yield() {}
