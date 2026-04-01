#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <mfidl.h>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfplat.lib")

// ============================================================
//  Globals
// ============================================================

FILE* logFile = nullptr;
bool  debug = false;
int   regWidth = 640;
int   regHeight = 480;
int   offsetY = 0;
bool  videoFinished = false;
bool victoryMovieFinished = false;
bool  isFullscreen = false;
HWND  overlayWindow = NULL;
IMFPMediaPlayer* pMediaPlayer = NULL;
bool playerIsHuman = (GetFileAttributesA("human.cd") != INVALID_FILE_ATTRIBUTES);

// Credits scroll state
Image* creditsImage = nullptr;
float   creditsScrollY = 0.0f;   // current scroll position in image pixels
float   creditsScrollPx = 0.0f;   // pixels per second, set after image loads
ULONG_PTR gdiplusToken = 0;

// ============================================================
//  Logging
// ============================================================

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

// ============================================================
//  Credits interception
//
//  No hook on sub_4041B0 - credits init runs fully so audio plays.
//
//  Two patches at startup:
//    0x405470 -> RET  (disables tile renderer: no crash, no broken visual)
//
//  Detection:
//    Menu path:        0x4D1490 == 0x80CA  (set when Credits button clicked)
//                      0x4D255C == 0x90    (menu idle state)
//    Post-victory:     0x4D255C == 0x80CA  (set when entering credits state)
//                      0x4D1490 == 0x80FE  (brief transitional - use as trigger)
//                      0x4D1490 == 0x90    (stable post-victory credits state)
//
//  End detection:
//    Menu:             0x4D1490 changes away from 0x80CA
//    Post-victory:     0x4D1490 changes away from settled value (0x90)
//                      Write 0x80C9 to 0x4D255C after destroy to reset state
//                      otherwise menu credits won't trigger after post-victory
// ============================================================

#define ADDR_STATE_255C  ((volatile DWORD*)0x4D255C)
#define ADDR_STATE_1490  ((volatile DWORD*)0x4D1490)

HWND   creditsOverlay = NULL;
HANDLE creditsThread = NULL;
bool   creditsShutdown = false;

void CreateOverlayWindow();
void CloseOverlayWindow();

void InstallCreditsHook()
{
    DWORD old;

    // Patch sub_405470 to return immediately.
    // This prevents the tile renderer from being created (no crash at any resolution).
    // Audio is set up in sub_4052B0 which runs before sub_405470 is ever called.
    BYTE* p = (BYTE*)0x405470;
    VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
    *p = 0xC3;  // RET
    VirtualProtect(p, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 1);
    Log("sub_405470 patched to RET (tile renderer disabled)");
}

// ============================================================
//  Credits overlay window
// ============================================================

#define WM_CLOSE_CREDITS (WM_USER + 2)

LRESULT CALLBACK CreditsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { creditsOverlay = NULL; }
    if (msg == WM_CLOSE_CREDITS) { DestroyWindow(hwnd); }
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
        HWND gameWnd = FindWindowA(NULL, "The War Of The Worlds");
        if (gameWnd) PostMessage(gameWnd, msg, wParam, lParam);
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        // Double-buffer to avoid flicker
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        // Fill black
        HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(memDC, &rc, black);

        if (creditsImage) {
            Graphics g(memDC);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

            int imgW = (int)creditsImage->GetWidth();
            int imgH = (int)creditsImage->GetHeight();

            // Centre image horizontally, scroll vertically
            int destX = (w - imgW) / 2;
            int srcY = (int)creditsScrollY;
            int srcH = min(h, imgH - srcY);
            if (srcH > 0) {
                g.DrawImage(creditsImage,
                    destX, 0,           // dest top-left
                    0, srcY, imgW, srcH, // src rect
                    UnitPixel);
            }
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void CreateCreditsOverlay()
{
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = CreditsWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "WoWCreditsOverlay";
    RegisterClassExA(&wc);

    HWND gameWnd = FindWindowA(NULL, "The War Of The Worlds");
    POINT clientPos = { 0, 0 };
    if (gameWnd) ClientToScreen(gameWnd, &clientPos);
    creditsOverlay = CreateWindowExA(
        (isFullscreen ? (WS_EX_TOPMOST | WS_EX_NOACTIVATE) : WS_EX_NOACTIVATE) | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        "WoWCreditsOverlay", NULL, WS_POPUP,
        clientPos.x, clientPos.y, regWidth, regHeight,
        isFullscreen ? NULL : gameWnd,
        NULL, GetModuleHandleA(NULL), NULL
    );

    if (creditsOverlay) {
        SetLayeredWindowAttributes(creditsOverlay, 0, 255, LWA_ALPHA);  // fully opaque but input-transparent
        ShowWindow(creditsOverlay, SW_SHOWNA);
        // Load credits.png from the same directory as the exe
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        // Replace filename with credits.png
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
        strcat(exePath, "credits.png");

        wchar_t wPath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, exePath, -1, wPath, MAX_PATH);
        creditsImage = Image::FromFile(wPath);

        if (creditsImage && creditsImage->GetLastStatus() == Ok) {
            // Scroll the full image height plus one screen height over the window lifetime.
            // Speed is set here as pixels/second - adjust to taste or measure audio later.
            // Default: scroll full image in ~60 seconds (tune per language).
            creditsScrollY = 0.0f;

            Log("Player faction: %s", playerIsHuman ? "Human" : "Martian");

            // Timing table: { pngSize, humanDur (s), martianDur (s) }
            struct CreditsInfo { DWORD pngSize; float humanDur; float martianDur; };
            DWORD pngSize = 0;
            HANDLE hf = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hf != INVALID_HANDLE_VALUE) { pngSize = GetFileSize(hf, NULL); CloseHandle(hf); }

            static const CreditsInfo kTiming[] = {
                { 252640, 103.0f, 136.0f },  // English
                { 287375, 106.0f, 143.0f },  // French
                { 289388, 106.0f, 143.0f },  // German
                { 255818, 109.0f, 145.0f },  // Italian
                { 270546, 107.0f, 144.0f },  // Spanish
            };

            float dur = playerIsHuman ? 103.0f : 136.0f;  // fallback to English
            for (const auto& info : kTiming) {
                if (info.pngSize == pngSize) {
                    dur = (playerIsHuman ? info.humanDur : info.martianDur);
                    Log("Matched language by PNG size %u, dur=%.1fs", pngSize, dur);
                    break;
                }
            }

            creditsScrollPx = (float)creditsImage->GetHeight() / dur;
            Log("Scroll speed: %.2f px/s over %.1fs", creditsScrollPx, dur);


            Log("credits.png loaded %dx%d scroll=%.1f px/s",
                creditsImage->GetWidth(), creditsImage->GetHeight(), creditsScrollPx);
        }
        else {
            Log("credits.png failed to load from %s", exePath);
            if (creditsImage) { delete creditsImage; creditsImage = nullptr; }
        }
        Log("Credits overlay created %dx%d", regWidth, regHeight);
    }
    else {
        Log("Credits overlay FAILED err=%d", GetLastError());
    }
}

void DestroyCreditsOverlay()
{
    if (creditsImage) { delete creditsImage; creditsImage = nullptr; }
    if (creditsOverlay) { PostMessage(creditsOverlay, WM_CLOSE_CREDITS, 0, 0); }
    Log("Credits overlay destroyed");
}

// ============================================================
//  Watch thread
// ============================================================

// Forward declaration
void CreateCreditsOverlay();
void DestroyCreditsOverlay();

DWORD WINAPI CreditsWatchThread(LPVOID)
{
    Log("CreditsWatchThread started");

    // Track the last value of 4D1490 we acted on so we don't re-trigger
    // after returning to menu (where 4D1490 stays 0x80CA until next click).
    DWORD last1490Handled = 0;
    float creditsHoldRemaining = 2.0f;  // seconds to hold before scrolling
    DWORD lastTick = GetTickCount();

    while (!creditsShutdown)
    {
        DWORD s255C = *ADDR_STATE_255C;
        DWORD s1490 = *ADDR_STATE_1490;
        Log("State: 255C=0x%X 1490=0x%X", s255C, s1490);

        bool postVictory = (s255C == 0x80CA && s1490 == 0x80FE);
        bool fromMenu = (s1490 == 0x80CA && s255C != 0x80CA && s1490 != last1490Handled);

        if (!postVictory && !fromMenu) { Sleep(10); continue; }

        last1490Handled = s1490;  // prevent re-entry after return

        Log("Credits detected (postVictory=%d s255C=0x%X s1490=0x%X)", postVictory ? 1 : 0, s255C, s1490);

        if (postVictory) {
            victoryMovieFinished = false; // reset before waiting
            Log("Post-victory: waiting for victory movie to finish");
            while (!creditsShutdown && !victoryMovieFinished) {
                Sleep(100);
            }
            Sleep(100);
            Log("Post-victory: movie finished, spawning credits");
        }

        CreateCreditsOverlay();
        DWORD settled1490 = *ADDR_STATE_1490; // should be 0x90
        // Wait for end: the watched variable changes away from 0x80CA.
        // Post-victory: 4D255C changes from 0x80CA to 0x80C9 when music ends.
        // Menu:         4D1490 changes when the next menu button is clicked,
        //               OR when sub_4048F0 fires (state machine rebuild).
        //               Also catch 4D255C going to 0 (sub_4048F0 clears it).
        while (!creditsShutdown)
        {
            DWORD cur255C = *ADDR_STATE_255C;
            DWORD cur1490 = *ADDR_STATE_1490;
            bool ended = postVictory ? cur1490 != settled1490 : cur1490 != 0x80CA;

            Log("Watching: 255C=0x%X 1490=0x%X ended=%d", cur255C, cur1490, ended ? 1 : 0);

            if (ended) {
                Log("Credits ended (255C=0x%X 1490=0x%X)", cur255C, cur1490);
                break;
            }

            DWORD now = GetTickCount();
            float dt = min((now - lastTick) / 1000.0f, 0.1f);  // cap dt to avoid jump after sleep
            lastTick = now;

            if (creditsHoldRemaining > 0.0f) {
                creditsHoldRemaining -= dt;
            }
            else if (creditsImage) {
                creditsScrollY += creditsScrollPx * dt;
                InvalidateRect(creditsOverlay, NULL, FALSE);
                if (creditsScrollY > (float)creditsImage->GetHeight()) {
                    creditsScrollY = (float)creditsImage->GetHeight();
                    Sleep(500);
                    // Send ESC to the game to trigger its own credits exit
                    HWND gameWnd = FindWindowA(NULL, "The War Of The Worlds");
                    if (gameWnd) PostMessage(gameWnd, WM_KEYDOWN, VK_ESCAPE, 0);
                    break;
                }
            }

            MSG msg;
            while (PeekMessageA(&msg, creditsOverlay, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            Sleep(16);
        }

        DestroyCreditsOverlay();
        if (postVictory) {
            *ADDR_STATE_255C = 0x80C9; // clear post-victory state
        }
        Log("Credits destroyed (postVictory=%d s255C=0x%X s1490=0x%X)", postVictory ? 1 : 0, s255C, s1490);
        for (int i = 0; i < 20 && creditsOverlay != NULL; i++) { Sleep(10); }
        last1490Handled = 0;
        creditsScrollY = 0.0f;
        creditsHoldRemaining = 2.0f;
        lastTick = GetTickCount();

        // Drain leftover messages
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    Log("CreditsWatchThread exiting");
    return 0;
}

// ============================================================
//  Media Foundation callback
// ============================================================

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
            victoryMovieFinished = true;
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

// ============================================================
//  SMK movie overlay
// ============================================================

#define WM_CLOSE_OVERLAY (WM_USER + 1)

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { overlayWindow = NULL; }
    if (msg == WM_CLOSE_OVERLAY) { DestroyWindow(hwnd); }
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
        overlayWindow = NULL;
    }
}

WNDPROC origGameProc = NULL;

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOVE) {
        POINT clientPos = { 0, 0 };
        ClientToScreen(hwnd, &clientPos);
        if (overlayWindow) {
            SetWindowPos(overlayWindow, HWND_TOP, clientPos.x, clientPos.y + offsetY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (creditsOverlay) {
            SetWindowPos(creditsOverlay, HWND_TOP, clientPos.x, clientPos.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
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
    RegisterClassExA(&wc);

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
        (isFullscreen ? (WS_EX_TOPMOST | WS_EX_NOACTIVATE) : WS_EX_NOACTIVATE) | WS_EX_TRANSPARENT | WS_EX_LAYERED,
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

// ============================================================
//  Smacker stubs
// ============================================================

struct FakeSmack { uint32_t Width = 0, Height = 0; int OffsetY = 0; };
FakeSmack dummy;

extern "C" {
    void WINAPI SmackClose(void* smk) {
        uint32_t* smkData = (uint32_t*)smk;
        if (smkData) {
            smkData[2] = 1;
            smkData[3] = 0;
        }
        victoryMovieFinished = true; // if skipped
        Log("SmackClose");
        ShowCursor(TRUE);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
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

        CreateOverlayWindow();

        if (overlayWindow) {
            pCallback = new MediaPlayerCallback();
            HRESULT hr = MFPCreateMediaPlayer(nullptr, FALSE, 0, pCallback, overlayWindow, &pMediaPlayer);
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
        if (videoFinished) {
            // force finish movie with dud frame count
            uint32_t* smkData = (uint32_t*)smk;
            if (smkData) {
                smkData[2] = 1;     // Total Frames = 1
                smkData[3] = 1;     // Current Frame = 1
                videoFinished = false;
            }
        }
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
        return  0;
    }

    void WINAPI SmackToBuffer(void* smk, DWORD l, DWORD t, DWORD p, DWORD h, void* buf, DWORD f) {
        Sleep(1); /*Log("SmackToBuffer"); */ // logged
    }
}

// ============================================================
//  Keyboard hook
// ============================================================
// Somehow this empty keyboard proc effects the compiled file and allows RAGELOGO to play during the human campaign...
// WARNING: This empty function must remain in the compiled binary.
// Its presence changes the DLL's compiled size and memory layout,
// which alters thread scheduling timing enough to prevent a race condition
// Removing this function causes RAGELOGO.mp4 to be skipped on human launch.
// The root cause is a timing race that has not been fully isolated.
// Do not remove this function. Do not move it. Do not add code to it.
// See: https://en.wikipedia.org/wiki/Heisenbug

HHOOK kbHook = NULL;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(kbHook, nCode, wParam, lParam);
}

// ============================================================
//  DllMain
// ============================================================

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DeleteFileA("Smackw32_log.txt");
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
        //"640x480         (4:3)",  // Classic baseline 4:3                         // Exists In-Game
        //"800x600         (4:3)",  // Legacy 4:3 standard                          // Exists In-Game
        //"1024x768       (4:3)",   // XGA — very common                            // Exists In-Game
        //"1152x864       (4:3)",   // Slightly higher 4:3 (rare)                   // Exists In-Game
        //"1280x768       (15:9)",  // WXGA – rare variant of 1280x800 (15:9)
        //"1280x800       (16:10)", // WXGA — early widescreen laptops (16:10)      // Exists In-Game
        //"1280x1024     (5:4)",    // SXGA — tall 5:4 monitor resolution
        //"1360x768       (16:9)",  // 16:9 — GPU-aligned, better than 1366x768
        //"1366x768       (16:9)",  // Common 16:9 laptop resolution
        // These resolutions only work on the main menu - newly expanded warmap allows these resolutions to work
        //"1600x900       (16:9)",  // 16:9 — upper-mid range laptop displays
        //"1600x1024     (5:4)",    // Unusual 5:4 wide — seems to pass internal checks
        //"1600x1200     (4:3)",    // UXGA — classic high-res 4:3
        //"1680x1050     (16:10)",  // WSXGA+ — widescreen 16:10, works well
        //"1920x1080     (16:9)"    // 1080p
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
            case 2624: offsetY = 62; break;     // 1600x1024
            case 2730: offsetY = 52; break;     // 1680x1050
        }

        GdiplusStartupInput gdiplusInput;
        GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);
        MFStartup(MF_VERSION);
        InstallCreditsHook();

        creditsShutdown = false;
        creditsThread = CreateThread(NULL, 0, CreditsWatchThread, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        creditsShutdown = true;
        if (creditsThread) {
            WaitForSingleObject(creditsThread, 500);
            CloseHandle(creditsThread); creditsThread = NULL;
        }
        if (creditsOverlay) { CloseOverlayWindow(); }
        CloseOverlayWindow();
        if (pMediaPlayer) {
            pMediaPlayer->Shutdown(); pMediaPlayer->Release(); pMediaPlayer = NULL;
        }
        MFShutdown();
        GdiplusShutdown(gdiplusToken);
    }
    return TRUE;
}