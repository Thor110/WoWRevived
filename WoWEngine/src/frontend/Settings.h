// The game's settings, kept in WoWRevived.ini in the game folder like the rest of WoW Revived (the same file and
// format as the launcher and the patch DLLs, through the shared IniStore.h). Section and value names are the ones
// the game itself uses ("Sound\Volume" / "Master", "Options" / "Show lights"...), so existing installs carry over.
// Every change is saved straight away.
#pragma once
#include "core/GameFiles.h"

#include <memory>
#include <string>

namespace wow
{
    class IniStore;

    class Settings
    {
    public:
        explicit Settings(const GameFiles& files);
        ~Settings();

        std::string GetString(const char* section, const char* name, const std::string& fallback) const;
        int GetInt(const char* section, const char* name, int fallback) const;
        float GetFloat(const char* section, const char* name, float fallback) const;
        bool GetBool(const char* section, const char* name, bool fallback) const { return GetInt(section, name, fallback ? 1 : 0) != 0; }

        void SetString(const char* section, const char* name, const std::string& value);
        void SetInt(const char* section, const char* name, int value);
        void SetFloat(const char* section, const char* name, float value);
        void SetBool(const char* section, const char* name, bool value) { SetInt(section, name, value ? 1 : 0); }

    private:
        std::unique_ptr<IniStore> ini;
    };
}
