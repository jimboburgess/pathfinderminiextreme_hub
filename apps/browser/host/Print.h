#pragma once
#include "Arduino.h"
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    size_t write(const char* text) {
        size_t n = 0;
        while (*text) n += write(static_cast<uint8_t>(*text++));
        return n;
    }
    size_t print(const char* text) { return write(text); }
    size_t println(const char* text) { return write(text) + write("\r\n"); }
};
