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
FILE* logFile = nullptr;
bool debug = true; // true for logging
bool musicFocus = (GetFileAttributesA("music_focus.txt") != INVALID_FILE_ATTRIBUTES); // allow music to continue playing while the window is out of focus
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
//float masterVolume = 1.0f; // flt_4CA870
//float ambientVolume = 1.0f; // flt_4CA858
//float speechVolume = 1.0f; // unk_4CA86C
DWORD cdState = 1;
volatile bool isStopping = false;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		if (debug) DeleteFileA("_inmm_log.txt");
		char exeName[MAX_PATH];
		GetModuleFileNameA(NULL, exeName, MAX_PATH);
		isNetworkVersion = (strstr(exeName, "WoW_network") != NULL);
		InitializeCriticalSection(&audioLock);
		HKEY hKey;
		cdState = 1; // Default to ON

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Sound\\Volume", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			char buffer[256];
			DWORD type = 0;
			DWORD bufferSize;

			auto ReadFloat = [&](const char* valueName, DWORD addr) {
				bufferSize = sizeof(buffer);
				if (RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
					buffer[min(bufferSize, (DWORD)255)] = '\0';
					*(float*)addr = (float)atof(buffer);
				}
			};

			ReadFloat("Master", 0x004CA870);
			ReadFloat("Ambient", 0x004CA858);
			ReadFloat("Speech", 0x004CA86C);

			DWORD cdSize = sizeof(DWORD);
			if (RegQueryValueExA(hKey, "CD", NULL, &type, (LPBYTE)&cdState, &cdSize) == ERROR_SUCCESS) {
				*(int*)0x004B8A88 = (int)cdState;
			}
			RegCloseKey(hKey);
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		// Volume Persistence (Master, Ambient, Speech, In-Game CD Music)
		HKEY hKey;
		if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Sound\\Volume",
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			char volBuffer[32];

			// Master Volume
			sprintf(volBuffer, "%f", *(float*)0x004CA870);
			RegSetValueExA(hKey, "Master", 0, REG_SZ, (LPBYTE)volBuffer, (DWORD)(strlen(volBuffer) + 1));

			// Ambient Volume
			sprintf(volBuffer, "%f", *(float*)0x004CA858);
			RegSetValueExA(hKey, "Ambient", 0, REG_SZ, (LPBYTE)volBuffer, (DWORD)(strlen(volBuffer) + 1));

			// Speech Volume
			sprintf(volBuffer, "%f", *(float*)0x004CA86C);
			RegSetValueExA(hKey, "Speech", 0, REG_SZ, (LPBYTE)volBuffer, (DWORD)(strlen(volBuffer) + 1));

			// In-Game CD Music
			DWORD cdState = (DWORD)*(int*)0x004B8A88;
			RegSetValueExA(hKey, "CD", 0, REG_DWORD, (const BYTE*)&cdState, sizeof(DWORD));

			RegCloseKey(hKey);
		}
		DeleteCriticalSection(&audioLock);
	}
	return TRUE;
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

#define CHUNK_SIZE 8192  // bytes per chunk — adjust for latency vs responsiveness
#define NUM_BUFFERS 2    // double buffering

WAVEHDR waveHdrs[NUM_BUFFERS] = {};
HGLOBAL hWaveData[NUM_BUFFERS] = {};
LPSTR pAudioData = nullptr;   // full decoded PCM
DWORD audioDataSize = 0;
DWORD audioReadPos = 0;

// StopAudio_Locked: must be called with audioLock already held.
// Separating the locked body lets PlayWav_Locked reuse it without re-entering.
static void StopAudio_Locked() {
    if (hWaveOut) {
        isStopping = true;        // tell callback to stop queueing
        waveOutReset(hWaveOut);   // returns all pending buffers
        for (int b = 0; b < NUM_BUFFERS; b++) {
            if (waveHdrs[b].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(hWaveOut, &waveHdrs[b], sizeof(WAVEHDR));
            if (hWaveData[b]) { GlobalFree(hWaveData[b]); hWaveData[b] = nullptr; }
            waveHdrs[b] = {};
        }
        waveOutClose(hWaveOut);
        hWaveOut = NULL;
        isStopping = false;       // reset for next use
    }
    if (pAudioData) { GlobalFree(pAudioData); pAudioData = nullptr; }
    audioDataSize = 0;
    audioReadPos = 0;
}

void StopAudio() {
	EnterCriticalSection(&audioLock);
	StopAudio_Locked();
	LeaveCriticalSection(&audioLock);
}

HWND gameWindow = NULL;
WNDPROC origWndProc = NULL;

// lastFocusEventTick is written only inside audioLock (in WndProcHook) and read
// only inside audioLock (in _ciSendCommandA MCI_STOP), so no separate guard needed.
DWORD lastFocusEventTick = 0;

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if ((int)cdState == 0 && *pMenuState1 != CD_PLAYER_MENU_ID) {
		StopAudio();
		return CallWindowProc(origWndProc, hwnd, msg, wParam, lParam);
	}
	if (msg == WM_ACTIVATEAPP) {
		// Acquire the lock before touching lastFocusEventTick so the timestamp
		// and the isPaused state change are a single atomic operation from the
		// perspective of _ciSendCommandA.
		EnterCriticalSection(&audioLock);
		lastFocusEventTick = GetTickCount();
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
			// Only restart if we still have a valid handle — PlayWav may have
			// raced ahead and already opened a new one.
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

void CALLBACK WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg != WOM_DONE) return;
	if (isStopping) return;  // bail out during stop/cleanup
	WAVEHDR* hdr = (WAVEHDR*)dwParam1;

	EnterCriticalSection(&audioLock);
	if (audioReadPos >= audioDataSize || !hWaveOut) {
		LeaveCriticalSection(&audioLock);
		return;
	}

	DWORD remaining = audioDataSize - audioReadPos;
	DWORD toWrite = min(remaining, (DWORD)CHUNK_SIZE);
	float vol = *(float*)0x004CA870;

	// Copy and scale into the buffer
	int16_t* src = (int16_t*)(pAudioData + audioReadPos);
	int16_t* dst = (int16_t*)hdr->lpData;
	DWORD samples = toWrite / sizeof(int16_t);
	for (DWORD i = 0; i < samples; i++) {
		int32_t s = (int32_t)(src[i] * vol);
		if (s > 32767) s = 32767;
		if (s < -32768) s = -32768;
		dst[i] = (int16_t)s;
	}

	audioReadPos += toWrite;
	hdr->dwBufferLength = toWrite;
	waveOutPrepareHeader(hWaveOut, hdr, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, hdr, sizeof(WAVEHDR));
	LeaveCriticalSection(&audioLock);
}

static void PlayWav_Locked(const char* path) {
	StopAudio_Locked();

	HMMIO hmmio = mmioOpenA((LPSTR)path, NULL, MMIO_READ);
	if (!hmmio) return;

	MMCKINFO riff = {}, fmt = {}, data = {};
	riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mmioDescend(hmmio, &riff, NULL, MMIO_FINDRIFF);
	fmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmioDescend(hmmio, &fmt, &riff, MMIO_FINDCHUNK);
	WAVEFORMATEX wfx = {};
	mmioRead(hmmio, (HPSTR)&wfx, sizeof(WAVEFORMATEX));
	mmioAscend(hmmio, &fmt, 0);
	data.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mmioDescend(hmmio, &data, &riff, MMIO_FINDCHUNK);

	// Load full PCM into pAudioData
	if (pAudioData) { GlobalFree(pAudioData); pAudioData = nullptr; }
	HGLOBAL hRaw = GlobalAlloc(GMEM_MOVEABLE, data.cksize);
	pAudioData = (LPSTR)GlobalLock(hRaw);
	mmioRead(hmmio, pAudioData, data.cksize);
	mmioClose(hmmio, 0);
	audioDataSize = data.cksize;
	audioReadPos = 0;

	// Open with callback
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx,
		(DWORD_PTR)WaveOutCallback, 0, CALLBACK_FUNCTION);

	// Allocate chunk buffers and prime both
	for (int b = 0; b < NUM_BUFFERS; b++) {
		hWaveData[b] = GlobalAlloc(GMEM_MOVEABLE, CHUNK_SIZE);
		waveHdrs[b].lpData = (LPSTR)GlobalLock(hWaveData[b]);
		waveHdrs[b].dwBufferLength = CHUNK_SIZE;
		// Prime by calling the callback logic directly
		WaveOutCallback(hWaveOut, WOM_DONE, 0, (DWORD_PTR)&waveHdrs[b], 0);
	}
}

extern "C" DLLEXPORT MCIERROR WINAPI _ciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if ((int)cdState == 0 && *pMenuState1 != CD_PLAYER_MENU_ID) {
		StopAudio();
		return 0;
	}

	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		EnterCriticalSection(&audioLock);
		DWORD currentTick = GetTickCount();

		// lastFocusEventTick is now written inside the lock by WndProcHook,
		// so this read is safe.
		if (musicFocus && (currentTick - lastFocusEventTick < 250)) {
			Log("MCI_STOP: Suppressing focus-related stop spam.");
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		if (isNetworkVersion && GetTickCount() - lastPlayTime < 1000) {
			Log("Suppressed post-play stop");
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		dwStartTime = 0;
		totalElapsedBeforePause = 0;
		isPaused = false;
		StopAudio_Locked();
		LeaveCriticalSection(&audioLock);
		Log("MCI_STOP/MCI_CLOSE");
		return 0;
	}

	// 3. SEEK
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		// currentTrack is only written here and read in MCI_PLAY/STATUS — both on
		// the same game thread — so no lock needed for this assignment.
		currentTrack = (int)lpSeek->dwTo;
		if (isNetworkVersion) seekAfterOpen = true;
		Log("MCI_SEEK to: %d", (int)lpSeek->dwTo);
		return 0;
	}

	// 4. OPEN
	if (uMsg == MCI_OPEN) {
		// gameWindow init: guard against double-hook from rapid MCI_OPEN calls.
		// The check-then-act must be atomic; use the existing lock.
		EnterCriticalSection(&audioLock);
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
		lastOpenTime = GetTickCount();
		if (isNetworkVersion) seekAfterOpen = false;
		LeaveCriticalSection(&audioLock);

		LPMCI_OPEN_PARMS lpOpen = (LPMCI_OPEN_PARMS)dwParam;
		if (lpOpen) lpOpen->wDeviceID = (MCIDEVICEID)FAKE_CD_ID;
		Log("MCI_OPEN");
		return 0;
	}

	// 5. PLAY
	if (uMsg == MCI_PLAY) {
		EnterCriticalSection(&audioLock);

		if (!isNetworkVersion && !seekAfterOpen && GetTickCount() - lastOpenTime < 1000) {
			Log("Suppressed focus-triggered play");
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		Log("MCI_PLAY: Initializing track %d", currentTrack);
		// only play track values within range to prevent seeking to tracks that dont exist

		if (isNetworkVersion) {
			lastPlayTime = GetTickCount();
			Log("MCI_PLAY Tick: %d", lastPlayTime);
		}

		if (currentTrack >= 2 && currentTrack <= 5) {
			if (isPaused) {
				// Resume: hWaveOut is still valid (we hold the lock, nobody can close it)
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
			else {
				char path[MAX_PATH];
				wsprintfA(path, "Music\\%02d Track%02d.wav", currentTrack, currentTrack);
				currentTrackLength = GetWavDuration(path);
				dwStartTime = GetTickCount();
				// PlayWav_Locked opens the new handle inside the lock, so WndProcHook
				// can never observe the window between StopAudio and waveOutWrite.
				PlayWav_Locked(path);
			}
		}
		LeaveCriticalSection(&audioLock);
		return 0;
	}

	// 6. STATUS
	if (uMsg == MCI_STATUS) {
		if (hWaveOut) {
			float liveVolume = *(float*)0x004CA870;

			// FORCE a value just to see if the hardware responds at all
			// If you hardcode this to 0.1f and the music stays loud, 
			// then waveOutSetVolume is being ignored by the OS.
			static float lastVol = -1.0f;
			if (liveVolume != lastVol) {
				Log("Volume Change Detected in Memory: %f", liveVolume);
				lastVol = liveVolume;

				WORD volWord = (WORD)(liveVolume * 0xFFFF);
				DWORD dwVolume = ((DWORD)volWord << 16) | volWord;
				waveOutSetVolume(NULL, dwVolume);
			}
		}
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		if (lpStatus->dwItem == MCI_STATUS_POSITION) {
			// Read dwStartTime and currentTrackLength inside the lock so a concurrent
			// MCI_STOP can't zero dwStartTime mid-calculation and produce a wrapped
			// elapsed value.
			EnterCriticalSection(&audioLock);
			DWORD elapsed = (dwStartTime > 0) ? GetTickCount() - dwStartTime : 0;
			DWORD trackLen = currentTrackLength;
			LeaveCriticalSection(&audioLock);

			if (elapsed > trackLen) elapsed = trackLen;
			DWORD seconds = elapsed / 1000;
			DWORD minutes = seconds / 60;
			seconds = seconds % 60;
			lpStatus->dwReturn = MCI_MAKE_TMSF(currentTrack, minutes, seconds, 0);
		}
		else if (lpStatus->dwItem == MCI_STATUS_NUMBER_OF_TRACKS) {
			lpStatus->dwReturn = 5;
		}
		else if (lpStatus->dwItem == MCI_STATUS_CURRENT_TRACK) {
			lpStatus->dwReturn = currentTrack;
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
		EnterCriticalSection(&audioLock);
		if (!isPaused) {
			totalElapsedBeforePause = GetTickCount() - dwStartTime;
			isPaused = true;
			waveOutPause(hWaveOut);
			Log("MCI_PAUSE");
		}
		LeaveCriticalSection(&audioLock);
		return 0;
	}
	return 0;
}

extern "C" DLLEXPORT BOOL WINAPI _ciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText) { return mciGetErrorStringA(mcierr, pszText, cchText); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeBeginPeriod(UINT uPeriod) { return timeBeginPeriod(uPeriod); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeGetDevCaps(LPTIMECAPS ptc, UINT cbtc) { return timeGetDevCaps(ptc, cbtc); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeEndPeriod(UINT uPeriod) { return timeEndPeriod(uPeriod); }