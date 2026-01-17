#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <ddraw.h>
#include <cstdio>
#pragma comment(lib, "dxguid.lib")
FILE* logFile = nullptr;

// === Logging ===
void Log(const char* fmt, ...)
{
    if (!logFile) logFile = fopen("ddraw_log.txt", "w");
    if (!logFile) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    fprintf(logFile, "\n");
    fflush(logFile);
    va_end(args);
}

// === Our Hooks ===
HRESULT WINAPI MySetDisplayMode(IDirectDraw7* self, DWORD w, DWORD h, DWORD bpp, DWORD refresh, DWORD flags)
{
    Log("SetDisplayMode: %ux%u @ %ubpp", w, h, bpp);
    if (bpp > 16) bpp = 16; // Force 8 or 16-bit only
    return self->SetDisplayMode(w, h, bpp, refresh, flags);
}

// === Patch VTable for IDirectDraw7 ===
void PatchVTable(IDirectDraw7* dd7)
{
    void** vtbl = *(void***)dd7;
    Log("Patching vtable: %p", vtbl);

    // Patch SetDisplayMode (index 12)
    DWORD oldProtect;
    VirtualProtect(&vtbl[12], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtbl[12] = (void*)&MySetDisplayMode;
    VirtualProtect(&vtbl[12], sizeof(void*), oldProtect, &oldProtect);
}

// === Forwarder: DirectDrawCreate ===
extern "C" HRESULT WINAPI DirectDrawCreate(GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
    Log("DirectDrawCreate called");

    static HMODULE realDDraw = LoadLibraryA("ddraw_orig.dll");
    if (!realDDraw)
    {
        DWORD err = GetLastError();
        Log("Failed to load ddraw_orig.dll. GetLastError = %lu", err);
        return E_FAIL;
    }

    auto realFunc = (decltype(&DirectDrawCreate))GetProcAddress(realDDraw, "DirectDrawCreate");
    if (!realFunc)
    {
        Log("Failed to get address of DirectDrawCreate");
        return E_FAIL;
    }

    HRESULT hr = realFunc(lpGUID, lplpDD, pUnkOuter);
    if (FAILED(hr))
    {
        Log("DirectDrawCreate failed: HRESULT 0x%08lX", hr);
        return hr;
    }

    IDirectDraw7* dd7 = nullptr;
    if (SUCCEEDED((*lplpDD)->QueryInterface(IID_IDirectDraw7, (void**)&dd7)))
    {
        PatchVTable(dd7);
        dd7->Release();
        Log("IDirectDraw7 vtable patched successfully");
    }
    else
    {
        Log("IDirectDraw7 not supported");
    }

    return hr;
}

// === Clean Exit ===
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH && logFile)
    {
        fclose(logFile);
        logFile = nullptr;
    }
    return TRUE;
}
