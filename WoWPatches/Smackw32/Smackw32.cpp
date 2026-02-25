#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <mfidl.h>

#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfplat.lib")

FILE* logFile = nullptr;
bool debug = true;
int regWidth = 640;
int regHeight = 480;
int offsetY = 0;
bool videoFinished = false;
HWND overlayWindow = NULL;
IMFPMediaPlayer* pMediaPlayer = NULL;

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

// Media Foundation callback
class MediaPlayerCallback : public IMFPMediaPlayerCallback {
    LONG refCount = 1;
public:
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader) override {
        if (pEventHeader->eEventType == MFP_EVENT_TYPE_MEDIAITEM_CREATED) {
            MFP_MEDIAITEM_CREATED_EVENT* pEvent = MFP_GET_MEDIAITEM_CREATED_EVENT(pEventHeader);
            if (SUCCEEDED(pEventHeader->hrEvent)) {
                pMediaPlayer->SetMediaItem(pEvent->pMediaItem);
                //Log("MFP_EVENT_TYPE_MEDIAITEM_CREATED SUCCEEDED"); logged
            }
        }
        else if (pEventHeader->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET) {
            pMediaPlayer->Play();
            IMFVideoDisplayControl* pDisplay = NULL;
            HRESULT hr2 = MFGetService(pMediaPlayer, MR_VIDEO_RENDER_SERVICE, IID_IMFVideoDisplayControl, (void**)&pDisplay);
            Log("MFGetService result: %08X", hr2);
            if (SUCCEEDED(hr2)) {
                int videoHeight = (regHeight - (offsetY * 2)) * 2;
                RECT destRect = { 0, 0, regWidth, videoHeight };
                hr2 = pDisplay->SetVideoPosition(NULL, &destRect);
                Log("SetVideoPosition result: %08X", hr2);
                hr2 = pDisplay->SetAspectRatioMode(MFVideoARMode_None);
                Log("SetAspectRatioMode result: %08X", hr2);
                pDisplay->Release();
            }
            Log("Playback started");
        }
        else if (pEventHeader->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED) {
            videoFinished = true;
            Log("Video playback ended");
        }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(IMFPMediaPlayerCallback)) {
            *ppv = this;
            AddRef();
            Log("AddRef Called"); // this hasn't been logged yet
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&refCount);
        //Log("Release Happens"); // logged
        if (count == 0) delete this;
        return count;
    }
};

MediaPlayerCallback* pCallback = nullptr;

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { overlayWindow = NULL; }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void CloseOverlayWindow() {
    if (pMediaPlayer) {
        pMediaPlayer->Stop();
        pMediaPlayer->Release();
        pMediaPlayer = NULL;
    }
    if (pCallback) {
        pCallback->Release();
        pCallback = NULL;
    }
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        overlayWindow = NULL;
    }
}

WNDPROC origGameProc = NULL;

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOVE && overlayWindow) {
        POINT clientPos = { 0, 0 };
        ClientToScreen(hwnd, &clientPos);
        SetWindowPos(overlayWindow, HWND_TOPMOST,
            clientPos.x, clientPos.y + offsetY,
            0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (msg == WM_DESTROY || msg == WM_CLOSE) {
        CloseOverlayWindow();
    }
    return CallWindowProc(origGameProc, hwnd, msg, wParam, lParam);
}

void CreateOverlayWindow() {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SmackOverlay";
    //RegisterClassExA(&wc);
    if (!RegisterClassExA(&wc)) {
        // already registered, ignore error
    }

    // Get game window position
    HWND gameWnd = FindWindowA(NULL, "The War Of The Worlds");
    RECT gameRect = {};
    RECT clientRect = {};
    POINT clientPos = { 0, 0 };
    if (gameWnd) {
        GetClientRect(gameWnd, &clientRect);
        ClientToScreen(gameWnd, &clientPos);
    }
    // Video display area: full width, half height (540 for 1080p source), offset by letterbox
    int videoHeight = regHeight - (offsetY * 2);

    overlayWindow = CreateWindowExA(
        WS_EX_NOACTIVATE,
        "SmackOverlay", NULL,
        WS_POPUP | WS_VISIBLE,
        clientPos.x, clientPos.y + offsetY,
        regWidth, videoHeight,
        gameWnd,  // parent = game window
        NULL,
        GetModuleHandleA(NULL), NULL
    );
    if (gameWnd && !origGameProc) {
        origGameProc = (WNDPROC)SetWindowLongPtr(gameWnd, GWLP_WNDPROC, (LONG_PTR)GameWndProc);
    }
    Log("Overlay window created: %dx%d at x=%d y=%d", regWidth, videoHeight, gameRect.left, gameRect.top + offsetY); // logged
}

struct FakeSmack {
    uint32_t Version = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Frames = 0;
    int OffsetY = 0; // 1360 + 1366x768 use the default
};

FakeSmack dummy;

// We use __stdcall (WINAPI) to match the @X byte counts perfectly
extern "C" {
    void WINAPI SmackClose(void* smk) {
        Log("SmackClose - Forcing cleanup");
        videoFinished = true; // Ensure any last DoFrame calls see the exit signal
        CloseOverlayWindow();
    }
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
        Log("Updated String: %s", movie.c_str());

        videoFinished = false;
        CreateOverlayWindow();

        if (overlayWindow) {
            pCallback = new MediaPlayerCallback();
            HRESULT hr = MFPCreateMediaPlayer(
                nullptr, FALSE, 0,
                pCallback,
                overlayWindow,
                &pMediaPlayer
            );
            if (SUCCEEDED(hr)) {
                // Convert to wide string for MFPlay
                int len = MultiByteToWideChar(CP_ACP, 0, movie.c_str(), -1, NULL, 0);
                std::wstring wMovie(len, 0);
                MultiByteToWideChar(CP_ACP, 0, movie.c_str(), -1, &wMovie[0], len);

                hr = pMediaPlayer->CreateMediaItemFromURL(wMovie.c_str(), FALSE, 0, NULL);
                Log(SUCCEEDED(hr) ? "Media item created" : "Failed to create media item"); // logged
            }
            else {
                Log("MFPCreateMediaPlayer failed: %08X", hr); // hasn't been logged happening yet
            }
        }

        dummy.Width = regWidth;
        dummy.Height = regHeight;
        dummy.Frames = 1000; // do we even need this
        dummy.OffsetY = offsetY;

        return &dummy;
    }

    void WINAPI SmackUseMMX(DWORD flag) { Log("SmackUseMMX: %d", flag); }
    void WINAPI SmackSoundUseDirectSound(void* ds) { Log("SmackSoundUseDirectSound"); }
    void WINAPI SmackNextFrame(void* smk) {
        // pump messages so the overlay window stays responsive
        //Log("NEXT FRAME"); // logged
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    int WINAPI SmackDoFrame(void* smk) {
        //Log("SmackDoFrame"); // logged
        return videoFinished ? 1 : 0;
    }

    void WINAPI SmackToBuffer(void* smk, DWORD l, DWORD t, DWORD p, DWORD h, void* buf, DWORD f) {
        //Log("SmackToBuffer"); // logged
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DeleteFileA("smack_bink_log.txt");
        HKEY hKey;
        // Note: We use HKEY_LOCAL_MACHINE and the path you provided. 
        // Since your app is 32-bit, Windows automatically handles the WOW6432Node redirection.
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Screen", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hKey, "Size", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                sscanf(buffer, "%d,%d", &regWidth, &regHeight);
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
        // determine letterboxing arrangement
        // Calculate letterbox offset
        switch (regWidth + regHeight) {
            case 1120: offsetY = 60; break;     // 640x480
            case 1400: offsetY = 75; break;     // 800x600
            case 1792: offsetY = 96; break;     // 1024x768
            case 2016: offsetY = 108; break;    // 1152x864
            case 2048: offsetY = 24; break;     // 1280x768
            case 2080: offsetY = 40; break;     // 1280x800
            case 2304: offsetY = 152; break;    // 1280x1024
        }
        MFStartup(MF_VERSION);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        CloseOverlayWindow();
        if (pMediaPlayer) {
            pMediaPlayer->Shutdown();
            pMediaPlayer->Release();
            pMediaPlayer = NULL;
        }
        MFShutdown();
    }
    return TRUE;
}