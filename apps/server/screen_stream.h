#pragma once
#include "protocol.h"
#include <array>
#include <vector>
namespace pfme::server {
// One immutable pending frame; no queue. Only the authenticated owner receives it.
class ScreenStream {
    std::array<uint16_t,57600> previous_{}, snapshot_{};
    std::vector<uint8_t> pending_;
    uint64_t revision_=0;
    size_t offset_=0;
public:
    void reset() { previous_.fill(0); snapshot_.fill(0); pending_.clear(); revision_=0; offset_=0; }
    bool next(const uint16_t* pixels, uint64_t since, size_t offset, json::object& out);
};
}
