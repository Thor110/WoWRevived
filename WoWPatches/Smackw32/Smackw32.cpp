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
bool debug = false;
int regWidth = 640;
int regHeight = 480;
int offsetY = 0;
bool videoFinished = false;
bool isFullscreen = false;
HWND overlayWindow = NULL;
IMFPMediaPlayer* pMediaPlayer = NULL;

// === Logging === //
void Log(const char* fmt, ...)
{
    if (!debug) return;
    if (!logFile) logFile = fopen("Smackw32_log.txt", "a");
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

#define WM_CLOSE_OVERLAY (WM_USER + 1)

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { overlayWindow = NULL; }
    if (msg == WM_CLOSE_OVERLAY) {
        DestroyWindow(hwnd);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void CloseOverlayWindow() {
    if (pMediaPlayer) {
        pMediaPlayer->Stop();
        pMediaPlayer->Shutdown();
        pMediaPlayer->Release();
        pMediaPlayer = NULL;
    }
    if (pCallback) {
        pCallback->Release();
        pCallback = NULL;
    }
    if (overlayWindow) {
        PostMessage(overlayWindow, WM_CLOSE_OVERLAY, 0, 0);
        //BOOL result = DestroyWindow(overlayWindow);
        //Log("DestroyWindow result: %d, last error: %d", result, GetLastError());
        overlayWindow = NULL;
    }
}

WNDPROC origGameProc = NULL;

void CreateOverlayWindow();

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOVE && overlayWindow) {
        POINT clientPos = { 0, 0 };
        ClientToScreen(hwnd, &clientPos);
        SetWindowPos(overlayWindow, HWND_TOP,
            clientPos.x, clientPos.y + offsetY,
            0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (msg == WM_DESTROY || msg == WM_CLOSE) {
        CloseOverlayWindow();
    }
    if (msg == WM_ACTIVATEAPP && isFullscreen) {
        if (!wParam) {
            videoFinished = true;
            CloseOverlayWindow();
            TerminateProcess(GetCurrentProcess(), 0); // force kill the process because alt-tabbing doesn't work in full-screen mode
        }
    }
    return CallWindowProc(origGameProc, hwnd, msg, wParam, lParam);
}

void CreateOverlayWindow() {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SmackOverlay";
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
        isFullscreen ? (WS_EX_TOPMOST | WS_EX_NOACTIVATE) : WS_EX_NOACTIVATE,
        "SmackOverlay", NULL,
        WS_POPUP,
        clientPos.x, clientPos.y + offsetY,
        regWidth, videoHeight,
        isFullscreen ? NULL : gameWnd,
        NULL,
        GetModuleHandleA(NULL), NULL
    );
    ShowWindow(overlayWindow, SW_SHOWNA);
    if (gameWnd && !origGameProc) {
        origGameProc = (WNDPROC)SetWindowLongPtr(gameWnd, GWLP_WNDPROC, (LONG_PTR)GameWndProc);
    }
    Log("Overlay window created: %dx%d at x=%d y=%d", regWidth, videoHeight, gameRect.left, gameRect.top + offsetY); // logged
}

struct FakeSmack {
    uint32_t Width = 0;
    uint32_t Height = 0;
    int OffsetY = 0; // 1360 + 1366x768 use the default
};

FakeSmack dummy;

bool firstMovie = false;

// We use __stdcall (WINAPI) to match the @X byte counts perfectly
extern "C" {
    void WINAPI SmackClose(void* smk) {
        Log("SmackClose");
        ShowCursor(TRUE);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        videoFinished = true; // Ensure any last DoFrame calls see the exit signal
        CloseOverlayWindow();
    }
    void WINAPI SmackBlitClose(void* smk) { Sleep(1); /*Log("SmackBlitClose");*/ }
    void WINAPI SmackWait(void* smk) { Sleep(1); /*Log("SmackWait");*/ }
    void WINAPI SmackBlitOpen(void* smk) { Sleep(1); /*Log("SmackBlitOpen");*/ }
    void WINAPI SmackDDSurfaceType(void* lpDDS) { Sleep(1); /*Log("SmackDDSurfaceType");*/ }

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
        dummy.OffsetY = offsetY;

        return &dummy;
    }

    void WINAPI SmackUseMMX(DWORD flag) { Sleep(1); /*Log("SmackUseMMX: %d", flag);*/ }
    void WINAPI SmackSoundUseDirectSound(void* ds) { Sleep(1); /*Log("SmackSoundUseDirectSound");*/ }
    void WINAPI SmackNextFrame(void* smk) {
        SetCursor(NULL);
        ShowCursor(FALSE);
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
        Sleep(1); /*Log("SmackToBuffer"); */ // logged
    }
}

HHOOK kbHook = NULL;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && isFullscreen) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (kb->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                FILE* f = fopen("_alttab_exit.txt", "w");
                if (f) fclose(f);
            }
        }
    }
    return CallNextHookEx(kbHook, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DeleteFileA("Smackw32_log.txt");
        kbHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
        // Note: We use HKEY_LOCAL_MACHINE and the path you provided. 
        // Since your app is 32-bit, Windows automatically handles the WOW6432Node redirection.
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            DWORD type = 0;
            if (RegQueryValueExA(hKey, "Full Screen", NULL, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                if (type == REG_DWORD) {
                    isFullscreen = (*(DWORD*)buffer == 1); // dword in networked executable
                }
                else {
                    isFullscreen = (strcmp(buffer, "1") == 0); // string in regular executable
                }
            }
            bufferSize = sizeof(buffer);
            HKEY screenKey;
            if (RegOpenKeyExA(hKey, "Screen", 0, KEY_READ, &screenKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(screenKey, "Size", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                    sscanf(buffer, "%d,%d", &regWidth, &regHeight);
                }
                RegCloseKey(screenKey);
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
        if (kbHook) UnhookWindowsHookEx(kbHook);
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