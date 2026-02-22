/*
Попробуем написать реализацию winmm.dll для Boss Rally и починить таким образом CDDA для Windows XP.
Шкала громкости музыки не работает в игре, но то же происходит на реальной Win98 при использовании Daemon Tools 3.47.

Надо положить earpds.dll и _inmm.dll в папку с игрой.
*/
#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <fstream>

#define DLLEXPORT			__declspec(dllexport)

int badthinghappened = 0;
MCI_OPEN_PARMS mciparms;
int currentTrack = 2;
int previousTrack = 2;
uint32_t currentTrackLength = 0;
DWORD dwStartTime = 0;

// We need a dummy ID that isn't 0
#define FAKE_CD_ID 0xBEEF 

FILE* logFile = nullptr;

bool debug = false; // true on release, false for logging

// === Logging ===
void Log(const char* fmt, ...)
{
	if (debug) return;
	if (!logFile) logFile = fopen("_inmm_log.txt", "w");
	if (!logFile) return;

	va_list args;
	va_start(args, fmt);
	vfprintf(logFile, fmt, args);
	fprintf(logFile, "\n");
	fflush(logFile);
	va_end(args);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeKillEvent(UINT uTimerID)
{
	return timeKillEvent(uTimerID);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent)
{
	return timeSetEvent(uDelay, uResolution, fptc, dwUser, fuEvent);
}

extern "C" DLLEXPORT DWORD WINAPI _imeGetTime(void)
{
	return timeGetTime();
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

extern "C" DLLEXPORT MCIERROR WINAPI _ciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
	//Log("MCI_COMMAND: ID=%X, Msg=%X, Flags=%X", IDDevice, uMsg, fdwCommand);
	// 1. Success for SET
	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE: Use the most generic command possible
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		//Log("MCI_STOP/CLOSE");
		PlaySound(NULL, NULL, 0);
		return 0;
	}

	// 3. SEEK: Track selection
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		if (lpSeek) currentTrack = (int)lpSeek->dwTo;
		//Log("MCI_SEEK to track: %d", currentTrack);
		return 0;
	}

	// 4. OPEN: Handle the fake device ID
	if (uMsg == MCI_OPEN) {
		LPMCI_OPEN_PARMS lpOpen = (LPMCI_OPEN_PARMS)dwParam;
		if (lpOpen) lpOpen->wDeviceID = (MCIDEVICEID)FAKE_CD_ID;
		return 0;
	}

	// 5. PLAY: 
	if (uMsg == MCI_PLAY) {
		Log("MCI_PLAY: Initializing track %d", currentTrack);
		// only get track length, set path or play if the track actually exists
		if (currentTrack < 6 && currentTrack != 1) // seek happens multiple times
		{
			char path[MAX_PATH];
			wsprintfA(path, "Music\\%02d Track%02d.wav", currentTrack, currentTrack);
			currentTrackLength = GetWavDuration(path);
			previousTrack = currentTrack; // set previous track number for displaying when stopped
			dwStartTime = GetTickCount(); // Start our internal timer
			PlaySound(path, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
		}
		return 0;
	}

	// 6. STATUS
	if (uMsg == MCI_STATUS) {
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		if (lpStatus) {
			if (lpStatus->dwItem == MCI_STATUS_NUMBER_OF_TRACKS) lpStatus->dwReturn = 5;
			if (lpStatus->dwItem == MCI_STATUS_POSITION)
			{
				if (currentTrack > 5 || currentTrack == 1) // stop sets track to 6 and sometimes seeks higher or to 1
				{
					lpStatus->dwReturn = (DWORD_PTR)previousTrack; // display correct track number when stop is pressed
				}
				else
				{
					lpStatus->dwReturn = (DWORD_PTR)currentTrack; // reports current track
				}
			}
		}
		return 0;
	}
	return 0;
}

extern "C" DLLEXPORT BOOL WINAPI _ciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText)
{
	return mciGetErrorStringA(mcierr, pszText, cchText);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeBeginPeriod(UINT uPeriod)
{
	return timeBeginPeriod(uPeriod);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeGetDevCaps(LPTIMECAPS ptc, UINT cbtc)
{
	return timeGetDevCaps(ptc, cbtc);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeEndPeriod(UINT uPeriod)
{
	return timeEndPeriod(uPeriod);
}