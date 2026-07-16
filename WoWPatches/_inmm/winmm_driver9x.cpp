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
bool debug = false; // true for logging
bool musicFocus = false; // allow music to continue playing while the window is out of focus
// Test-only latch for the force-press-Stop experiment (see ForceStopButtonPress
// below) - stops it firing repeatedly once per elapsed-time threshold, and gets
// reset whenever a new track starts.
bool forceStopFired = false;
// Pointers to the game's internal Menu State
volatile BYTE* pCDMusicToggle = nullptr;
// The Control ID for the CD Player menu
const DWORD CD_PLAYER_MENU_ID = 0x803E;
CRITICAL_SECTION audioLock;
bool playerIsHuman = (GetFileAttributesA("human.cd") != INVALID_FILE_ATTRIBUTES);
// Tracks the system uptime tick of the last physical Escape press
DWORD lastEscapeTick = 0;
// Tracks the system uptime tick of the last physical mouse click
DWORD lastClickTick = 0;

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
bool isNetworkVersion = false;	// vanilla			// network
//float masterVolume = 1.0f;	// flt_4CA870		// dword_530654
//float ambientVolume = 1.0f;	// flt_4CA858		// flt_53063C
//float speechVolume = 1.0f;	// unk_4CA86C		// unk_530650
DWORD cdState = 1;
volatile bool isStopping = false;

HWND notifyWindow = NULL;
DWORD notifyDeviceID = FAKE_CD_ID;
bool notifyPending = false;

// Tracks whether the game currently believes the CD device is open.
// The real MCI stack would reject SEEK/PLAY/STATUS on a closed device;
// we never did, which lets the game's post-notify MCI_SEEK through when
// it should have failed and triggered the game's own error handling.
bool deviceOpen = false;

// === Force-press the CD Player menu's own Stop button ===
//
// Reverse-engineered from the retail exe (sub_407990 builds the CD Player
// menu's 5 transport buttons; sub_4081D0 is that menu object's own internal
// message handler). All 5 transport buttons share one "group" ID, passed as
// the LOWORD of wParam on the object's internal WM_COMMAND (0x111) message;
// the specific button is distinguished by the LOWORD of lParam:
//   0 = Previous, 1 = Play, 2 = Stop, 3 = Next, 5 = Pause
//
// This is the same internal call the game makes to itself when the menu's
// own hit-testing detects a real mouse click on one of these buttons - i.e.
// calling this replicates a manual Stop click exactly, rather than trying
// to reconstruct what a click "should" do. This deliberately does NOT go
// through MM_MCINOTIFY (0x3B9) - testing showed the retail build's handler
// for that case is a dead stub (fires WM_COMMAND with wParam=3, which
// matches none of the real button/track IDs and does nothing useful).
#define CD_TRANSPORT_BUTTON_GROUP 0x80D3
#define CD_BUTTON_STOP 2

typedef int(__thiscall* CDPlayerOnMessage_t)(void* pThis, UINT uMsg, UINT wParam, UINT lParam);
// VANILLA: sub_4081D0, confirmed directly. NETWORK: not yet known - the ctor
// sets the object's vtable to off_51B1E0 (mov dword ptr [edx], offset off_51B1E0
// in sub_4AF680), and vanilla's equivalent OnMessage lives at vtable slot +0x1C
// (call dword ptr [edx+1Ch] in sub_4081D0's own MM_MCINOTIFY handler). So the
// network build's OnMessage address is whatever DWORD sits at off_51B1E0+0x1C
// in its .rdata - need that looked up before this will do anything on network.
#define CD_PLAYER_ON_MESSAGE_ADDR_VANILLA 0x4081D0
#define CD_PLAYER_ON_MESSAGE_ADDR_NETWORK 0x4B0510
CDPlayerOnMessage_t CDPlayerOnMessage = nullptr;

// TODO: this is NOT a fixed address yet - the CD Player menu object is
// heap-allocated fresh each time the menu opens (see sub_407990), so there's
// no static "this" pointer to hardcode. pCDMusicToggle (above) sits at a
// fixed address and holds the *ID* of whichever menu is currently active;
// a "current menu ID" field like that is often stored right next to a
// "current menu object pointer" field in the same struct, so scanning the
// DWORDs immediately around 0x4B8A88 (vanilla) / 0x5427CC (network) while
// the CD Player menu is open is the most promising place to find it.
// Once found, point this at that (possibly indirected) address.
void* pCDPlayerMenuThis = nullptr;

void ForceStopButtonPress()
{
	if (!pCDPlayerMenuThis) {
		Log("ForceStopButtonPress: pCDPlayerMenuThis not set, skipping");
		return;
	}
	if (!CDPlayerOnMessage) {
		Log("ForceStopButtonPress: CDPlayerOnMessage address not set for this build, skipping");
		return;
	}
	Log("ForceStopButtonPress: invoking CDPlayerOnMessage(this=0x%p, WM_COMMAND, group=0x%X, STOP)", pCDPlayerMenuThis, CD_TRANSPORT_BUTTON_GROUP);
	CDPlayerOnMessage(pCDPlayerMenuThis, 0x111 /* WM_COMMAND */, CD_TRANSPORT_BUTTON_GROUP, CD_BUTTON_STOP);
}

// NOTE: the memory-scan approach that used to live here targeted pCDMusicToggle
// (0x4B8A88 / 0x5427CC) on the theory that a "current menu ID" field is often
// stored next to a "current menu object" pointer. Disassembly disproved the
// premise entirely: pCDMusicToggle sits in a block of registry-default seeds
// (volume slider defaults, the CD toggle default, and the literal "Sound\Volume"
// key-path string right after it) - nothing to do with menu tracking. Tracing
// the real CD_PLAYER_MENU_ID (0x803E) comparisons in sub_402B80 also ruled out
// a simple polled global: that function receives the menu ID as a parameter in
// eax, i.e. it's a dispatcher/factory, not something reading persistent state.
// Next approach: hook the CD Player menu constructor directly and capture ecx
// (== "this" under __thiscall) the moment it's called, rather than hunting
// for a static pointer that may not exist.
//
// VANILLA: sub_407990 at 0x407990. Frame-omitted prologue; the opening SEH
// setup (push -1; push offset SEH_407990) gives a clean 7-byte window that
// never touches ECX.
//
// NETWORK: sub_4AF680 at 0x4AF680, confirmed via the network exe's own
// disassembly - same role (builds the same 5 transport buttons with the same
// 0x80D3 group ID and 0/1/2/3/5 sub-IDs as vanilla, confirming this is the
// same WCdPlayer.cpp source compiled with different settings, not different
// logic). Its prologue is a full debug-style frame (push ebp; mov ebp,esp;
// push -1), which conveniently sums to exactly 5 bytes - a perfect jmp-hook
// fit with no NOP padding needed. ECX isn't touched until the later
// "mov [ebp+var_104], ecx", well after our overwritten region.
//
// Both are real inline hooks (read original bytes, build a trampoline that
// runs them plus a jmp back, then overwrite the live function with a jmp to
// our stub) - same category of technique as the VirtualProtect byte-patches
// in Smackw32.cpp, just intercepting a call instead of patching static data.
#define CD_PLAYER_MENU_CTOR_ADDR_VANILLA 0x407990
#define CD_PLAYER_MENU_CTOR_ADDR_NETWORK 0x4AF680
const int HOOK_LEN_VANILLA = 7; // "push -1" (2 bytes) + "push offset SEH_407990" (5 bytes)
const int HOOK_LEN_NETWORK = 5; // "push ebp" (1) + "mov ebp,esp" (2) + "push -1" (2)

BYTE g_originalBytes[8]; // sized for the larger of the two hook lengths
BYTE* g_trampoline = nullptr;
int g_hookLen = 0;

void __declspec(naked) CDPlayerMenuCtor_HookStub()
{
	__asm {
		push eax
		mov eax, ecx        // ecx = "this" for the CD Player menu object being constructed
		mov pCDPlayerMenuThis, eax
		pop eax
		jmp g_trampoline    // resume the original prologue exactly as if nothing happened
	}
}

void InstallCDPlayerMenuHook()
{
	BYTE* target = isNetworkVersion ? (BYTE*)CD_PLAYER_MENU_CTOR_ADDR_NETWORK : (BYTE*)CD_PLAYER_MENU_CTOR_ADDR_VANILLA;
	g_hookLen = isNetworkVersion ? HOOK_LEN_NETWORK : HOOK_LEN_VANILLA;

	if (!isNetworkVersion) {
		CDPlayerOnMessage = (CDPlayerOnMessage_t)CD_PLAYER_ON_MESSAGE_ADDR_VANILLA;
	}
	else {
		CDPlayerOnMessage = (CDPlayerOnMessage_t)CD_PLAYER_ON_MESSAGE_ADDR_NETWORK;
	}
	// else: left null until the network build's off_51B1E0+0x1C vtable slot is known.

	memcpy(g_originalBytes, target, g_hookLen);

	// Trampoline = original bytes + a jmp back to right after them.
	g_trampoline = (BYTE*)VirtualAlloc(NULL, g_hookLen + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memcpy(g_trampoline, g_originalBytes, g_hookLen);
	g_trampoline[g_hookLen] = 0xE9; // jmp rel32
	*(DWORD*)(g_trampoline + g_hookLen + 1) = (DWORD)(target + g_hookLen) - (DWORD)(g_trampoline + g_hookLen + 5);

	// Patch the live function: jmp rel32 to our stub, NOP-pad any remaining bytes.
	DWORD oldProtect;
	VirtualProtect(target, g_hookLen, PAGE_EXECUTE_READWRITE, &oldProtect);
	target[0] = 0xE9;
	*(DWORD*)(target + 1) = (DWORD)&CDPlayerMenuCtor_HookStub - (DWORD)(target + 5);
	for (int i = 5; i < g_hookLen; i++) target[i] = 0x90;
	VirtualProtect(target, g_hookLen, oldProtect, &oldProtect);

	Log("InstallCDPlayerMenuHook: hooked %s CD Player menu ctor at 0x%p", isNetworkVersion ? "network" : "vanilla", target);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		if (debug) DeleteFileA("_inmm_log.txt");
		char exeName[MAX_PATH];
		GetModuleFileNameA(NULL, exeName, MAX_PATH);
		isNetworkVersion = (strstr(exeName, "WoW_network") != NULL);
		InstallCDPlayerMenuHook();
		InitializeCriticalSection(&audioLock);
		cdState = 1; // Default to ON
		HKEY hKey;
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
			if (isNetworkVersion)
			{
				pCDMusicToggle = (volatile BYTE*)0x5427CC;
				ReadFloat("Master", 0x00530654);
				ReadFloat("Ambient", 0x0053063C);
				ReadFloat("Speech", 0x00530650);



				pCDMusicToggle = (volatile BYTE*)0x5427CC;

				// Direct helper to seed a integer address safely out of the registry
				auto ReadDWORD = [&](const char* valueName, DWORD addr) {
					bufferSize = sizeof(buffer);
					if (RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
						if (type == REG_DWORD) {
							*(DWORD*)addr = *(DWORD*)buffer;
						}
						else if (type == REG_SZ) {
							*(DWORD*)addr = (DWORD)atoi(buffer);
						}
					}
				};

				ReadDWORD("Master", 0x00530654);
				ReadDWORD("Ambient", 0x0053063C);
				ReadDWORD("Speech", 0x00530650);
			}
			else
			{
				pCDMusicToggle = (volatile BYTE*)0x4B8A88;
				ReadFloat("Master", 0x004CA870);
				ReadFloat("Ambient", 0x004CA858);
				ReadFloat("Speech", 0x004CA86C);
			}

			DWORD cdSize = sizeof(DWORD);
			DWORD cdFocus = 1;
			if (RegQueryValueExA(hKey, "CD", NULL, &type, (LPBYTE)&cdState, &cdSize) == ERROR_SUCCESS) {
				// is cd state the same memory address as well?

				if (isNetworkVersion)
				{
					// find network version memory addresses
					*(int*)0x005427CC = (int)cdState;
				}
				else
				{
					*(int*)0x004B8A88 = (int)cdState;
				}
			}
			if (RegQueryValueExA(hKey, "CD-Focus", NULL, &type, (LPBYTE)&cdFocus, &cdSize) == ERROR_SUCCESS) {
				musicFocus = (bool)cdFocus;
			}
			RegCloseKey(hKey);
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		// Volume Persistence (Master, Ambient, Speech, In-Game CD Music)
		HKEY hKey;
		if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Sound\\Volume",
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			char volBufferA[32];
			char volBufferB[32];
			char volBufferC[32];
			DWORD cdState = (DWORD) * (int*)0x004B8A88;
			if (isNetworkVersion)
			{
				// find network version memory addresses
				//cdState = (DWORD) * (int*)0x004B8A88;
				sprintf(volBufferA, "%f", *(float*)0x00530654);
				sprintf(volBufferB, "%f", *(float*)0x0053063C);
				sprintf(volBufferC, "%f", *(float*)0x00530650);
			}
			else
			{
				sprintf(volBufferA, "%f", *(float*)0x004CA870);
				sprintf(volBufferB, "%f", *(float*)0x004CA858);
				sprintf(volBufferC, "%f", *(float*)0x004CA86C);
			}
			// Master Volume
			RegSetValueExA(hKey, "Master", 0, REG_SZ, (LPBYTE)volBufferA, (DWORD)(strlen(volBufferA) + 1));

			// Ambient Volume
			RegSetValueExA(hKey, "Ambient", 0, REG_SZ, (LPBYTE)volBufferB, (DWORD)(strlen(volBufferB) + 1));

			// Speech Volume
			RegSetValueExA(hKey, "Speech", 0, REG_SZ, (LPBYTE)volBufferC, (DWORD)(strlen(volBufferC) + 1));

			// In-Game CD Music
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

#define CHUNK_SIZE 8192  // bytes per chunk � adjust for latency vs responsiveness
#define NUM_BUFFERS 2    // double buffering

WAVEHDR waveHdrs[NUM_BUFFERS] = {};
HGLOBAL hWaveData[NUM_BUFFERS] = {};
LPSTR pAudioData = nullptr;   // full decoded PCM
DWORD audioDataSize = 0;
DWORD audioReadPos = 0;

// StopAudio_Locked: must be called with audioLock already held.
// Separating the locked body lets PlayWav_Locked reuse it without re-entering.
static void StopAudio_Locked() {
	/* // old code
	if (hWaveOut) {
		isStopping = true;        // tell callback to stop queueing
		waveOutReset(hWaveOut);   // returns all pending buffers
		for (int b = 0; b < NUM_BUFFERS; b++) {
			if (waveHdrs[b].dwFlags & WHDR_PREPARED) {
				waveOutUnprepareHeader(hWaveOut, &waveHdrs[b], sizeof(WAVEHDR));
			}
			if (hWaveData[b]) {
				GlobalUnlock(hWaveData[b]);
				GlobalFree(hWaveData[b]);
				hWaveData[b] = nullptr;
			}
			waveHdrs[b] = {};
		}
		waveOutClose(hWaveOut);
		hWaveOut = NULL;
		isStopping = false;       // reset for next use
	}
	*/
	HWAVEOUT hToClose = NULL;

	EnterCriticalSection(&audioLock);
	if (hWaveOut) {
		isStopping = true;
		hToClose = hWaveOut; // Copy the handle pointer
		hWaveOut = NULL;     // Instantly clear it so other threads know it's dead
	}
	if (pAudioData) {
		GlobalUnlock(pAudioData); // Ensure we unlock if we used GlobalLock
		GlobalFree(pAudioData);
		pAudioData = nullptr;
	}
	audioDataSize = 0;
	audioReadPos = 0;

	// new code
	LeaveCriticalSection(&audioLock);

	// Now perform the blocking OS teardown completely unprotected by the lock
	if (hToClose) {
		waveOutReset(hToClose); // Flushes buffers safely; callback can now grab audioLock if needed
		for (int b = 0; b < NUM_BUFFERS; b++) {
			if (waveHdrs[b].dwFlags & WHDR_PREPARED) {
				waveOutUnprepareHeader(hToClose, &waveHdrs[b], sizeof(WAVEHDR));
			}
			if (hWaveData[b]) {
				GlobalUnlock(hWaveData[b]);
				GlobalFree(hWaveData[b]);
				hWaveData[b] = nullptr;
			}
			waveHdrs[b] = {};
		}
		waveOutClose(hToClose);

		EnterCriticalSection(&audioLock);
		isStopping = false; // Reset flag safely
		LeaveCriticalSection(&audioLock);
	}
}

void StopAudio() {
	EnterCriticalSection(&audioLock);
	StopAudio_Locked();
	LeaveCriticalSection(&audioLock);
}

HWND gameWindow = NULL;
WNDPROC origWndProc = NULL;

// Custom message used to marshal ForceStopButtonPress() onto the game's own
// main thread. WaveOutCallback runs on a system-created multimedia thread, not
// the game's thread - calling CDPlayerOnMessage directly from there crashed
// the game, almost certainly because that internal handler assumes single-
// threaded access with no locking. PostMessage here is the safe hand-off:
// WndProcHook only ever runs on the same thread as the game's real message
// loop, exactly like the existing WM_ACTIVATEAPP handling below already relies on.
#define WM_APP_FORCE_STOP_PRESS (WM_APP + 0x444)

// lastFocusEventTick is written only inside audioLock (in WndProcHook) and read
// only inside audioLock (in _ciSendCommandA MCI_STOP), so no separate guard needed.
DWORD lastFocusEventTick = 0;

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_APP_FORCE_STOP_PRESS) {
		ForceStopButtonPress();
		return 0;
	}
	// --- ESCAPE KEY MUSIC LATCH ---
	if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
		EnterCriticalSection(&audioLock);
		lastEscapeTick = GetTickCount();
		Log("HOOK: Escape key down registered at tick %lu", lastEscapeTick);
		LeaveCriticalSection(&audioLock);
	}
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if ((int)cdState == 0 && *pCDMusicToggle != CD_PLAYER_MENU_ID) {
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
			// Only restart if we still have a valid handle � PlayWav may have
			// raced ahead and already opened a new one.
			if (hWaveOut && isPaused && !musicFocus) {
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
		}
		LeaveCriticalSection(&audioLock);
	}
	// --- MOUSE CLICK MUSIC LATCH ---
	if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
		EnterCriticalSection(&audioLock);
		lastClickTick = GetTickCount();
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

	// Force-press Stop once elapsed playback time reaches the current track's
	// real length, so the game's own Stop logic (which we've confirmed works
	// cleanly, unlike the broken MM_MCINOTIFY path) runs exactly when the
	// track would naturally finish.
	DWORD elapsed = (dwStartTime > 0) ? GetTickCount() - dwStartTime : 0;
	if (!forceStopFired && dwStartTime > 0 && elapsed >= currentTrackLength) {
		forceStopFired = true;
		Log("Track finished (%lu ms elapsed / %lu ms length), posting WM_APP_FORCE_STOP_PRESS to main thread", elapsed, currentTrackLength);
		LeaveCriticalSection(&audioLock);
		if (gameWindow) PostMessage(gameWindow, WM_APP_FORCE_STOP_PRESS, 0, 0);
		return;
	}
	
	if (audioReadPos >= audioDataSize || !hWaveOut) {
		notifyPending = false;
		dwStartTime = 0;
		totalElapsedBeforePause = 0;
		isPaused = false;
		LeaveCriticalSection(&audioLock);

		Log("Track %d finished naturally; reset timer locally (no MM_MCINOTIFY)", currentTrack);
		return;
	}

	DWORD remaining = audioDataSize - audioReadPos;
	DWORD toWrite = min(remaining, (DWORD)CHUNK_SIZE);

	// === MULTI-BUILD SAFE VOLUME EVALUATION ===
	float vol = 1.0f;
	if (isNetworkVersion) {
		// Read as a float directly matching your DllMain ReadFloat setup
		vol = *(float*)0x00530654;

		// Defensive check: If IDA was right and the engine treats this as a 0-100 integer,
		// a value greater than 1.0f means we need to scale it down to a 0.0-1.0 float.
		if (vol > 1.0f) {
			DWORD rawIntVol = *(DWORD*)0x00530654;
			vol = (float)rawIntVol / 100.0f;
		}
	}
	else {
		vol = *(float*)0x004CA870;
	}

	// Safety clamping bounds
	if (vol > 1.0f) vol = 1.0f;
	if (vol < 0.0f) vol = 0.0f;

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
	Log("IN: uMsg=0x%X fdwCommand=0x%llX deviceOpen=%d", uMsg, (unsigned long long)fdwCommand, deviceOpen);

	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if ((int)cdState == 0 && *pCDMusicToggle != CD_PLAYER_MENU_ID) {
		StopAudio();
		return 0;
	}

	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		EnterCriticalSection(&audioLock);
		// If this stop command was triggered by an Escape menu loop, abort it cleanly
		DWORD currentTick = GetTickCount();

		// --- TIME-BASED MENU SHIELD ---
		// Blocks ANY stop command fired within 600ms of an Escape press
		if (currentTick - lastEscapeTick < 600) {
			Log("MCI_STOP: Suppressed via Escape timing latch (%lu ms delta). Music plays on.", currentTick - lastEscapeTick);
			LeaveCriticalSection(&audioLock);
			return 0;
		}

		// Blocks stop commands fired within 250ms of a physical mouse click - when pressing resume game
		// (Only if the CD Player menu isn't currently open and active)
		bool isCDPlayerOpen = (pCDMusicToggle != nullptr && *pCDMusicToggle == CD_PLAYER_MENU_ID);
		if (currentTick - lastClickTick < 250 && !isCDPlayerOpen) {
			Log("MCI_STOP: Suppressed via Mouse Click timing latch (%lu ms delta). Music plays on.", currentTick - lastClickTick);
			LeaveCriticalSection(&audioLock);
			return 0;
		}

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
		notifyPending = false; //
		deviceOpen = false;
		StopAudio_Locked();
		LeaveCriticalSection(&audioLock);
		Log("MCI_STOP/MCI_CLOSE");
		return 0;
	}

	// 3. SEEK
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		// currentTrack is only written here and read in MCI_PLAY/STATUS � both on
		// the same game thread � so no lock needed for this assignment.
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
		deviceOpen = true;
		LeaveCriticalSection(&audioLock);

		LPMCI_OPEN_PARMS lpOpen = (LPMCI_OPEN_PARMS)dwParam;
		if (lpOpen) lpOpen->wDeviceID = (MCIDEVICEID)FAKE_CD_ID;
		Log("MCI_OPEN");
		return 0;
	}

	// 5. PLAY
	if (uMsg == MCI_PLAY) {
		EnterCriticalSection(&audioLock);
		// Record whether the caller wants MM_MCINOTIFY when this track ends.
		// MCI_PLAY_PARMS.dwCallback holds the window handle when MCI_NOTIFY is set.
		if (fdwCommand & MCI_NOTIFY) {
			LPMCI_PLAY_PARMS lpPlay = (LPMCI_PLAY_PARMS)dwParam;
			notifyWindow = (HWND)lpPlay->dwCallback;
			notifyDeviceID = (DWORD)IDDevice;
			notifyPending = true;
			Log("MCI_PLAY: notify requested, hwnd=0x%p", notifyWindow);
		}
		else {
			notifyPending = false;
		}
		// TODO : Check timing and potential for race condition here ( music doesn't start unless opening the pause menu with the escape key and stops when exiting )
		if (!isNetworkVersion && !seekAfterOpen && GetTickCount() - lastOpenTime < 1000) {
			Log("Suppressed focus-triggered play");
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		Log("MCI_PLAY: Initializing track %d", currentTrack);
		forceStopFired = false; // allow the test threshold to fire again for this track
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
				std::string newFolder = playerIsHuman ? "-Human" : "-Martian";
				wsprintfA(path, "Music%s\\%02d Track%02d.wav", newFolder.c_str(), currentTrack, currentTrack);
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
			float liveVolume = 1.0f;
			if (isNetworkVersion) {
				// Read the true integer from memory and map it to a 0.0 to 1.0 scale
				DWORD rawIntVol = *(DWORD*)0x00530654;
				liveVolume = (float)rawIntVol / 100.0f;

				// Safety clamp to prevent accidental amplification errors
				if (liveVolume > 1.0f) liveVolume = 1.0f;
				if (liveVolume < 0.0f) liveVolume = 0.0f;
			}
			else {
				liveVolume = *(float*)0x004CA870;
			}
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