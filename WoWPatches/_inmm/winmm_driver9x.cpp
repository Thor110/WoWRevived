#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <fstream>

#define DLLEXPORT			__declspec(dllexport)
#define FAKE_CD_ID 0xBEEF 

int currentTrack = 2;
uint32_t currentTrackLength = 0;
DWORD dwStartTime = 0;
DWORD totalElapsedBeforePause = 0;
bool isPaused = false;
DWORD lastOpenTime = 0;
HWAVEOUT hWaveOut = NULL;
WAVEHDR waveHdr = {};
HGLOBAL hWaveData = NULL;
FILE* logFile = nullptr;
bool debug = true; // true for logging
bool musicFocus = (GetFileAttributesA("music_focus.txt") != INVALID_FILE_ATTRIBUTES); // allow music to continue playing while the window is out of focus
int* pGameMusicEnabled = (int*)0x004B8A88; // detect if music is enabled or disabled
// Pointers to the game's internal Menu State
volatile DWORD* pMenuState1 = (volatile DWORD*)0x4D1490;
// The Control ID for the CD Player menu
const DWORD CD_PLAYER_MENU_ID = 0x803E;

CRITICAL_SECTION audioLock;

// === Logging === //
void Log(const char* fmt, ...)
{
	if (!debug) return;
	if (!logFile) logFile = fopen("_inmm_log.txt", "w");
	if (!logFile) return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	fprintf(logFile, "[%02d:%02d:%02d.%03d] : ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	va_list args;
	va_start(args, fmt);
	vfprintf(logFile, fmt, args);
	fprintf(logFile, "\n");
	fflush(logFile);
	va_end(args);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeKillEvent(UINT uTimerID) { return timeKillEvent(uTimerID); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent) { return timeSetEvent(uDelay, uResolution, fptc, dwUser, fuEvent); }

extern "C" DLLEXPORT DWORD WINAPI _imeGetTime(void) { return timeGetTime(); }

// networking executable specific variables
bool seekAfterOpen = false;
DWORD lastPlayTime = 0;
bool isNetworkVersion = false;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		if(debug) DeleteFileA("_inmm_log.txt");
		char exeName[MAX_PATH];
		GetModuleFileNameA(NULL, exeName, MAX_PATH);
		isNetworkVersion = (strstr(exeName, "WoW_network") != NULL);
		InitializeCriticalSection(&audioLock);
		// LOAD STATE: Check which file currently exists
		if (GetFileAttributesA("music_disabled.txt") != INVALID_FILE_ATTRIBUTES) {
			*pGameMusicEnabled = 0; // Force UI to "Off"
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		// SAVE STATE: When the game closes, check the current UI value
		if (*pGameMusicEnabled == 0) {
			MoveFileA("music_enabled.txt", "music_disabled.txt");
		}
		else {
			MoveFileA("music_disabled.txt", "music_enabled.txt");
		}
		DeleteCriticalSection(&audioLock);
	}
	return TRUE;
}

HWND gameWindow = NULL;
WNDPROC origWndProc = NULL;
bool losingFocus = false;
bool gainingFocus = false;

DWORD lastFocusEventTick = 0;

void StopAudio();

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if (*pGameMusicEnabled == 0 && *pMenuState1 != CD_PLAYER_MENU_ID) {
		StopAudio();
		return CallWindowProc(origWndProc, hwnd, msg, wParam, lParam);
	}
	if (msg == WM_ACTIVATEAPP) {
		lastFocusEventTick = GetTickCount(); // Mark exactly WHEN focus shifted
		EnterCriticalSection(&audioLock);
		if (!wParam) {
			Log("HOOK: Focus Lost");
			if (hWaveOut && !isPaused && !musicFocus) {
				totalElapsedBeforePause = GetTickCount() - dwStartTime;
				isPaused = true;
				waveOutPause(hWaveOut);
			}
		}
		else {
			Log("HOOK: Focus Gained");
			if (hWaveOut && isPaused && !musicFocus) {
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
		}
		LeaveCriticalSection(&audioLock);
	}
	return CallWindowProc(origWndProc, hwnd, msg, wParam, lParam);
}

// Simple helper to get duration in milliseconds
uint32_t GetWavDuration(const char* filename) {
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()) return 0;

	char buffer[44]; // Standard WAV header size
	file.read(buffer, 44);

	// ByteRate is at offset 28, Subchunk2Size (Data Size) is at offset 40
	uint32_t byteRate = *reinterpret_cast<uint32_t*>(&buffer[28]);
	uint32_t dataSize = *reinterpret_cast<uint32_t*>(&buffer[40]);

	if (byteRate == 0) return 0;
	return (uint32_t)((double)dataSize / byteRate * 1000);
}

void StopAudio() {
	EnterCriticalSection(&audioLock);
	if (hWaveOut) {
		waveOutReset(hWaveOut); // stops playback immediately
		// Wait for the driver to release the buffer
		// If we don't wait here, GlobalFree will kill the memory 
		// while wdmaud is still cleaning up its header list.
		// Wait for driver to signal buffer completion
		int timeout = 0;
		while (!(waveHdr.dwFlags & WHDR_DONE) && timeout < 100) {
			Sleep(1);
			timeout++;
		}

		// Unprepare MUST happen before Close or Free
		MMRESULT res = waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
		if (res != MMSYSERR_NOERROR) Log("StopAudio: Unprepare failed: %d", res);
		//
		//waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		hWaveOut = NULL;
	}
	if (hWaveData) {
		GlobalFree(hWaveData);
		hWaveData = NULL;
	}
	LeaveCriticalSection(&audioLock);
}

void PlayWav(const char* path) {
	StopAudio(); // always clean up first

	// Read the whole file into memory
	HMMIO hmmio = mmioOpenA((LPSTR)path, NULL, MMIO_READ);
	if (!hmmio) return;

	MMCKINFO riff = {}, data = {};
	riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mmioDescend(hmmio, &riff, NULL, MMIO_FINDRIFF);

	MMCKINFO fmt = {};
	fmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmioDescend(hmmio, &fmt, &riff, MMIO_FINDCHUNK);

	WAVEFORMATEX wfx = {};
	mmioRead(hmmio, (HPSTR)&wfx, sizeof(WAVEFORMATEX));
	mmioAscend(hmmio, &fmt, 0);

	data.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mmioDescend(hmmio, &data, &riff, MMIO_FINDCHUNK);

	hWaveData = GlobalAlloc(GMEM_MOVEABLE, data.cksize);
	LPSTR pData = (LPSTR)GlobalLock(hWaveData);
	mmioRead(hmmio, pData, data.cksize);
	mmioClose(hmmio, 0);

	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);

	waveHdr = {};
	waveHdr.lpData = pData;
	waveHdr.dwBufferLength = data.cksize;
	waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));
}

extern "C" DLLEXPORT MCIERROR WINAPI _ciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if (*pGameMusicEnabled == 0 && *pMenuState1 != CD_PLAYER_MENU_ID) {
		StopAudio();
		return 0;
	}
	//Log("MCI_COMMAND: ID=%X, Msg=%X, Flags=%X", IDDevice, uMsg, fdwCommand);
	// 1. Success for SET
	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE: Use the most generic command possible
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		DWORD currentTick = GetTickCount();

		// If musicFocus is enabled AND a focus event happened within the last 250ms
		// we can safely assume this STOP is a redundant 'auto-stop' from the engine/OS.
		if (musicFocus && (currentTick - lastFocusEventTick < 250)) {
			Log("MCI_STOP: Suppressing focus-related stop spam.");
			return 0;
		}
		// network version specific
		if (isNetworkVersion && GetTickCount() - lastPlayTime < 1000) {
			Log("Suppressed post-play stop");
			Log("MCI_STOP Tick: %d", GetTickCount());
			Log("MCI_STOP Last: %d", lastPlayTime);
			return 0;
		}
		dwStartTime = 0;
		totalElapsedBeforePause = 0;
		isPaused = false;
		StopAudio();
		Log("MCI_STOP/MCI_CLOSE");
		return 0;
	}

	// 3. SEEK: Track selection
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		currentTrack = (int)lpSeek->dwTo;

		if (isNetworkVersion) {
			Log("MCI_SEEK Tick: %d", GetTickCount());
			Log("MCI_SEEK Last: %d", lastPlayTime);
			seekAfterOpen = true; // network version specific
		}

		Log("MCI_SEEK to: %d", (int)lpSeek->dwTo);
		return 0;
	}

	// 4. OPEN: Handle the fake device ID
	if (uMsg == MCI_OPEN) {
		if (!gameWindow) {
			HWND hw = GetForegroundWindow();
			char title[256];
			GetWindowTextA(hw, title, sizeof(title));
			Log("Window title: %s", title);
			gameWindow = FindWindow(NULL, title);
			if (gameWindow) {
				Log("Found window: %s", title);
				origWndProc = (WNDPROC)SetWindowLongPtr(gameWindow, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
			}
		}
		LPMCI_OPEN_PARMS lpOpen = (LPMCI_OPEN_PARMS)dwParam;
		if (lpOpen) lpOpen->wDeviceID = (MCIDEVICEID)FAKE_CD_ID;
		lastOpenTime = GetTickCount();

		if (isNetworkVersion) {
			Log("MCI_OPEN Tick: %d", GetTickCount());
			Log("MCI_OPEN Last: %d", lastPlayTime);
			seekAfterOpen = false; // network version specific
		}

		Log("MCI_OPEN close");
		return 0;
	}

	// 5. PLAY: 
	if (uMsg == MCI_PLAY) {
		// this fails in network version because it fires MCI_OPEN
		if (!isNetworkVersion && !seekAfterOpen && GetTickCount() - lastOpenTime < 1000) {
			Log("Suppressed focus-triggered play");
			return 0;
		}
		Log("MCI_PLAY: Initializing track %d", currentTrack);
		// only play track values within range to prevent seeking to tracks that dont exist

		if (isNetworkVersion) {
			lastPlayTime = GetTickCount(); // network version specific
			Log("MCI_PLAY Tick: %d", GetTickCount());
			Log("MCI_PLAY Last: %d", lastPlayTime);
		}

		if (currentTrack >= 2 && currentTrack <= 5) {
			if (isPaused) {
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
			else {
				char path[MAX_PATH];
				wsprintfA(path, "Music\\%02d Track%02d.wav", currentTrack, currentTrack);
				currentTrackLength = GetWavDuration(path);
				//Log("Track %d duration: %d ms", currentTrack, currentTrackLength);
				dwStartTime = GetTickCount();
				PlayWav(path);
			}
		}
		return 0;
	}

	// 6. STATUS
	if (uMsg == MCI_STATUS) {
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		//Log("MCI_STATUS: dwItem=%X, dwTrack=%d, flags=%X", lpStatus->dwItem, lpStatus->dwTrack, fdwCommand);
		if (lpStatus->dwItem == MCI_STATUS_POSITION)
		{
			DWORD elapsed = (dwStartTime > 0) ? GetTickCount() - dwStartTime : 0;
			if (elapsed > currentTrackLength) elapsed = currentTrackLength;

			DWORD seconds = elapsed / 1000;
			DWORD minutes = seconds / 60;
			seconds = seconds % 60;

			lpStatus->dwReturn = MCI_MAKE_TMSF(currentTrack, minutes, seconds, 0);
			//Log("MCI_STATUS_POSITION %d", lpStatus->dwReturn);
		}
		else if (lpStatus->dwItem == MCI_STATUS_NUMBER_OF_TRACKS) {
			lpStatus->dwReturn = 5;
			//Log("MCI_STATUS_NUMBER_OF_TRACKS %d", lpStatus->dwReturn);
		}
		else if (lpStatus->dwItem == MCI_STATUS_CURRENT_TRACK) {
			lpStatus->dwReturn = currentTrack;
			//Log("MCI_STATUS_CURRENT_TRACK %d", lpStatus->dwReturn);
		}
		else if (lpStatus->dwItem == MCI_STATUS_LENGTH) {
			/* // stop counter? progress? something
			DWORD totalSeconds = currentTrackLength / 1000;
			DWORD minutes = totalSeconds / 60;
			DWORD seconds = totalSeconds % 60;
			lpStatus->dwReturn = MCI_MAKE_TMSF(currentTrack, minutes, seconds, 0);
			*/ // pause counter on focus lost / resume on regained?
			//Log("MCI_STATUS_LENGTH %d", lpStatus->dwReturn);
		}
		return 0;
	}

	// 7. PAUSE
	if (uMsg == MCI_PAUSE) {
		if (!isPaused) {
			Log("MCI_PAUSE Tick: %d", GetTickCount());
			Log("MCI_PAUSE Last: %d", lastPlayTime);
			totalElapsedBeforePause = GetTickCount() - dwStartTime;
			isPaused = true;
			waveOutPause(hWaveOut);
		}
		return 0;
	}
	return 0;
}

extern "C" DLLEXPORT BOOL WINAPI _ciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText) { return mciGetErrorStringA(mcierr, pszText, cchText); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeBeginPeriod(UINT uPeriod) { return timeBeginPeriod(uPeriod); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeGetDevCaps(LPTIMECAPS ptc, UINT cbtc) { return timeGetDevCaps(ptc, cbtc); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeEndPeriod(UINT uPeriod) { return timeEndPeriod(uPeriod); }