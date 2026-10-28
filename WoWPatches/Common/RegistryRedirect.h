// Serves the game's registry key from WoWRevived.ini instead of the registry, making the installation portable.
//
// Both game builds use only six ADVAPI32 functions, and only through their own import table. InstallRegistryRedirect
// swaps those six import table entries (in the game executable only, so DirectPlay and Windows itself are unaffected)
// for the functions below. Calls for HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000 and its
// subkeys are answered from the store, everything else goes to the real registry.
//
// Every value is returned as REG_SZ text. Both builds read every setting from text (their readers fall back to
// atoi/atof/sscanf for anything that isn't a DWORD), so the vanilla/network DWORD-versus-string differences that
// the launcher used to convert between no longer matter. The vanilla build's string-only settings can't read
// DWORDs at all, which is what used to break it after running the network build.
//
// Special values in the 1.00.000 key:
//   CD Path / Install Path  always the game's own folder; the game's writes and deletes are ignored
//   Use INI                 never present: when "1" the game would switch some settings to its own unfinished INI mode
#pragma once
#include "IniStore.h"
#include <cstdint>
#include <set>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace wow
{
    namespace registry
    {
        static const char* const kRootPath = "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000";

        struct FakeKey { uint32_t magic; std::string section; };
        static const uint32_t kFakeKeyMagic = 0x57525649; // "WRVI"

        struct State {
            IniStore* store = nullptr;
            std::string gameDir; // with a trailing backslash
            std::mutex keysMutex;
            std::set<FakeKey*> keys;
            // the real functions, for every call that isn't for the game's key
            decltype(&RegOpenKeyExA) realOpen = &RegOpenKeyExA;
            decltype(&RegCreateKeyExA) realCreate = &RegCreateKeyExA;
            decltype(&RegQueryValueExA) realQuery = &RegQueryValueExA;
            decltype(&RegSetValueExA) realSet = &RegSetValueExA;
            decltype(&RegDeleteValueA) realDelete = &RegDeleteValueA;
            decltype(&RegCloseKey) realClose = &RegCloseKey;
        };
        inline State& GetState() { static State state; return state; }

        inline bool IEquals(const std::string& a, const char* b)
        {
            size_t n = strlen(b);
            if (a.size() != n) return false;
            for (size_t i = 0; i < n; i++) {
                char x = a[i], y = b[i];
                if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
                if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
                if (x != y) return false;
            }
            return true;
        }

        inline FakeKey* AsFake(HKEY key)
        {
            State& state = GetState();
            std::lock_guard<std::mutex> lock(state.keysMutex);
            FakeKey* fake = (FakeKey*)key;
            return state.keys.count(fake) ? fake : nullptr;
        }

        inline std::string TrimSlashes(std::string s)
        {
            while (!s.empty() && s.front() == '\\') s.erase(0, 1);
            while (!s.empty() && s.back() == '\\') s.pop_back();
            return s;
        }

        // Works out which section a key path refers to. Returns false if it isn't the game's key.
        inline bool ResolveSection(HKEY parent, LPCSTR subKey, std::string& section)
        {
            std::string sub = TrimSlashes(subKey ? subKey : "");
            if (FakeKey* fake = AsFake(parent)) {
                if (sub.empty()) section = fake->section;
                else section = IEquals(fake->section, IniStore::RootSection()) ? sub : fake->section + "\\" + sub;
                return true;
            }
            if (parent != HKEY_LOCAL_MACHINE) return false;
            size_t rootLength = strlen(kRootPath);
            if (sub.size() < rootLength || !IEquals(sub.substr(0, rootLength), kRootPath)) return false;
            std::string rest = sub.substr(rootLength);
            if (!rest.empty() && rest[0] != '\\') return false; // e.g. "...\1.00.0001"
            rest = TrimSlashes(rest);
            section = rest.empty() ? IniStore::RootSection() : rest;
            return true;
        }

        inline bool KeyExists(const std::string& section)
        {
            IniStore& store = *GetState().store;
            if (IEquals(section, IniStore::RootSection())) return store.HasAnySection();
            return store.KeyExists(section);
        }

        inline HKEY NewFakeKey(const std::string& section)
        {
            State& state = GetState();
            FakeKey* fake = new FakeKey{ kFakeKeyMagic, section };
            std::lock_guard<std::mutex> lock(state.keysMutex);
            state.keys.insert(fake);
            return (HKEY)fake;
        }

        inline bool IsInstallPathValue(const std::string& section, const std::string& name)
        {
            return IEquals(section, IniStore::RootSection()) && (IEquals(name, "CD Path") || IEquals(name, "Install Path"));
        }
        inline bool IsHiddenValue(const std::string& section, const std::string& name)
        {
            return IEquals(section, IniStore::RootSection()) && (IEquals(name, "Use INI") || IEquals(name, "Autoexec install path"));
        }

        inline LONG WINAPI OpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
        {
            std::string section;
            if (!ResolveSection(hKey, lpSubKey, section)) return GetState().realOpen(hKey, lpSubKey, ulOptions, samDesired, phkResult);
            if (!phkResult) return ERROR_INVALID_PARAMETER;
            if (!KeyExists(section)) return ERROR_FILE_NOT_FOUND; // the game then writes its default value
            *phkResult = NewFakeKey(section);
            return ERROR_SUCCESS;
        }

        inline LONG WINAPI CreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired,
            const LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
        {
            std::string section;
            if (!ResolveSection(hKey, lpSubKey, section))
                return GetState().realCreate(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
            if (!phkResult) return ERROR_INVALID_PARAMETER;
            bool existed = KeyExists(section);
            GetState().store->EnsureSection(section);
            if (lpdwDisposition) *lpdwDisposition = existed ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
            *phkResult = NewFakeKey(section);
            return ERROR_SUCCESS;
        }

        inline LONG WINAPI QueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
        {
            FakeKey* fake = AsFake(hKey);
            if (!fake) return GetState().realQuery(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
            if (lpData && !lpcbData) return ERROR_INVALID_PARAMETER;
            std::string name = lpValueName ? lpValueName : "";
            std::string value;
            if (IsInstallPathValue(fake->section, name)) value = GetState().gameDir;
            else if (IsHiddenValue(fake->section, name) || !GetState().store->Get(fake->section, name, value)) return ERROR_FILE_NOT_FOUND;
            DWORD size = (DWORD)value.size() + 1; // including the terminating null, like the registry
            if (lpType) *lpType = REG_SZ;
            if (!lpData) {
                if (lpcbData) *lpcbData = size;
                return ERROR_SUCCESS;
            }
            if (*lpcbData < size) {
                *lpcbData = size;
                return ERROR_MORE_DATA;
            }
            memcpy(lpData, value.c_str(), size);
            *lpcbData = size;
            return ERROR_SUCCESS;
        }

        inline LONG WINAPI SetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)
        {
            FakeKey* fake = AsFake(hKey);
            if (!fake) return GetState().realSet(hKey, lpValueName, Reserved, dwType, lpData, cbData);
            std::string name = lpValueName ? lpValueName : "";
            if (IsInstallPathValue(fake->section, name) || IsHiddenValue(fake->section, name)) return ERROR_SUCCESS; // ignored
            std::string text;
            if (dwType == REG_DWORD && lpData && cbData >= 4) {
                int32_t number;
                memcpy(&number, lpData, 4);
                text = std::to_string(number); // signed, the game's integer settings can be negative
            }
            else if (dwType == REG_QWORD && lpData && cbData >= 8) {
                int64_t number;
                memcpy(&number, lpData, 8);
                text = std::to_string(number);
            }
            else if (lpData) {
                const char* chars = (const char*)lpData;
                size_t length = 0;
                while (length < cbData && chars[length] != '\0') length++;
                text.assign(chars, length);
            }
            GetState().store->Set(fake->section, name, text);
            return ERROR_SUCCESS;
        }

        inline LONG WINAPI DeleteValueA(HKEY hKey, LPCSTR lpValueName)
        {
            FakeKey* fake = AsFake(hKey);
            if (!fake) return GetState().realDelete(hKey, lpValueName);
            std::string name = lpValueName ? lpValueName : "";
            if (IsInstallPathValue(fake->section, name)) return ERROR_SUCCESS; // the game clears these on exit, they're not stored
            return GetState().store->Delete(fake->section, name) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
        }

        inline LONG WINAPI CloseKey(HKEY hKey)
        {
            FakeKey* fake = AsFake(hKey);
            if (!fake) return GetState().realClose(hKey);
            State& state = GetState();
            {
                std::lock_guard<std::mutex> lock(state.keysMutex);
                state.keys.erase(fake);
            }
            delete fake;
            state.store->Save(); // only writes if something changed; the game closes the key after each change
            return ERROR_SUCCESS;
        }

#ifdef _WIN32
        // Replaces the six ADVAPI32 registry imports of the game executable. Returns how many were redirected.
        inline int InstallRedirect(IniStore* store, const std::string& gameDir)
        {
            State& state = GetState();
            state.store = store;
            state.gameDir = gameDir;
            BYTE* base = (BYTE*)GetModuleHandleA(NULL);
            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
            IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
            IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!dir.VirtualAddress) return 0;
            struct Replacement { const char* name; void* function; void** real; };
            Replacement replacements[] = {
                { "RegOpenKeyExA", (void*)&OpenKeyExA, (void**)&state.realOpen },
                { "RegCreateKeyExA", (void*)&CreateKeyExA, (void**)&state.realCreate },
                { "RegQueryValueExA", (void*)&QueryValueExA, (void**)&state.realQuery },
                { "RegSetValueExA", (void*)&SetValueExA, (void**)&state.realSet },
                { "RegDeleteValueA", (void*)&DeleteValueA, (void**)&state.realDelete },
                { "RegCloseKey", (void*)&CloseKey, (void**)&state.realClose },
            };
            int redirected = 0;
            for (IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; imp++) {
                if (_stricmp((const char*)(base + imp->Name), "ADVAPI32.dll") != 0) continue;
                if (!imp->OriginalFirstThunk) continue; // no names to match against
                IMAGE_THUNK_DATA* names = (IMAGE_THUNK_DATA*)(base + imp->OriginalFirstThunk);
                IMAGE_THUNK_DATA* slots = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
                for (; names->u1.AddressOfData; names++, slots++) {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                    const char* name = (const char*)((IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData))->Name;
                    for (Replacement& r : replacements) {
                        if (strcmp(name, r.name) != 0) continue;
                        DWORD oldProtect;
                        if (!VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &oldProtect)) break;
                        *r.real = (void*)slots->u1.Function; // keep whatever the import resolved to for everything else
                        slots->u1.Function = (ULONG_PTR)r.function;
                        VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), oldProtect, &oldProtect);
                        redirected++;
                        break;
                    }
                }
            }
            return redirected;
        }
#endif
    }
}
