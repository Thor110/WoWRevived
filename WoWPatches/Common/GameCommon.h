// Shared helpers for the DLLs that patch the running game (Smackw32.dll and _inmm.dll).
//
// Every address the DLLs touch belongs to one specific build of the game. The vanilla WoW_patched.exe and the
// custom WoW_network.exe have completely different layouts, so before any code is patched the bytes at the
// target are compared against what that build is known to contain. A mismatch skips the patch (and logs it)
// instead of corrupting whatever happens to be at that address.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace wow
{
    enum GameBuild { BUILD_UNKNOWN, BUILD_VANILLA, BUILD_NETWORK };
    enum Language { LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_ES };

    // true if [p, p + n) is committed, readable memory inside one region
    inline bool IsReadable(const void* p, size_t n)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
        return (const BYTE*)p + n <= (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
    }

    inline bool BytesMatch(DWORD address, const BYTE* expected, size_t length)
    {
        const void* p = (const void*)(uintptr_t)address;
        return IsReadable(p, length) && memcmp(p, expected, length) == 0;
    }

    // Writes patch over the code at address, but only if the bytes there are exactly what we expect.
    inline bool PatchIfMatches(DWORD address, const BYTE* expected, size_t expectedLength, const BYTE* patch, size_t patchLength)
    {
        if (!BytesMatch(address, expected, expectedLength)) return false;
        BYTE* target = (BYTE*)(uintptr_t)address;
        DWORD oldProtect;
        if (!VirtualProtect(target, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        memcpy(target, patch, patchLength);
        VirtualProtect(target, patchLength, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), target, patchLength);
        return true;
    }

    // Identifies the running executable from the CD Player menu constructor prologue, which differs between builds.
    // Safe to call from DllMain: the main executable is always mapped before its imported DLLs initialise.
    inline GameBuild DetectGameBuild()
    {
        static const BYTE vanilla[] = { 0x6A, 0xFF, 0x68, 0x27, 0x4F, 0x4A, 0x00 };                   // 0x407990: push -1; push 0x4A4F27
        static const BYTE network[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x9F, 0x17, 0x50, 0x00 }; // 0x4AF680: push ebp; mov ebp,esp; push -1; push 0x50179F
        if (BytesMatch(0x407990, vanilla, sizeof(vanilla))) return BUILD_VANILLA;
        if (BytesMatch(0x4AF680, network, sizeof(network))) return BUILD_NETWORK;
        return BUILD_UNKNOWN;
    }

    inline const char* GameBuildName(GameBuild build)
    {
        return build == BUILD_VANILLA ? "vanilla" : build == BUILD_NETWORK ? "network" : "unknown";
    }

    // Full path of a file next to the game executable.
    inline void GamePath(const char* fileName, char* out, size_t outSize)
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
        _snprintf_s(out, outSize, _TRUNCATE, "%s%s", exePath, fileName);
    }

    inline DWORD FileSizeOf(const char* path)
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0;
        return data.nFileSizeLow;
    }

    // Same rules as WoWLauncher's Program.DetectLanguage: language.txt (shipped with language packs) first,
    // then the size of credits.png, which differs per language. Falls back to English.
    inline Language DetectLanguage()
    {
        char path[MAX_PATH];
        GamePath("language.txt", path, sizeof(path));
        FILE* f = nullptr;
        if (fopen_s(&f, path, "r") == 0 && f)
        {
            char line[32] = {};
            if (fgets(line, sizeof(line), f))
            {
                // trim whitespace and line endings
                size_t len = strlen(line);
                while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t')) line[--len] = '\0';
                char* start = line;
                if ((BYTE)start[0] == 0xEF && (BYTE)start[1] == 0xBB && (BYTE)start[2] == 0xBF) start += 3; // UTF-8 BOM
                while (*start == ' ' || *start == '\t') ++start;
                Language result = LANG_EN;
                bool matched = true;
                if (_stricmp(start, "English") == 0) result = LANG_EN;
                else if (_stricmp(start, "French") == 0) result = LANG_FR;
                else if (_stricmp(start, "German") == 0) result = LANG_DE;
                else if (_stricmp(start, "Italian") == 0) result = LANG_IT;
                else if (_stricmp(start, "Spanish") == 0) result = LANG_ES;
                else matched = false;
                fclose(f);
                if (matched) return result;
            }
            else fclose(f);
        }
        GamePath("credits.png", path, sizeof(path));
        switch (FileSizeOf(path))
        {
            case 292567: return LANG_FR;
            case 294349: return LANG_DE;
            case 258919: return LANG_IT;
            case 268939: return LANG_ES;
            default:     return LANG_EN; // 258221, or unknown
        }
    }
}
