#pragma once
#include "Arduino.h"
#include <type_traits>
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    size_t write(const char* text) { size_t n=0; while (*text) n+=write(static_cast<uint8_t>(*text++)); return n; }
    size_t print(const char* text) { return write(text); }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(const std::string& text) { return write(text.c_str()); }
    template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    size_t print(T value) { return print(std::to_string(value)); }
    template<class T> size_t println(const T& value) { return print(value)+write("\r\n"); }
    size_t println() { return write("\r\n"); }
};
