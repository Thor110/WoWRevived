#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>

// Manual Linker Exports: This forces the exact string the game is hunting for.
#pragma comment(linker, "/export:_SmackUseMMX@4=SmackUseMMX")
#pragma comment(linker, "/export:_SmackOpen@12=SmackOpen")
#pragma comment(linker, "/export:_SmackClose@4=SmackClose")
#pragma comment(linker, "/export:_SmackBlitOpen@16=SmackBlitOpen")
#pragma comment(linker, "/export:_SmackBlitClose@4=SmackBlitClose")
#pragma comment(linker, "/export:_SmackToBuffer@28=SmackToBuffer")
#pragma comment(linker, "/export:_SmackDoFrame@4=SmackDoFrame")
#pragma comment(linker, "/export:_SmackNextFrame@4=SmackNextFrame")
#pragma comment(linker, "/export:_SmackWait@4=SmackWait")
#pragma comment(linker, "/export:_SmackDDSurfaceType@4=SmackDDSurfaceType")
#pragma comment(linker, "/export:_SmackSoundUseDirectSound@4=SmackSoundUseDirectSound")

FILE* logFile = nullptr;

void Log(const char* fmt, ...) {
    if (!logFile) logFile = fopen("smack_bink_log.txt", "a");
    if (!logFile) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    fprintf(logFile, "\n");
    fflush(logFile);
    va_end(args);
}

// We use __stdcall (WINAPI) to match the @X byte counts perfectly
extern "C" {
    void WINAPI SmackClose(void* smk) { Log("SmackClose"); }
    void WINAPI SmackBlitClose(void* smk) { Log("SmackBlitClose"); }
    void WINAPI SmackWait(void* smk) { Log("SmackWait"); }
    void WINAPI SmackBlitOpen(void* smk, void* dds, DWORD x, DWORD y) { Log("SmackBlitOpen: x=%d, y=%d", x, y); }
    void WINAPI SmackDDSurfaceType(void* lpDDS) { Log("SmackDDSurfaceType"); }
    void* WINAPI SmackOpen(const char* name, DWORD flags, DWORD extra) {
        Log("SmackOpen: %s", name);
        return (void*)0xDEADBEEF;
    }
    void WINAPI SmackUseMMX(DWORD flag) { Log("SmackUseMMX: %d", flag); }
    void WINAPI SmackSoundUseDirectSound(void* ds) { Log("SmackSoundUseDS"); }
    void WINAPI SmackNextFrame(void* smk) {}
    void WINAPI SmackDoFrame(void* smk) {}
    void WINAPI SmackToBuffer(void* smk, DWORD l, DWORD t, DWORD p, DWORD h, void* buf, DWORD f) {}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log("--- smackw32 wrapper initialized via Pragma ---");
    }
    return TRUE;
}