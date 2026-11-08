#pragma once

#include "Arduino.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef ERROR
#endif

#include "CharacterFiles.h"

namespace pfme { namespace runtime { uint8_t selectedController(); } }

class Preferences
{
    std::string space_;
    unsigned owner_ = 255;
    std::string account_;
    bool readOnly_ = true;

    static std::map<std::string, std::vector<uint8_t>>& storage()
    {
        static std::map<std::string, std::vector<uint8_t>> values;
        return values;
    }

    std::string key(const char* name) const
    {
        return std::to_string(owner_) + ":" + space_ + ":" + name;
    }

    bool isSaveKey(const char* name) const
    {
        return space_ == "pathfinder" && name != nullptr &&
               (std::strcmp(name, "player") == 0 ||
                std::strcmp(name, "player0") == 0 ||
                std::strcmp(name, "player1") == 0 ||
                std::strcmp(name, "player2") == 0);
    }

    static std::filesystem::path saveRoot()
    {
        if (const char* overridePath = std::getenv("PFME_SAVE_DIR"))
        {
            if (*overridePath != '\0')
                return overridePath;
        }

#ifdef _WIN32
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
            return std::filesystem::path(localAppData) /
                   "PathfinderMiniExtreme";
        if (const char* appData = std::getenv("APPDATA"))
            return std::filesystem::path(appData) /
                   "PathfinderMiniExtreme";
#else
        if (const char* dataHome = std::getenv("XDG_DATA_HOME"))
            return std::filesystem::path(dataHome) /
                   "pathfinder-mini-extreme";
        if (const char* home = std::getenv("HOME"))
            return std::filesystem::path(home) / ".local" / "share" /
                   "pathfinder-mini-extreme";
#endif
        return std::filesystem::temp_directory_path() /
               "pathfinder-mini-extreme";
    }

    std::filesystem::path savePath(const char* name) const
    {
        std::filesystem::path directory = saveRoot();
        if (owner_ != 255)
            directory /= "controller_" + std::to_string(owner_);

        if (std::strcmp(name, "player0") == 0)
            return directory / "save_slot_1.dat";
        if (std::strcmp(name, "player1") == 0)
            return directory / "save_slot_2.dat";
        if (std::strcmp(name, "player2") == 0)
            return directory / "save_slot_3.dat";
        return directory / "save.dat";
    }

    static bool replaceFile(const std::filesystem::path& temporary,
                            const std::filesystem::path& destination)
    {
#ifdef _WIN32
        return MoveFileExW(temporary.c_str(), destination.c_str(),
                           MOVEFILE_REPLACE_EXISTING |
                           MOVEFILE_WRITE_THROUGH) != 0;
#else
        return std::rename(temporary.c_str(), destination.c_str()) == 0;
#endif
    }

public:
    bool begin(const char* name, bool readOnly = false)
    {
        owner_ = pfme::runtime::selectedController();
        account_ = CharacterFiles::owner(owner_);
        space_ = name != nullptr ? name : "";
        if(space_=="pathfinder" && CharacterFiles::required() && account_.empty())return false;
        readOnly_ = readOnly;
        return !space_.empty();
    }

    void end()
    {
        space_.clear();
    }

    size_t getBytesLength(const char* name)
    {
        if(isSaveKey(name)&&!account_.empty())return CharacterFiles::length(account_,name);
        if (isSaveKey(name))
        {
            std::error_code error;
            const auto size = std::filesystem::file_size(savePath(name), error);
            return error ? 0 : static_cast<size_t>(size);
        }

        auto stored = storage().find(key(name));
        return stored == storage().end() ? 0 : stored->second.size();
    }

    size_t getBytes(const char* name, void* out, size_t size)
    {
        if (out == nullptr)
            return 0;

        if(isSaveKey(name)&&!account_.empty())return CharacterFiles::read(account_,name,out,size);
        if (isSaveKey(name))
        {
            const size_t storedSize = getBytesLength(name);
            if (storedSize == 0 || storedSize > size)
                return 0;
            std::ifstream input(savePath(name), std::ios::binary);
            input.read(static_cast<char*>(out),
                       static_cast<std::streamsize>(storedSize));
            return input && static_cast<size_t>(input.gcount()) == storedSize
                ? storedSize : 0;
        }

        auto stored = storage().find(key(name));
        if (stored == storage().end() || stored->second.size() > size)
            return 0;
        std::memcpy(out, stored->second.data(), stored->second.size());
        return stored->second.size();
    }

    size_t putBytes(const char* name, const void* data, size_t size)
    {
        if (readOnly_ || space_.empty() || data == nullptr || size > 65536)
            return 0;

        if(isSaveKey(name)&&!account_.empty())return CharacterFiles::write(account_,name,data,size);
        if (isSaveKey(name))
        {
            const std::filesystem::path destination = savePath(name);
            const std::filesystem::path temporary = destination.string() + ".tmp";
            std::error_code error;
            std::filesystem::create_directories(destination.parent_path(), error);
            if (error)
                return 0;

            {
                std::ofstream output(temporary,
                                     std::ios::binary | std::ios::trunc);
                output.write(static_cast<const char*>(data),
                             static_cast<std::streamsize>(size));
                output.flush();
                if (!output)
                    return 0;
            }

            if (!replaceFile(temporary, destination))
            {
                std::filesystem::remove(temporary, error);
                return 0;
            }
            return size;
        }

        if (storage().size() > 16)
            return 0;
        const auto* bytes = static_cast<const uint8_t*>(data);
        storage()[key(name)] = std::vector<uint8_t>(bytes, bytes + size);
        return size;
    }

    bool remove(const char* name)
    {
        if (readOnly_ || space_.empty())
            return false;
        if(isSaveKey(name)&&!account_.empty())return CharacterFiles::remove(account_,name);
        if (isSaveKey(name))
        {
            std::error_code error;
            return std::filesystem::remove(savePath(name), error) && !error;
        }
        return storage().erase(key(name)) != 0;
    }

    static void retireOwner(unsigned owner)
    {
        CharacterFiles::retire(owner);
        auto& values = storage();
        const auto prefix = std::to_string(owner) + ":";
        for (auto entry = values.begin(); entry != values.end();)
        {
            if (entry->first.rfind(prefix, 0) == 0)
                entry = values.erase(entry);
            else
                ++entry;
        }
    }

    bool getBool(const char* name, bool fallback = false)
    {
        uint8_t value = 0;
        return getBytes(name, &value, 1) == 1 ? value != 0 : fallback;
    }

    size_t putBool(const char* name, bool value)
    {
        const uint8_t stored = value;
        return putBytes(name, &stored, 1);
    }
};
