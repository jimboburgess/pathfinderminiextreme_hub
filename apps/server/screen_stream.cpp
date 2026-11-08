#include "screen_stream.h"
#include <algorithm>
#include <limits>
namespace pfme::server {
namespace {
std::string base64(const uint8_t* data,size_t size) {
    static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result; result.reserve((size+2)/3*4);
    for(size_t i=0;i<size;i+=3) {
        const uint32_t value=(uint32_t(data[i])<<16) | (i+1<size?uint32_t(data[i+1])<<8:0) | (i+2<size?data[i+2]:0);
        result.push_back(alphabet[(value>>18)&63]); result.push_back(alphabet[(value>>12)&63]);
        result.push_back(i+1<size?alphabet[(value>>6)&63]:'='); result.push_back(i+2<size?alphabet[value&63]:'=');
    }
    return result;
}
}
bool ScreenStream::next(const uint16_t* pixels,uint64_t since,size_t offset,json::object& out) {
    if(!cursorValid(since,offset)) return false;
    if(pending_.empty()) {
        std::copy(pixels,pixels+57600,snapshot_.begin());
        auto word=[&](uint16_t v) { pending_.push_back(uint8_t(v)); pending_.push_back(uint8_t(v>>8)); };
        for(size_t i=0;i<57600;) {
            if(previous_[i]==snapshot_[i]) { ++i; continue; }
            const size_t first=i;
            auto repeated=[&](size_t at) {
                return at+2<57600 && snapshot_[at]==snapshot_[at+1] && snapshot_[at]==snapshot_[at+2] &&
                    previous_[at+1]!=snapshot_[at+1] && previous_[at+2]!=snapshot_[at+2];
            };
            if(repeated(i)) {
                const uint16_t color=snapshot_[i];
                while(i<57600 && i-first<32767 && previous_[i]!=snapshot_[i] && snapshot_[i]==color) ++i;
                word(static_cast<uint16_t>(first));word(static_cast<uint16_t>(0x8000|(i-first)));word(color);
            } else {
                ++i;
                while(i<57600 && i-first<32767 && previous_[i]!=snapshot_[i] && !repeated(i)) ++i;
                word(static_cast<uint16_t>(first));word(static_cast<uint16_t>(i-first));
                for(size_t p=first;p<i;++p) word(snapshot_[p]);
            }
        }
    }
    const size_t count=std::min(size_t(9216),pending_.size()-offset_);
    const bool more=offset_+count<pending_.size();
    const bool changed=!pending_.empty();
    out={{"encoding","rgb565-patch-rle1"},{"base",std::to_string(revision_)},{"frame",std::to_string(revision_+(changed?1:0))},
         {"offset",offset_},{"total",pending_.size()},{"more",more},
         {"pixels",count?base64(pending_.data()+offset_,count):std::string()}};
    offset_+=count;
    if(!more) {
        if(changed) { previous_=snapshot_; ++revision_; }
        offset_=0; pending_.clear();
    }
    return true;
}
}
