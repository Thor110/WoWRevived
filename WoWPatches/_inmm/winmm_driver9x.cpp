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
DWORD totalElapsedBeforePause = 0;
bool isPaused = false;

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

HWAVEOUT hWaveOut = NULL;
WAVEHDR waveHdr = {};
HGLOBAL hWaveData = NULL;

void StopAudio() {
	if (hWaveOut) {
		waveOutReset(hWaveOut);      // stops playback immediately
		waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		hWaveOut = NULL;
	}
	if (hWaveData) {
		GlobalFree(hWaveData);
		hWaveData = NULL;
	}
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
	Log("MCI_COMMAND: ID=%X, Msg=%X, Flags=%X", IDDevice, uMsg, fdwCommand);
	// 1. Success for SET
	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE: Use the most generic command possible
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		currentTrack = previousTrack;
		dwStartTime = 0;
		totalElapsedBeforePause = 0;
		isPaused = false;
		Log("MCI_STOP/CLOSE");
		StopAudio();
		return 0;
	}

	// 3. SEEK: Track selection
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		currentTrack = (int)lpSeek->dwTo;
		Log("MCI_SEEK to: %d", (int)lpSeek->dwTo);
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
		char path[MAX_PATH];
		wsprintfA(path, "Music\\%02d Track%02d.wav", currentTrack, currentTrack);
		if (currentTrack >= 2 && currentTrack <= 5) {
			if (isPaused) {
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
			else {
				currentTrackLength = GetWavDuration(path);
				previousTrack = currentTrack; // set previous track number for displaying when stopped
				dwStartTime = GetTickCount();
				PlayWav(path);
			}
		}
		return 0;
	}

	// 6. STATUS
	if (uMsg == MCI_STATUS) {
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		if (lpStatus->dwItem == MCI_STATUS_POSITION)
		{
			DWORD elapsed = (dwStartTime > 0) ? GetTickCount() - dwStartTime : 0;
			if (elapsed > currentTrackLength) elapsed = currentTrackLength;

			DWORD seconds = elapsed / 1000;
			DWORD minutes = seconds / 60;
			seconds = seconds % 60;

			lpStatus->dwReturn = MCI_MAKE_TMSF(previousTrack, minutes, seconds, 0);
		}
		return 0;
	}


	// 7. PAUSE
	if (uMsg == MCI_PAUSE) {
		if (!isPaused) {
			totalElapsedBeforePause = GetTickCount() - dwStartTime;
			isPaused = true;
			waveOutPause(hWaveOut);
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