/*
Попробуем написать реализацию winmm.dll для Boss Rally и починить таким образом CDDA для Windows XP.
Шкала громкости музыки не работает в игре, но то же происходит на реальной Win98 при использовании Daemon Tools 3.47.

Надо положить earpds.dll и _inmm.dll в папку с игрой.
*/
#include <stdio.h>
#include <conio.h>
#include <windows.h>

#define DLLEXPORT			__declspec(dllexport)

int badthinghappened = 0;
MCI_OPEN_PARMS mciparms;
int currentTrack = 2; // Global variable at top of file

// We need a dummy ID that isn't 0
#define FAKE_CD_ID 0xBEEF 

extern "C" DLLEXPORT MCIERROR WINAPI _ciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{

	// 1. Success for SET
	if (uMsg == MCI_SET) return 0;

	// 2. STOP & CLOSE: Use the most generic command possible
	if (uMsg == MCI_STOP || uMsg == MCI_CLOSE) {
		mciSendStringA("stop all", NULL, 0, 0);
		mciSendStringA("close all", NULL, 0, 0);
		return 0;
	}

	// 3. SEEK: Track selection
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		if (lpSeek) currentTrack = (int)lpSeek->dwTo;
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
		LPMCI_PLAY_PARMS lpPlay = (LPMCI_PLAY_PARMS)dwParam;
		int trackToPlay = (int)lpPlay->dwFrom;

		// Mask for TMSF and fallback to seeked track
		trackToPlay = trackToPlay & 0xFF;
		if (trackToPlay == 0) trackToPlay = currentTrack;
		if (trackToPlay < 2) trackToPlay = 2;

		char path[MAX_PATH];
		char cmd[512];

		// Use a static path to avoid any GetFullPathName overhead
		wsprintfA(path, "Music\\%02d Track%02d.wav", trackToPlay, trackToPlay);

		// KILL EVERYTHING before starting new track to prevent overlap
		mciSendStringA("stop all", NULL, 0, 0);
		mciSendStringA("close all", NULL, 0, 0);

		wsprintfA(cmd, "open \"%s\" type waveaudio alias trackmusic", path);
		if (mciSendStringA(cmd, NULL, 0, 0) == 0) {
			mciSendStringA("play trackmusic", NULL, 0, 0);
		}
		return 0;
	}

	// 6. STATUS
	if (uMsg == MCI_STATUS) {
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		if (lpStatus) {
			if (lpStatus->dwItem == MCI_STATUS_NUMBER_OF_TRACKS) lpStatus->dwReturn = 5;
			if (lpStatus->dwItem == MCI_STATUS_POSITION) lpStatus->dwReturn = (DWORD_PTR)currentTrack;
		}
		return 0;
	}

	return 0;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerGetDevCapsA(UINT_PTR uMxId, LPMIXERCAPSA pmxcaps, UINT cbmxcaps)
{
	MMRESULT myres;
	myres = mixerGetDevCapsA(uMxId, pmxcaps, cbmxcaps);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerGetDevCapsA: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerGetLineControlsA(HMIXEROBJ hmxobj, LPMIXERLINECONTROLSA pmxlc, DWORD fdwControls)
{
	MMRESULT myres;
	myres = mixerGetLineControlsA(hmxobj, pmxlc, fdwControls);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerGetLineControlsA: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerOpen(LPHMIXER phmx, UINT uMxId, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen)
{
	MMRESULT myres;
	myres =  mixerOpen(phmx, uMxId, dwCallback, dwInstance, fdwOpen);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerOpen: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerClose(HMIXER hmx)
{
	MMRESULT myres;
	myres = mixerClose(hmx);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerClose: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerGetControlDetailsA(HMIXEROBJ hmxobj, LPMIXERCONTROLDETAILS pmxcd, DWORD fdwDetails)
{
	MMRESULT myres;
	myres = mixerGetControlDetailsA(hmxobj, pmxcd, fdwDetails);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerGetControlDetailsA: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT UINT WINAPI _ixerGetNumDevs(void)
{
	return mixerGetNumDevs();
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerSetControlDetails(HMIXEROBJ hmxobj, LPMIXERCONTROLDETAILS pmxcd, DWORD fdwDetails)
{
	MMRESULT myres;
	myres = mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerSetControlDetails: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _ixerGetLineInfoA(HMIXEROBJ hmxobj, LPMIXERLINEA pmxl, DWORD fdwInfo)
{
	MMRESULT myres;
	myres = mixerGetLineInfoA(hmxobj, pmxl, fdwInfo);
	if (myres != MMSYSERR_NOERROR)
		printf("mixerGetLineInfoA: %x\n", myres);
	return myres;
}

extern "C" DLLEXPORT DWORD WINAPI _imeGetTime(void)
{
	return timeGetTime();
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeKillEvent(UINT uTimerID)
{
	MMRESULT myres = timeKillEvent(uTimerID);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeBeginPeriod(UINT uPeriod)
{
	MMRESULT myres = timeBeginPeriod(uPeriod);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent)
{
	MMRESULT myres;
	myres = timeSetEvent(uDelay, uResolution, fptc, dwUser, fuEvent);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeEndPeriod(UINT uPeriod)
{
	MMRESULT myres = timeEndPeriod(uPeriod);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _uxGetVolume(UINT uDeviceID, LPDWORD pdwVolume)
{
	MMRESULT myres = auxGetVolume(uDeviceID, pdwVolume);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _uxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
	MMRESULT myres = auxSetVolume(uDeviceID, dwVolume);
	return myres;
}

// 1. Force the game to see at least one audio device
extern "C" DLLEXPORT UINT WINAPI _uxGetNumDevs(void)
{
	return 1;
}

// 2. Force that device to identify as a CD-ROM with Volume Control
extern "C" DLLEXPORT MMRESULT WINAPI _uxGetDevCapsA(UINT uDeviceID, LPAUXCAPS pac, UINT cbac)
{
	pac->wMid = 1;                // Microsoft
	pac->wPid = 2;                // Dummy Product ID
	pac->vDriverVersion = 0x0100; // 1.0
	strcpy_s(pac->szPname, "CD Audio");
	pac->wTechnology = AUXCAPS_CDAUDIO;
	pac->dwSupport = AUXCAPS_VOLUME;
	return MMSYSERR_NOERROR;
}

extern "C" DLLEXPORT MMRESULT WINAPI _mioClose(HMMIO hmmio, UINT fuClose)
{
	MMRESULT myres = mmioClose(hmmio, fuClose);
	return myres;
}

extern "C" DLLEXPORT LONG WINAPI _mioRead(HMMIO hmmio, HPSTR pch, LONG cch)
{
	return mmioRead(hmmio, pch, cch);
}

extern "C" DLLEXPORT LONG WINAPI _mioSeek(HMMIO hmmio, LONG lOffset, int iOrigin)
{
	return mmioSeek(hmmio, lOffset, iOrigin);
}

extern "C" DLLEXPORT MMRESULT WINAPI _mioAscend(HMMIO hmmio, LPMMCKINFO pmmcki, UINT fuAscend)
{
	MMRESULT myres = mmioAscend(hmmio, pmmcki, fuAscend);
	return myres;
}

extern "C" DLLEXPORT MMRESULT WINAPI _mioDescend(HMMIO hmmio, LPMMCKINFO pmmcki, const MMCKINFO* pmmckiParent, UINT fuDescend)
{
	MMRESULT myres = mmioDescend(hmmio, pmmcki, pmmckiParent, fuDescend);
	return myres;
}

extern "C" DLLEXPORT HMMIO WINAPI _mioOpenA(LPSTR pszFileName, LPMMIOINFO pmmioinfo, DWORD fdwOpen)
{
	return mmioOpenA(pszFileName, pmmioinfo, fdwOpen);
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeGetDevCaps(LPTIMECAPS ptc, UINT cbtc)
{
	MMRESULT myres = timeGetDevCaps(ptc, cbtc);
	return myres;
}

extern "C" DLLEXPORT BOOL WINAPI _ciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText)
{
	strcpy_s(pszText, cchText, "Success");
	return TRUE;
}