#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
// Process-local storage only. Uses existing save serialization without disk I/O.
class Preferences {
    std::string space_;
    bool readOnly_ = true;
    static std::map<std::string,std::vector<uint8_t>>& storage() {
        static std::map<std::string,std::vector<uint8_t>> values;
        return values;
    }
    std::string key(const char* name) const { return space_+":"+name; }
public:
    bool begin(const char* name, bool readOnly=false) { space_=name; readOnly_=readOnly; return true; }
    void end() { space_.clear(); }
    size_t getBytesLength(const char* name) { auto i=storage().find(key(name)); return i==storage().end()?0:i->second.size(); }
    size_t getBytes(const char* name, void* out, size_t size) {
        auto i=storage().find(key(name)); if(i==storage().end() || i->second.size()>size) return 0;
        std::memcpy(out,i->second.data(),i->second.size()); return i->second.size();
    }
    size_t putBytes(const char* name, const void* data, size_t size) {
        if(readOnly_ || space_.empty() || size>65536 || storage().size()>16) return 0;
        auto bytes=static_cast<const uint8_t*>(data);
        storage()[key(name)]=std::vector<uint8_t>(bytes,bytes+size); return size;
    }
    bool getBool(const char* name, bool fallback=false) { uint8_t value; return getBytes(name,&value,1)==1 ? value!=0 : fallback; }
    size_t putBool(const char* name, bool value) { uint8_t v=value; return putBytes(name,&v,1); }
};
