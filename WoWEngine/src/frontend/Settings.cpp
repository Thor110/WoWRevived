#include "frontend/Settings.h"

// IniStore.h includes <windows.h> on Windows, so keep that header's macros to a minimum
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "IniStore.h"

#include <SDL3/SDL_log.h>

#include <cstdio>
#include <cstdlib>

namespace wow
{
    Settings::Settings(const GameFiles& files) : ini(std::make_unique<IniStore>())
    {
        // the file is created on the first change if it isn't there yet
        auto existing = files.Resolve(IniStore::FileName());
        ini->Load((existing ? *existing : files.Root() / IniStore::FileName()).string());
    }

    Settings::~Settings() = default;

    std::string Settings::GetString(const char* section, const char* name, const std::string& fallback) const
    {
        std::string value;
        return ini->Get(section, name, value) ? value : fallback;
    }

    int Settings::GetInt(const char* section, const char* name, int fallback) const
    {
        std::string value;
        return ini->Get(section, name, value) && !value.empty() ? std::atoi(value.c_str()) : fallback;
    }

    float Settings::GetFloat(const char* section, const char* name, float fallback) const
    {
        std::string value;
        return ini->Get(section, name, value) && !value.empty() ? float(std::atof(value.c_str())) : fallback;
    }

    void Settings::SetString(const char* section, const char* name, const std::string& value)
    {
        ini->Set(section, name, value);
        if (!ini->Save()) SDL_Log("Couldn't save %s", IniStore::FileName());
    }

    void Settings::SetInt(const char* section, const char* name, int value) { SetString(section, name, std::to_string(value)); }

    void Settings::SetFloat(const char* section, const char* name, float value)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%f", double(value)); // the same "1.000000" format the game writes
        SetString(section, name, text);
    }
}
