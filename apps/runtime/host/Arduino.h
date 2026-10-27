#pragma once
#include "../../browser/host/Arduino.h"
#include <algorithm>
#include <cstdio>
using std::min;
using std::max;
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT_PULLUP = 2;
unsigned long millis();
void delay(unsigned long milliseconds);
long random(long upper);
long random(long lower, long upper);
void randomSeed(unsigned long seed);
uint32_t esp_random();
int digitalRead(int pin);
void digitalWrite(int pin, int value);
void pinMode(int pin, int mode);
inline void* ps_malloc(size_t size) { return std::malloc(size); }
inline double ledcSetup(int, double frequency, int) { return frequency; }
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int, int) {}
inline double ledcWriteTone(int, double frequency) { return frequency; }
struct NativeSerial {
    void begin(int) {}
    template<class... T> void print(T&&...) {}
    template<class... T> void println(T&&...) {}
    template<class... T> void printf(T&&...) {}
};
extern NativeSerial Serial;
