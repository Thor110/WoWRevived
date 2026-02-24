#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <string>

FILE* logFile = nullptr;

bool debug = true; // true for logging

// === Logging === //
void Log(const char* fmt, ...)
{
    if (!debug) return;
    if (!logFile) logFile = fopen("smack_bink_log.txt", "a");
    if (!logFile) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    fprintf(logFile, "\n");
    fflush(logFile);
    va_end(args);
}

struct FakeSmack {
    uint32_t Version;
    uint32_t Width;
    uint32_t Height;
    uint32_t Frames;
    // ... other fields
    int OffsetY = 0;
};

FakeSmack dummy;

// We use __stdcall (WINAPI) to match the @X byte counts perfectly
extern "C" {
    void WINAPI SmackClose(void* smk) { Log("SmackClose"); }
    void WINAPI SmackBlitClose(void* smk) { Log("SmackBlitClose"); }
    void WINAPI SmackWait(void* smk) { Log("SmackWait"); }
    void WINAPI SmackBlitOpen(void* smk) { Log("SmackBlitOpen"); }
    void WINAPI SmackDDSurfaceType(void* lpDDS) { Log("SmackDDSurfaceType"); }

    void* WINAPI SmackOpen(const char* name, DWORD flags, DWORD extra) {
        Log("SmackOpen: %s", name);

        //fix incoming name string .SMK = .mp4
        std::string movie = name;
        movie.resize(movie.length() - 3);

        movie.append("mp4");

        Log("TEST NAME: %s", movie.c_str());

        // Default fallback
        int regWidth = 640;
        int regHeight = 480;

        HKEY hKey;
        // Note: We use HKEY_LOCAL_MACHINE and the path you provided. 
        // Since your app is 32-bit, Windows automatically handles the WOW6432Node redirection.
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Screen", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hKey, "Size", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                // Parse "640,480"
                sscanf(buffer, "%d,%d", &regWidth, &regHeight);
                Log("Registry Resolution Detected: %dx%d", regWidth, regHeight);
            }
            RegCloseKey(hKey);
        }
        // supported reslutions and notes
        //"640x480         (4:3)",    // Classic baseline 4:3                         // Exists In-Game
        //"800x600         (4:3)",    // Legacy 4:3 standard                          // Exists In-Game
        //"1024x768       (4:3)",     // XGA — very common                            // Exists In-Game
        //"1152x864       (4:3)",     // Slightly higher 4:3 (rare)                   // Exists In-Game
        //"1280x768       (15:9)",    // WXGA – rare variant of 1280x800 (15:9)
        //"1280x800       (16:10)",   // WXGA — early widescreen laptops (16:10)      // Exists In-Game
        //"1280x1024     (5:4)",      // SXGA — tall 5:4 monitor resolution
        //"1360x768       (16:9)",    // 16:9 — GPU-aligned, better than 1366x768
        //"1366x768       (16:9)",    // Common 16:9 laptop resolution
        // hacky math for switch cases
        // determine letterboxing arrangement
        switch (regWidth + regHeight)
        {
            case 1120: //640+480=1120
                dummy.OffsetY = 60;
                break;
            case 1400: //800+600=1400
                dummy.OffsetY = 75;
                break;
            case 1792: //1024+768=1792
                dummy.OffsetY = 96;
                break;
            case 2016: //1152+864=2016
                dummy.OffsetY = 108;
                break;
            case 2048: //1280+768=2048
                dummy.OffsetY = 24;
                break;
            case 2080: //1280+800=2080
                dummy.OffsetY = 40;
                break;
            case 2304: //1280+1024=2304
                dummy.OffsetY = 152;
                break;
            case 2128: //1360+768=2128  // clip edges at 1360
            case 2134: //1366+768=2134  // default is already 0
            default: // fail?
        }


        dummy.Width = regWidth;
        dummy.Height = regHeight;
        dummy.Frames = 100; // Provide a dummy frame count so the loop runs

        return &dummy;
    }

    void WINAPI SmackUseMMX(DWORD flag) { Log("SmackUseMMX: %d", flag); }
    void WINAPI SmackSoundUseDirectSound(void* ds) { Log("SmackSoundUseDirectSound"); }
    void WINAPI SmackNextFrame(void* smk) { Log("SmackNextFrame"); }
    int WINAPI SmackDoFrame(void* smk) {
        // Logic to simulate video ending
        static int currentFrame = 0;
        currentFrame++;

        if (currentFrame > 100) { // Simulating a 100-frame video
            currentFrame = 0;
            return 1; // Often 1 or a specific flag signals "End of File" to the engine
        }

        return 0;
    }
    void WINAPI SmackToBuffer(void* smk, DWORD l, DWORD t, DWORD p, DWORD h, void* buf, DWORD f) { Log("SmackToBuffer"); }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DeleteFileA("smack_bink_log.txt");
    }
    return TRUE;
}