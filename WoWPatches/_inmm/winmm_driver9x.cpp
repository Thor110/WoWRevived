#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <ctime>
#include <cstdint>
#include <string>
#include <vector>
#include "../Common/GameCommon.h"
#include "../Common/RegistryRedirect.h"

using namespace wow;

#pragma warning(disable: 28159) // suppress GetTickCount -> GetTickCount64 warning, no one is going to play War of the Worlds for over 49 days straight and if they do? More for them.
#define DLLEXPORT			__declspec(dllexport)
#define FAKE_CD_ID 0xBEEF

int currentTrack = 2;
uint32_t currentTrackLength = 0;
DWORD dwStartTime = 0;
DWORD totalElapsedBeforePause = 0;
bool isPaused = false; // pause state in the cd player menu
DWORD lastOpenTime = 0;
HWAVEOUT hWaveOut = NULL;
FILE* logFile = nullptr;
bool debug = false; // logging to _inmm_log.txt, also switched on by "Debug Log=1" in the [Revived] section of WoWRevived.ini
bool musicFocus = false; // allow music to continue playing while the window is out of focus
// Latch so the end-of-track Stop press is only posted once per track, reset whenever a new track starts.
bool forceStopFired = false;
// audioLock guards the playback state shared with the audio thread (hWaveOut, the PCM data, timers).
// deviceLock serialises opening and closing the wave device. Lock order is always deviceLock, then audioLock,
// and no waveOut call that waits on the device (waveOutReset/Close) is ever made while audioLock is held.
CRITICAL_SECTION audioLock;
CRITICAL_SECTION deviceLock;
// human.cd's presence selects the Martian music and FMV folders (verified in-game, see commit c933737),
// Smackw32.dll uses the same rule. Everything that picks a faction's assets goes through UseMartianAssets().
bool humanCdPresent = (GetFileAttributesA("human.cd") != INVALID_FILE_ATTRIBUTES);
inline bool UseMartianAssets() { return humanCdPresent; }
// Tracks the system uptime tick of the last physical Escape press
DWORD lastEscapeTick = 0;
// Tracks the system uptime tick of the last physical mouse click
DWORD lastClickTick = 0;
GameBuild gameBuild = BUILD_UNKNOWN;
bool isNetworkVersion = false;
bool resumingFocus = false;

// === Per-build game addresses === //
// Every game address lives here so the vanilla and network executables can never be mixed up.
// Both builds store the volumes as floats (0.0 - 1.0) in the same layout, confirmed from the
// disassembly: the network setter at 0x4180C7 and getter at 0x418066 use fld/mov on a float array.
struct GameAddrs {
	volatile DWORD* cdToggle;       // in-game "CD music" toggle, seeded from Sound\Volume\CD, the game reads its low byte
	volatile float* masterVol;
	volatile float* ambientVol;
	volatile float* speechVol;
	volatile BYTE*  humanCampaign;  // the "Network\I Am Human" setting, 1 when playing the human side
	volatile DWORD* networkPlayer;  // network only: the local DirectPlay player id, non zero while in a multiplayer session
	// CD Player menu constructor (hooked to capture "this"), destructor (hooked to clear it) and message handler
	DWORD cdCtor; const BYTE* cdCtorSig; int cdCtorSigLen; int cdCtorHookLen;
	DWORD cdDtor; const BYTE* cdDtorSig; int cdDtorSigLen; int cdDtorHookLen; int cdDtorRelCallAt;
	DWORD cdOnMessage; const BYTE* cdOnMessageSig; int cdOnMessageSigLen;
	// Constructor and destructor of the base class every game screen is built on (main menu, CD Player, options, and
	// also the war map and battle map). Hooked to know when a campaign is being played, see OnMenuConstructed.
	DWORD menuCtor; const BYTE* menuCtorSig; int menuCtorSigLen; int menuCtorHookLen;
	DWORD menuDtor; const BYTE* menuDtorSig; int menuDtorSigLen; int menuDtorHookLen;
};

// vanilla: sub_407990 = push -1; push offset SEH_407990 (7 bytes, never touches ECX)
static const BYTE kVanillaCtorSig[] = { 0x6A, 0xFF, 0x68, 0x27, 0x4F, 0x4A, 0x00 };
// vanilla: off_4A95D0 slot 0 = push esi; mov esi,ecx; call 0x4080A0 (8 bytes, the call is relocated in the trampoline)
static const BYTE kVanillaDtorSig[] = { 0x56, 0x8B, 0xF1, 0xE8, 0x18, 0x00, 0x00, 0x00 };
// vanilla: off_4A95D0 slot 7 (+0x1C) = sub_4081D0, handles MM_MCINOTIFY (0x3B9) and WM_COMMAND (0x111)
static const BYTE kVanillaOnMessageSig[] = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF, 0x68, 0x89, 0x4F, 0x4A, 0x00 };
// network: sub_4AF680 = push ebp; mov ebp,esp; push -1 (5 bytes hooked, longer signature checked)
static const BYTE kNetworkCtorSig[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x9F, 0x17, 0x50, 0x00 };
// network: off_51B1E0 slot 0 = push ebp; mov ebp,esp; push ecx; mov [ebp-4],ecx (7 bytes, no relative operands)
static const BYTE kNetworkDtorSig[] = { 0x55, 0x8B, 0xEC, 0x51, 0x89, 0x4D, 0xFC };
// network: off_51B1E0 slot 10 (+0x28) = sub_4B0510, handles 0x3B9 and 0x111 like vanilla's sub_4081D0
static const BYTE kNetworkOnMessageSig[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0xED, 0x17, 0x50, 0x00 };
// vanilla: menu base class constructor sub_40A060 (screen id, name, ...) and destructor sub_40A150,
// both start push -1; push offset SEH (7 bytes, absolute operands only)
static const BYTE kVanillaMenuCtorSig[] = { 0x6A, 0xFF, 0x68, 0x08, 0x51, 0x4A, 0x00 };
static const BYTE kVanillaMenuDtorSig[] = { 0x6A, 0xFF, 0x68, 0x28, 0x51, 0x4A, 0x00 };
// network: menu base class constructor sub_4B6C80 and destructor sub_4B6DA0, push ebp; mov ebp,esp; push -1 (5 bytes)
static const BYTE kNetworkMenuCtorSig[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0xF8, 0x1B, 0x50, 0x00 };
static const BYTE kNetworkMenuDtorSig[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x0A, 0x1C, 0x50, 0x00 };

static const GameAddrs kVanillaAddrs = {
	(volatile DWORD*)0x4B8A88, (volatile float*)0x4CA870, (volatile float*)0x4CA858, (volatile float*)0x4CA86C,
	(volatile BYTE*)0x4B84C4, nullptr,
	0x407990, kVanillaCtorSig, sizeof(kVanillaCtorSig), 7,
	0x408080, kVanillaDtorSig, sizeof(kVanillaDtorSig), 8, 3,
	0x4081D0, kVanillaOnMessageSig, sizeof(kVanillaOnMessageSig),
	0x40A060, kVanillaMenuCtorSig, sizeof(kVanillaMenuCtorSig), 7,
	0x40A150, kVanillaMenuDtorSig, sizeof(kVanillaMenuDtorSig), 7,
};
static const GameAddrs kNetworkAddrs = {
	(volatile DWORD*)0x5427CC, (volatile float*)0x530654, (volatile float*)0x53063C, (volatile float*)0x530650,
	// 0x530518: "I Am Human", registered by sub_5024D0 (vanilla 0x4B84C4)
	(volatile BYTE*)0x530518,
	// 0x5432D0: written by IDirectPlay4::CreatePlayer when hosting or joining (calls at 0x407030 / 0x407599), zeroed at
	// 0x407926 just before IDirectPlay4::Close when leaving, otherwise only read (as the sender id for Send).
	(volatile DWORD*)0x5432D0,
	0x4AF680, kNetworkCtorSig, sizeof(kNetworkCtorSig), 5,
	0x4B1310, kNetworkDtorSig, sizeof(kNetworkDtorSig), 7, -1,
	0x4B0510, kNetworkOnMessageSig, sizeof(kNetworkOnMessageSig),
	0x4B6C80, kNetworkMenuCtorSig, sizeof(kNetworkMenuCtorSig), 5,
	0x4B6DA0, kNetworkMenuDtorSig, sizeof(kNetworkMenuDtorSig), 5,
};
static const GameAddrs kNoAddrs = {}; // unknown build: every game memory access is skipped
const GameAddrs* addrs = &kNoAddrs;
extern volatile LONG g_campaignScreens; // war map / battle map screens currently open, see OnMenuConstructed

// The in-game music toggle, read live so turning music off or on in the options takes effect immediately.
bool CurrentCdState() { return addrs->cdToggle ? ((*addrs->cdToggle & 0xFF) != 0) : true; }

float CurrentMasterVolume()
{
	if (!addrs->masterVol) return 1.0f;
	float vol = *addrs->masterVol;
	if (!(vol >= 0.0f)) return 0.0f; // also catches NaN
	return vol > 1.0f ? 1.0f : vol;
}

// === Settings === //
// When WoWRevived.ini exists the game's registry calls are redirected to it (see RegistryRedirect.h), and this DLL's
// own settings use the same functions so they always land in the same place as the game's.
wow::IniStore g_settings;
bool g_useIni = false;
LONG SettingsOpenKey(LPCSTR subKey, HKEY* key) {
	return g_useIni ? wow::registry::OpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, key) : RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, key);
}
LONG SettingsCreateKey(LPCSTR subKey, HKEY* key) {
	return g_useIni ? wow::registry::CreateKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, key, NULL)
		: RegCreateKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, key, NULL);
}
LONG SettingsQuery(HKEY key, LPCSTR name, DWORD* type, BYTE* data, DWORD* size) {
	return g_useIni ? wow::registry::QueryValueExA(key, name, NULL, type, data, size) : RegQueryValueExA(key, name, NULL, type, data, size);
}
LONG SettingsSet(HKEY key, LPCSTR name, DWORD type, const BYTE* data, DWORD size) {
	return g_useIni ? wow::registry::SetValueExA(key, name, 0, type, data, size) : RegSetValueExA(key, name, 0, type, data, size);
}
LONG SettingsClose(HKEY key) { return g_useIni ? wow::registry::CloseKey(key) : RegCloseKey(key); }
// Reads a value as text whether it's stored as a string or a DWORD.
bool SettingsReadText(HKEY key, LPCSTR name, char* text, DWORD textSize) {
	BYTE buffer[256] = {};
	DWORD type = 0, size = sizeof(buffer) - 1;
	if (SettingsQuery(key, name, &type, buffer, &size) != ERROR_SUCCESS) return false;
	if (type == REG_DWORD && size >= 4) { sprintf_s(text, textSize, "%ld", (long)*(int32_t*)buffer); return true; }
	if (type != REG_SZ) return false;
	buffer[min(size, (DWORD)(sizeof(buffer) - 1))] = '\0';
	strncpy_s(text, textSize, (const char*)buffer, _TRUNCATE);
	return true;
}

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

// --- Discord RPC Definitions ---
struct DiscordRichPresence {
	const char* state;
	const char* details;
	int64_t startTimestamp;
	int64_t endTimestamp;
	const char* largeImageKey;
	const char* largeImageText;
	const char* smallImageKey;
	const char* smallImageText;
	const char* partyId;
	int partySize;
	int partyMax;
	const char* matchSecret;
	const char* joinSecret;
	const char* spectateSecret;
	int8_t instance;
};

struct DiscordUser {
	const char* userId;
	const char* username;
	const char* discriminator;
	const char* avatar;
};

// discord-rpc calls these from Discord_RunCallbacks. Explicitly __cdecl, the release build defaults to stdcall.
struct DiscordEventHandlers {
	void(__cdecl* ready)(const DiscordUser* request);
	void(__cdecl* disconnected)(int errorCode, const char* message);
	void(__cdecl* errored)(int errorCode, const char* message);
	void(__cdecl* joinGame)(const char* joinSecret);
	void(__cdecl* spectateGame)(const char* spectateSecret);
	void(__cdecl* joinRequest)(const DiscordUser* request);
};

static int64_t g_sessionStartTime = 0;

typedef void(__cdecl* Discord_Initialize_t)(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId);
typedef void(__cdecl* Discord_UpdatePresence_t)(const DiscordRichPresence* presence);
typedef void(__cdecl* Discord_RunCallbacks_t)(void);
typedef void(__cdecl* Discord_Shutdown_t)(void);

Discord_RunCallbacks_t pfnDiscord_RunCallbacks = nullptr;
HMODULE hDiscordDLL = NULL;
Discord_Initialize_t pfnDiscord_Initialize = nullptr;
Discord_UpdatePresence_t pfnDiscord_UpdatePresence = nullptr;
Discord_Shutdown_t pfnDiscord_Shutdown = nullptr;
static bool g_discordInitialized = false;

enum GameState { STATE_MENU, STATE_HUMAN, STATE_MARTIAN, STATE_NETWORK, STATE_CDPLAYER };
GameState g_lastGameState = (GameState)-1;
volatile bool g_forcePresenceRefresh = false; // set when Discord (re)connects so the current state is sent again

Language currentLanguage = LANG_EN;

// Track names per language, then soundtrack (0 = Human, 1 = Martian), then CD tracks 2 - 5.
// Discord expects UTF-8, the project is compiled with /utf-8 so these literals are stored as UTF-8.
static const char* const kTrackNames[5][2][4] = {
	{ // LANG_EN
		{ "Dead London", "The Spirit of Man", "Brave New World", "The Eve of the War (Human)" },
		{ "The Red Weed", "Horsell Common", "The Eve of the War (Martian)", "The Fighting Machine" },
	},
	{ // LANG_FR
		{ "Londres Mort", "L'Esprit de l'Homme", "Un Monde Nouveau", "La Veille de la Guerre (Humain)" },
		{ "L'Herbe Rouge", "Horsell Common", "La Veille de la Guerre (Martien)", "La Machine de Combat" },
	},
	{ // LANG_DE
		{ "Totes London", "Der Geist des Menschen", "Schöne Neue Welt", "Der Vorabend des Krieges (Mensch)" },
		{ "Das Rote Unkraut", "Horsell Common", "Der Vorabend des Krieges (Marsianisch)", "Die Kampfmaschine" },
	},
	{ // LANG_IT
		{ "Londra Morta", "Lo Spirito dell'Uomo", "Un Mondo Nuovo", "La Vigilia della Guerra (Umano)" },
		{ "L'Erba Rossa", "Horsell Common", "La Vigilia della Guerra (Marziano)", "La Macchina da Combattimento" },
	},
	{ // LANG_ES
		{ "Londres Muerta", "El Espíritu del Hombre", "Un Mundo Nuevo", "La Víspera de la Guerra (Humano)" },
		{ "La Hierba Roja", "Horsell Common", "La Víspera de la Guerra (Marciano)", "La Máquina de Combate" },
	},
};

// Discord image keys for CD tracks 2 - 5 (cd_bd5 is used by both soundtracks)
static const char* const kTrackImages[2][4] = {
	{ "cd_bd3", "cd_bd7", "cd_bd2", "cd_bd5" }, // Human
	{ "cd_bd6", "cd_bd5", "cd_bd4", "cd_bd1" }, // Martian
};

const char* GetTrackName() {
	if (currentTrack < 2 || currentTrack > 5 || currentLanguage < LANG_EN || currentLanguage > LANG_ES) return "Unknown Track";
	return kTrackNames[currentLanguage][UseMartianAssets() ? 1 : 0][currentTrack - 2];
}

const char* GetTrackImage() {
	if (currentTrack < 2 || currentTrack > 5) return "cd_bd1";
	return kTrackImages[UseMartianAssets() ? 1 : 0][currentTrack - 2];
}

void RefreshPresence(GameState newState) {
	DiscordRichPresence presence = {};
	//presence.startTimestamp = (int64_t)std::time(nullptr);
	presence.startTimestamp = g_sessionStartTime;
	switch (newState) {
		case STATE_MENU:
			presence.largeImageKey = "cover";
			switch (currentLanguage) {
				case LANG_FR:
					presence.state = "Décision du destin de la Terre";
					presence.details = "Menu principal";
					presence.largeImageText = "Jeff Wayne's 'La Guerre des mondes'";
					break;
				case LANG_DE:
					presence.state = "Das Schicksal der Erde entscheiden";
					presence.details = "Hauptmenü";
					presence.largeImageText = "Jeff Wayne's 'Der Krieg der Welten'";
					break;
				case LANG_IT:
					presence.state = "Decidere il destino della Terra";
					presence.details = "Menu principale";
					presence.largeImageText = "Jeff Wayne's 'La guerra dei mondi'";
					break;
				case LANG_ES:
					presence.state = "Decidiendo el destino de la Tierra";
					presence.details = "Menú principal";
					presence.largeImageText = "Jeff Wayne's 'La Guerra de los Mundos'";
					break;
				case LANG_EN:
				default:
					presence.state = "Deciding the Fate of Earth";
					presence.details = "Main Menu";
					presence.largeImageText = "Jeff Wayne's 'The War Of The Worlds'";
					break;
			}
			break;
		case STATE_HUMAN:
			presence.largeImageKey = "human";
			switch (currentLanguage) {
				case LANG_FR:
					presence.state = "Défense de la Terre";
					presence.details = "Campagne humaine";
					presence.largeImageText = "Les chances que quelque chose vienne de Mars...";
					break;
				case LANG_DE:
					presence.state = "Verteidigung der Erde";
					presence.details = "Menschen-Kampagne";
					presence.largeImageText = "Die Chancen, dass etwas vom Mars kommt...";
					break;
				case LANG_IT:
					presence.state = "Difesa della Terra";
					presence.details = "Campagna umana";
					presence.largeImageText = "Le probabilità che qualcosa arrivi da Marte...";
					break;
				case LANG_ES:
					presence.state = "Defendiendo la Tierra";
					presence.details = "Campaña humana";
					presence.largeImageText = "Las posibilidades de que algo venga de Marte...";
					break;
				case LANG_EN:
				default:
					presence.state = "Defending Earth";
					presence.details = "Human Campaign";
					presence.largeImageText = "The chances of anything coming from Mars...";
					break;
			}
			break;
		case STATE_MARTIAN:
			presence.largeImageKey = "martian";
			switch (currentLanguage) {
				case LANG_FR:
					presence.state = "Invasion de la Terre";
					presence.details = "Campagne martienne";
					presence.largeImageText = "Personne n'aurait cru...";
					break;
				case LANG_DE:
					presence.state = "Invasion der Erde";
					presence.details = "Marsianer-Kampagne";
					presence.largeImageText = "Niemand hätte geglaubt...";
					break;
				case LANG_IT:
					presence.state = "Invasione della Terra";
					presence.details = "Campagna marziana";
					presence.largeImageText = "Nessuno avrebbe mai creduto...";
					break;
				case LANG_ES:
					presence.state = "Invadiendo la Tierra";
					presence.details = "Campaña marciana";
					presence.largeImageText = "Nadie habría creído...";
					break;
				case LANG_EN:
				default:
					presence.state = "Invading Earth";
					presence.details = "Martian Campaign";
					presence.largeImageText = "No one would have believed...";
					break;
			}
			break;
		case STATE_NETWORK:
			presence.largeImageKey = "network";
			switch (currentLanguage) {
				case LANG_FR:
					presence.state = "Bataille pour la domination";
					presence.details = "Escarmouche multijoueur";
					presence.largeImageText = "Commandement des forces en ligne...";
					break;
				case LANG_DE:
					presence.state = "Kampf um die Vorherrschaft";
					presence.details = "Multiplayer-Gefecht";
					presence.largeImageText = "Befehl über Truppen online...";
					break;
				case LANG_IT:
					presence.state = "Battaglia per il dominio";
					presence.details = "Schermaglia multigiocatore";
					presence.largeImageText = "Comando delle forze online...";
					break;
				case LANG_ES:
					presence.state = "Batalla por el dominio";
					presence.details = "Escaramuza multijugador";
					presence.largeImageText = "Comandando fuerzas en línea...";
					break;
				case LANG_EN:
				default:
					presence.state = "Battling for Dominance";
					presence.details = "Multiplayer Skirmish";
					presence.largeImageText = "Commanding forces online...";
					break;
			}
			break;
		case STATE_CDPLAYER:
			presence.largeImageKey = GetTrackImage();	// get track image
			presence.largeImageText = GetTrackName();	// get track name
			switch (currentLanguage) {
				case LANG_FR:
					presence.state = "Écoute de la bande originale";
					presence.details = "Lecteur CD";
					break;
				case LANG_DE:
					presence.state = "Hören des Soundtracks";
					presence.details = "CD-Spieler";
					break;
				case LANG_IT:
					presence.state = "Ascolto della colonna sonora";
					presence.details = "Lettore CD";
					break;
				case LANG_ES:
					presence.state = "Escuchando la banda sonora";
					presence.details = "Reproductor de CD";
					break;
				case LANG_EN:
				default:
					presence.state = "Listening to the Score";
					presence.details = "CD Player";
					break;
			}
			break;
	}
	/* // TODO : contemplate hooking up networking one day
	if (isNetworkVersion) {
		presence.partyId = "session_12345";
		presence.partySize = 1;
		presence.partyMax = 8;
		presence.joinSecret = "encrypted_join_code";
	}
	else {
		// Leave secret fields NULL, effectively disabling invites
		presence.partyId = NULL;
		presence.joinSecret = NULL;
	}
	*/



	// network games also show which side is being played
	if (newState == STATE_NETWORK && addrs->humanCampaign) {
		static const char* const kSides[5][2] = {
			{ "Human", "Martian" }, { "Humain", "Martien" }, { "Mensch", "Marsianer" }, { "Umano", "Marziano" }, { "Humano", "Marciano" },
		};
		bool human = (*addrs->humanCampaign == 1);
		presence.smallImageKey = human ? "human" : "martian";
		presence.smallImageText = kSides[(currentLanguage >= LANG_EN && currentLanguage <= LANG_ES) ? currentLanguage : LANG_EN][human ? 0 : 1];
	}

	if (pfnDiscord_UpdatePresence) pfnDiscord_UpdatePresence(&presence);
}

// Set by the CD Player menu constructor hook, cleared by the destructor hook. Non null exactly while the
// CD Player menu is open, so it is also what decides whether the CD Player is showing.
void* volatile pCDPlayerMenuThis = nullptr;

// Static variables to persist between calls
static DWORD lastPresenceUpdate = 0;

int g_lastTrack;

void UpdateDiscordState() {
	if (pfnDiscord_RunCallbacks) pfnDiscord_RunCallbacks();

	if (g_sessionStartTime == 0) {
		g_sessionStartTime = time(NULL);
	}

	// Determine Current State
	GameState currentState;
	if (pCDPlayerMenuThis != NULL) {
		currentState = STATE_CDPLAYER;
		Log("CD Player State");
	}
	// network build: a multiplayer session is in progress (checked first, so its lobby counts too)
	else if (addrs->networkPlayer && *addrs->networkPlayer != 0) {
		currentState = STATE_NETWORK;
		Log("Network State (player id 0x%X)", *addrs->networkPlayer);
	}
	// a campaign is being played: its war map or battle map screen exists (see OnMenuConstructed)
	else if (g_campaignScreens > 0 && addrs->humanCampaign) {
		currentState = (*addrs->humanCampaign == 1) ? STATE_HUMAN : STATE_MARTIAN;
		Log("Campaign State: %s (%ld game screens)", currentState == STATE_HUMAN ? "Human" : "Martian", g_campaignScreens);
	}
	// the main menu and the other front end screens
	else {
		currentState = STATE_MENU;
		Log("Main Menu State");
	}

	// Logic: Update if State changes, Track changes, Discord (re)connected, OR 16s have passed
	bool stateChanged = (currentState != g_lastGameState) || g_forcePresenceRefresh;
	g_forcePresenceRefresh = false;
	bool trackChanged = (currentState == STATE_CDPLAYER && currentTrack != g_lastTrack);
	bool rateLimitExpired = (GetTickCount() - lastPresenceUpdate > 16000);

	// Only attempt an update if we are initialized
	if (g_discordInitialized && (stateChanged || trackChanged || rateLimitExpired)) {
		RefreshPresence(currentState);

		g_lastGameState = currentState;
		g_lastTrack = currentTrack;
		lastPresenceUpdate = GetTickCount();

		Log("Discord Presence Updated. State: %d, Track: %d", currentState, currentTrack);
	}
}

// --- Helper Functions ---
bool IsDiscordRunning() {
	bool running = false;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapshot, &pe)) {
			do {
				if (_stricmp(pe.szExeFile, "Discord.exe") == 0) {
					running = true;
					break;
				}
			} while (Process32Next(hSnapshot, &pe));
		}
		CloseHandle(hSnapshot);
	}
	return running;
}

static void __cdecl OnDiscordReady(const DiscordUser* user) {
	Log("Discord: connected as %s", (user && user->username) ? user->username : "?");
	g_forcePresenceRefresh = true;
}
static void __cdecl OnDiscordDisconnected(int errorCode, const char* message) {
	Log("Discord: disconnected (%d) %s", errorCode, message ? message : "");
}
static void __cdecl OnDiscordErrored(int errorCode, const char* message) {
	Log("Discord: error (%d) %s", errorCode, message ? message : ""); // e.g. a rejected presence update
}
static DiscordEventHandlers g_discordHandlers = { OnDiscordReady, OnDiscordDisconnected, OnDiscordErrored, nullptr, nullptr, nullptr };

void InitDiscordRPC() {
	if (g_discordInitialized) return;
	if (!IsDiscordRunning()) return;

	hDiscordDLL = LoadLibraryA("discord-rpc.dll");
	if (!hDiscordDLL) return;

	pfnDiscord_Initialize = (Discord_Initialize_t)GetProcAddress(hDiscordDLL, "Discord_Initialize");
	pfnDiscord_UpdatePresence = (Discord_UpdatePresence_t)GetProcAddress(hDiscordDLL, "Discord_UpdatePresence");
	pfnDiscord_Shutdown = (Discord_Shutdown_t)GetProcAddress(hDiscordDLL, "Discord_Shutdown");
	pfnDiscord_RunCallbacks = (Discord_RunCallbacks_t)GetProcAddress(hDiscordDLL, "Discord_RunCallbacks");

	if (!pfnDiscord_Initialize || !pfnDiscord_UpdatePresence || !pfnDiscord_Shutdown || !pfnDiscord_RunCallbacks) {
		FreeLibrary(hDiscordDLL);
		hDiscordDLL = NULL;
		pfnDiscord_Initialize = nullptr;
		pfnDiscord_UpdatePresence = nullptr;
		pfnDiscord_Shutdown = nullptr;
		pfnDiscord_RunCallbacks = nullptr;
		return;
	}

	// Application ID
	pfnDiscord_Initialize("1527400154154402022", &g_discordHandlers, 1, nullptr);
	Log("Discord: initialised");

	DiscordRichPresence presence = {};
	presence.startTimestamp = (int64_t)std::time(nullptr);

	pfnDiscord_UpdatePresence(&presence);
	g_discordInitialized = true;

	currentLanguage = DetectLanguage(); // language.txt, then the size of credits.png
}

void ShutdownDiscordRPC() {
	if (pfnDiscord_Shutdown) pfnDiscord_Shutdown();
	if (hDiscordDLL) {
		FreeLibrary(hDiscordDLL);
		hDiscordDLL = NULL;
	}
}

extern "C" DLLEXPORT MMRESULT WINAPI _imeKillEvent(UINT uTimerID) { return timeKillEvent(uTimerID); }

extern "C" DLLEXPORT MMRESULT WINAPI _imeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent) { return timeSetEvent(uDelay, uResolution, fptc, dwUser, fuEvent); }

extern "C" DLLEXPORT DWORD WINAPI _imeGetTime(void) { return timeGetTime(); }

// networking executable specific variables
bool seekAfterOpen = false;
DWORD lastPlayTime = 0;

HWND notifyWindow = NULL;
DWORD notifyDeviceID = FAKE_CD_ID;
bool notifyPending = false;

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
//
// VANILLA: sub_4081D0, vtable off_4A95D0 slot +0x1C. NETWORK: sub_4B0510, vtable off_51B1E0
// slot +0x28 (the debug-style network build has a different vtable layout); it handles the same
// 0x3B9 and 0x111 messages and the 0x80D3 button group, confirmed from its disassembly.
#define CD_TRANSPORT_BUTTON_GROUP 0x80D3
#define CD_BUTTON_STOP 2

typedef int(__thiscall* CDPlayerOnMessage_t)(void* pThis, UINT uMsg, UINT wParam, UINT lParam);
CDPlayerOnMessage_t CDPlayerOnMessage = nullptr; // only set once its signature has been verified

void ForceStopButtonPress()
{
	void* menu = pCDPlayerMenuThis;
	if (!menu) {
		Log("ForceStopButtonPress: pCDPlayerMenuThis not set, skipping");
		return;
	}
	if (!CDPlayerOnMessage) {
		Log("ForceStopButtonPress: CDPlayerOnMessage not verified for this build, skipping");
		return;
	}
	Log("ForceStopButtonPress: invoking CDPlayerOnMessage(this=0x%p, WM_COMMAND, group=0x%X, STOP)", menu, CD_TRANSPORT_BUTTON_GROUP);
	CDPlayerOnMessage(menu, 0x111 /* WM_COMMAND */, CD_TRANSPORT_BUTTON_GROUP, CD_BUTTON_STOP);
}

// === CD Player menu constructor / destructor hooks ===
//
// The CD Player menu object is heap-allocated fresh each time the menu opens, so there is no static
// pointer to it. Instead the constructor is hooked to capture ECX ("this" under __thiscall) and the
// destructor is hooked to clear it again. Both are ordinary inline hooks: the overwritten prologue
// bytes are copied into a trampoline followed by a jmp back, and the live function starts with a jmp
// to our stub. Each target's bytes are verified first, so an unexpected build is left untouched.
//
// Both destructors are referenced only once in their executables (slot 0 of the CD Player vtable),
// the identity check in the stub is kept as a safety net anyway.

BYTE* g_ctorTrampoline = nullptr;
BYTE* g_dtorTrampoline = nullptr;

void StopAudio(); // forward declaration

// Builds a trampoline for the first hookLen bytes of target and redirects target to stub.
// relCallAt is the offset of a relative CALL inside the copied bytes that must be re-aimed (-1 for none).
static BYTE* InstallJmpHook(DWORD targetAddress, const BYTE* sig, int sigLen, int hookLen, void* stub, int relCallAt, const char* name)
{
	if (!BytesMatch(targetAddress, sig, sigLen)) {
		Log("%s: bytes at 0x%08X don't match the %s build, hook skipped", name, targetAddress, GameBuildName(gameBuild));
		return nullptr;
	}
	BYTE* target = (BYTE*)targetAddress;
	BYTE* trampoline = (BYTE*)VirtualAlloc(NULL, hookLen + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!trampoline) return nullptr;
	memcpy(trampoline, target, hookLen);
	if (relCallAt >= 0) {
		// the copied CALL's displacement was computed for the original address
		DWORD originalRel32;
		memcpy(&originalRel32, target + relCallAt + 1, 4);
		DWORD destination = (DWORD)(target + relCallAt + 5) + originalRel32;
		DWORD newRel32 = destination - (DWORD)(trampoline + relCallAt + 5);
		memcpy(trampoline + relCallAt + 1, &newRel32, 4);
	}
	trampoline[hookLen] = 0xE9; // jmp rel32 back to the rest of the original function
	DWORD back = (DWORD)(target + hookLen) - (DWORD)(trampoline + hookLen + 5);
	memcpy(trampoline + hookLen + 1, &back, 4);
	FlushInstructionCache(GetCurrentProcess(), trampoline, hookLen + 5);

	DWORD oldProtect;
	if (!VirtualProtect(target, hookLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		VirtualFree(trampoline, 0, MEM_RELEASE);
		return nullptr;
	}
	target[0] = 0xE9;
	DWORD rel = (DWORD)stub - (DWORD)(target + 5);
	memcpy(target + 1, &rel, 4);
	for (int i = 5; i < hookLen; i++) target[i] = 0x90;
	VirtualProtect(target, hookLen, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), target, hookLen);
	Log("%s: hooked %s build at 0x%08X", name, GameBuildName(gameBuild), targetAddress);
	return trampoline;
}

void __declspec(naked) CDPlayerMenuCtor_HookStub()
{
	__asm {
		mov pCDPlayerMenuThis, ecx      // ecx = "this" for the CD Player menu object being constructed
		jmp dword ptr [g_ctorTrampoline] // resume the original prologue exactly as if nothing happened
	}
}

// Called from the destructor stub with every register saved. The network build kept playing music after
// leaving the CD Player, so its music is stopped here; vanilla stops it itself.
void OnCDPlayerMenuDestroyed()
{
	pCDPlayerMenuThis = nullptr;
	if (isNetworkVersion) StopAudio();
}

// Runs before the original destructor, which then runs in full (including its retn 4) via the trampoline.
void __declspec(naked) CDPlayerMenuDtor_HookStub()
{
	__asm {
		cmp ecx, pCDPlayerMenuThis
		jne skip
		pushad
		pushfd
		call OnCDPlayerMenuDestroyed
		popfd
		popad
	skip:
		jmp dword ptr [g_dtorTrampoline]
	}
}

void InstallCDPlayerMenuHook()
{
	if (!addrs->cdCtor) return;
	g_ctorTrampoline = InstallJmpHook(addrs->cdCtor, addrs->cdCtorSig, addrs->cdCtorSigLen, addrs->cdCtorHookLen,
		(void*)&CDPlayerMenuCtor_HookStub, -1, "CD Player menu constructor");
	if (g_ctorTrampoline && BytesMatch(addrs->cdOnMessage, addrs->cdOnMessageSig, addrs->cdOnMessageSigLen)) {
		CDPlayerOnMessage = (CDPlayerOnMessage_t)addrs->cdOnMessage;
	}
}

void InstallCDPlayerMenuDtorHook()
{
	if (!addrs->cdDtor || !g_ctorTrampoline) return; // no point clearing a pointer that is never set
	g_dtorTrampoline = InstallJmpHook(addrs->cdDtor, addrs->cdDtorSig, addrs->cdDtorSigLen, addrs->cdDtorHookLen,
		(void*)&CDPlayerMenuDtor_HookStub, addrs->cdDtorRelCallAt, "CD Player menu destructor");
}

// === Game screen tracking ===
// Every screen the game shows is built on one menu base class whose constructor takes the screen's id. The war map
// ("Warmap menu", 0x80B5) and battle map ("Battlemap menu", 0x80B8) are screens too, so a campaign is being played
// exactly while one of them exists. Same ids in both builds. Screens are only created and destroyed on the game thread.
#define SCREEN_ID_WARMAP 0x80B5
#define SCREEN_ID_BATTLEMAP 0x80B8
static void* g_campaignScreenObjects[8] = {};
volatile LONG g_campaignScreens = 0;
BYTE* g_menuCtorTrampoline = nullptr;
BYTE* g_menuDtorTrampoline = nullptr;

void __cdecl OnMenuConstructed(void* menu, DWORD screenId)
{
	if (screenId != SCREEN_ID_WARMAP && screenId != SCREEN_ID_BATTLEMAP) return;
	for (void*& slot : g_campaignScreenObjects) {
		if (slot == nullptr) {
			slot = menu;
			InterlockedIncrement(&g_campaignScreens);
			break;
		}
	}
	Log("Campaign screen 0x%X created (%ld open)", screenId, g_campaignScreens);
}

void __cdecl OnMenuDestroyed(void* menu)
{
	for (void*& slot : g_campaignScreenObjects) {
		if (slot == menu) {
			slot = nullptr;
			InterlockedDecrement(&g_campaignScreens);
			Log("Campaign screen destroyed (%ld open)", g_campaignScreens);
			break;
		}
	}
}

// entry of the base constructor: [esp] = return address, [esp+4] = screen id, ecx = the screen being constructed
void __declspec(naked) MenuCtor_HookStub()
{
	__asm {
		pushad
		pushfd
		push dword ptr [esp + 40]   // the screen id, 36 bytes of saved registers above the return address
		push ecx
		call OnMenuConstructed
		add esp, 8
		popfd
		popad
		jmp dword ptr [g_menuCtorTrampoline]
	}
}

void __declspec(naked) MenuDtor_HookStub()
{
	__asm {
		pushad
		pushfd
		push ecx
		call OnMenuDestroyed
		add esp, 4
		popfd
		popad
		jmp dword ptr [g_menuDtorTrampoline]
	}
}

void InstallScreenHooks()
{
	if (!addrs->menuCtor || !addrs->menuDtor) return;
	// both or neither: a constructor hook without the destructor hook would leave screens counted forever
	if (!BytesMatch(addrs->menuCtor, addrs->menuCtorSig, addrs->menuCtorSigLen) || !BytesMatch(addrs->menuDtor, addrs->menuDtorSig, addrs->menuDtorSigLen)) {
		Log("Game screen hooks skipped: bytes don't match the %s build", GameBuildName(gameBuild));
		return;
	}
	g_menuDtorTrampoline = InstallJmpHook(addrs->menuDtor, addrs->menuDtorSig, addrs->menuDtorSigLen, addrs->menuDtorHookLen,
		(void*)&MenuDtor_HookStub, -1, "Game screen destructor");
	if (g_menuDtorTrampoline) {
		g_menuCtorTrampoline = InstallJmpHook(addrs->menuCtor, addrs->menuCtorSig, addrs->menuCtorSigLen, addrs->menuCtorHookLen,
			(void*)&MenuCtor_HookStub, -1, "Game screen constructor");
	}
}

DWORD WINAPI PresenceThread(LPVOID); // defined after WndProcHook
extern volatile bool g_presenceShutdown;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		if (debug) DeleteFileA("_inmm_log.txt");
		InitializeCriticalSection(&audioLock);
		InitializeCriticalSection(&deviceLock);
		gameBuild = DetectGameBuild();
		isNetworkVersion = (gameBuild == BUILD_NETWORK);
		addrs = gameBuild == BUILD_VANILLA ? &kVanillaAddrs : gameBuild == BUILD_NETWORK ? &kNetworkAddrs : &kNoAddrs;
		Log("Detected %s build", GameBuildName(gameBuild));
		InstallCDPlayerMenuHook();
		InstallCDPlayerMenuDtorHook(); // at load, so the CD Player state can never be left set
		InstallScreenHooks();          // war map / battle map tracking for the campaign Discord states
		// Portable settings: the import table of the main executable is already bound and its own startup code
		// hasn't run yet, so redirecting it here catches every registry read the game makes.
		char gameDir[MAX_PATH], iniPath[MAX_PATH];
		GamePath("", gameDir, sizeof(gameDir));
		GamePath(wow::IniStore::FileName(), iniPath, sizeof(iniPath));
		if (g_settings.Load(iniPath)) {
			int redirected = wow::registry::InstallRedirect(&g_settings, gameDir);
			g_useIni = true; // our own settings use the store even if the import table couldn't be patched
			std::string logSetting;
			if (g_settings.Get("Revived", "Debug Log", logSetting) && atoi(logSetting.c_str()) != 0) debug = true;
			Log("WoWRevived.ini found, %d registry imports redirected", redirected);
		}
		HKEY hKey;
		if (SettingsOpenKey("SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Sound\\Volume", &hKey) == ERROR_SUCCESS) {
			char text[64];
			// volumes are stored as "%f" strings by both builds
			auto ReadVolume = [&](const char* valueName, volatile float* target) {
				if (!target || !SettingsReadText(hKey, valueName, text, sizeof(text))) return;
				float value = (float)atof(text);
				if (value >= 0.0f && value <= 1.0f) *target = value; // ignore anything out of range
			};
			ReadVolume("Master", addrs->masterVol);
			ReadVolume("Ambient", addrs->ambientVol);
			ReadVolume("Speech", addrs->speechVol);
			if (addrs->cdToggle && SettingsReadText(hKey, "CD", text, sizeof(text))) *addrs->cdToggle = atoi(text) ? 1 : 0;
			if (SettingsReadText(hKey, "CD-Focus", text, sizeof(text))) musicFocus = (atoi(text) != 0);
			SettingsClose(hKey);
		}
		// Starting a thread here is allowed, it only begins running once the loader lock has been released.
		HANDLE presenceThread = CreateThread(NULL, 0, PresenceThread, NULL, 0, NULL);
		if (presenceThread) CloseHandle(presenceThread);
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		// Volume Persistence (Master, Ambient, Speech, In-Game CD Music)
		HKEY hKey;
		if (addrs->masterVol && SettingsCreateKey("SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000\\Sound\\Volume", &hKey) == ERROR_SUCCESS) {

			auto WriteVolume = [&](const char* valueName, volatile float* source) {
				if (!source) return;
				float value = *source;
				if (!(value >= 0.0f && value <= 1.0f)) return; // never persist garbage
				char text[32];
				sprintf_s(text, "%f", value);
				SettingsSet(hKey, valueName, REG_SZ, (LPBYTE)text, (DWORD)(strlen(text) + 1));
			};
			WriteVolume("Master", addrs->masterVol);
			WriteVolume("Ambient", addrs->ambientVol);
			WriteVolume("Speech", addrs->speechVol);

			// In-Game CD Music
			DWORD cdState = CurrentCdState() ? 1 : 0;
			SettingsSet(hKey, "CD", REG_DWORD, (const BYTE*)&cdState, sizeof(DWORD));

			SettingsClose(hKey); // saves WoWRevived.ini if anything changed
		}
		// When the process is exiting (lpvReserved != NULL) the other threads are already gone and the OS frees
		// everything, so only tidy up Discord when the DLL is being unloaded on purpose.
		g_presenceShutdown = true;
		if (lpvReserved == NULL) ShutdownDiscordRPC();
	}
	return TRUE;
}

// === Music playback ===
//
// The decoded track is held in memory and streamed to the wave device in small chunks. Refilling is done by
// a dedicated audio thread that waits on the device's event (CALLBACK_EVENT): waveOut functions must not be
// called from a CALLBACK_FUNCTION callback, and the device is never reset or closed while audioLock is held,
// which is what could previously deadlock the game.

#define CHUNK_SIZE 8192  // bytes per chunk - adjust for latency vs responsiveness
#define NUM_BUFFERS 2    // double buffering

WAVEHDR waveHdrs[NUM_BUFFERS] = {};
static char g_chunks[NUM_BUFFERS][CHUNK_SIZE]; // only reused after the previous device has been fully closed
std::vector<char> g_pcm;                       // the whole decoded track
DWORD audioReadPos = 0;

HANDLE g_audioEvent = NULL;
HANDLE g_audioThread = NULL;
volatile bool g_audioThreadExit = false;

HWND gameWindow = NULL;
WNDPROC origWndProc = NULL;

// Custom message used to marshal ForceStopButtonPress() onto the game's own
// main thread. The audio thread is not the game's thread - calling CDPlayerOnMessage
// directly from there crashed the game, almost certainly because that internal handler
// assumes single-threaded access with no locking. PostMessage here is the safe hand-off:
// WndProcHook only ever runs on the same thread as the game's real message loop.
#define WM_APP_FORCE_STOP_PRESS (WM_APP + 0x444)

// Reads a 16-bit PCM WAV file into memory. Returns false (and leaves pcm empty) for anything else.
static bool LoadWav(const char* path, WAVEFORMATEX& wfx, std::vector<char>& pcm)
{
	pcm.clear();
	HMMIO hmmio = mmioOpenA((LPSTR)path, NULL, MMIO_READ | MMIO_ALLOCBUF);
	if (!hmmio) return false;
	bool ok = false;
	MMCKINFO riff = {}, fmt = {}, data = {};
	riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	fmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	data.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(hmmio, &riff, NULL, MMIO_FINDRIFF) == MMSYSERR_NOERROR &&
		mmioDescend(hmmio, &fmt, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR && fmt.cksize >= sizeof(PCMWAVEFORMAT)) {
		ZeroMemory(&wfx, sizeof(wfx));
		LONG want = (LONG)min(fmt.cksize, (DWORD)sizeof(PCMWAVEFORMAT)); // the fmt chunk of a PCM file is 16 bytes
		if (mmioRead(hmmio, (HPSTR)&wfx, want) == want && wfx.wFormatTag == WAVE_FORMAT_PCM && wfx.wBitsPerSample == 16 &&
			wfx.nChannels >= 1 && wfx.nBlockAlign > 0 && wfx.nAvgBytesPerSec > 0) {
			wfx.cbSize = 0;
			mmioAscend(hmmio, &fmt, 0);
			if (mmioDescend(hmmio, &data, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR && data.cksize > 0 && data.cksize < 0x7FFFFFFF) {
				pcm.resize(data.cksize);
				ok = mmioRead(hmmio, pcm.data(), (LONG)data.cksize) == (LONG)data.cksize;
			}
		}
	}
	mmioClose(hmmio, 0);
	if (!ok) pcm.clear();
	return ok;
}

// Copies the next chunk into buffer b with the master volume applied and queues it. Call with audioLock held.
static bool QueueBuffer_Locked(int b)
{
	if (!hWaveOut || audioReadPos >= g_pcm.size()) return false;
	DWORD toWrite = min((DWORD)g_pcm.size() - audioReadPos, (DWORD)CHUNK_SIZE) & ~1u; // whole 16-bit samples
	if (toWrite == 0) return false;
	float vol = CurrentMasterVolume();
	const int16_t* src = (const int16_t*)(g_pcm.data() + audioReadPos);
	int16_t* dst = (int16_t*)waveHdrs[b].lpData;
	DWORD samples = toWrite / sizeof(int16_t);
	for (DWORD i = 0; i < samples; i++) {
		int32_t s = (int32_t)(src[i] * vol);
		if (s > 32767) s = 32767;
		if (s < -32768) s = -32768;
		dst[i] = (int16_t)s;
	}
	audioReadPos += toWrite;
	waveHdrs[b].dwBufferLength = toWrite;
	return waveOutWrite(hWaveOut, &waveHdrs[b], sizeof(WAVEHDR)) == MMSYSERR_NOERROR;
}

static DWORD WINAPI AudioThread(LPVOID)
{
	while (!g_audioThreadExit) {
		WaitForSingleObject(g_audioEvent, 100); // signalled by the device when a buffer finishes
		bool postStop = false;
		EnterCriticalSection(&audioLock);
		if (hWaveOut && !isPaused) {
			// Force-press Stop once elapsed playback time reaches the current track's real length, so the
			// game's own Stop logic (unlike the broken MM_MCINOTIFY path) runs when the track finishes.
			DWORD elapsed = (dwStartTime > 0) ? GetTickCount() - dwStartTime : 0;
			if (!forceStopFired && dwStartTime > 0 && currentTrackLength > 0 && elapsed >= currentTrackLength) {
				forceStopFired = true;
				postStop = true;
				Log("Track finished (%lu ms elapsed / %lu ms length), posting WM_APP_FORCE_STOP_PRESS to main thread", elapsed, currentTrackLength);
			}
			bool anyQueued = false;
			for (int b = 0; b < NUM_BUFFERS; b++) {
				if (waveHdrs[b].dwFlags & WHDR_INQUEUE) anyQueued = true;
				else if ((waveHdrs[b].dwFlags & WHDR_DONE) && QueueBuffer_Locked(b)) anyQueued = true;
			}
			if (!anyQueued && dwStartTime > 0) {
				notifyPending = false;
				dwStartTime = 0;
				totalElapsedBeforePause = 0;
				Log("Track %d finished naturally; reset timer locally (no MM_MCINOTIFY)", currentTrack);
			}
		}
		HWND target = gameWindow;
		LeaveCriticalSection(&audioLock);
		if (postStop && target) PostMessage(target, WM_APP_FORCE_STOP_PRESS, 0, 0);
	}
	return 0;
}

// Started from the game thread on first playback, never from DllMain.
static bool EnsureAudioThread()
{
	if (g_audioThread) return true;
	if (!g_audioEvent) g_audioEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!g_audioEvent) return false;
	g_audioThread = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
	return g_audioThread != NULL;
}

// Takes ownership of the current device and track data. Call with audioLock held, then pass the
// result to TeardownAudio after releasing the lock.
static HWAVEOUT DetachAudio_Locked()
{
	HWAVEOUT detached = hWaveOut;
	hWaveOut = NULL; // the audio thread ignores everything once this is cleared
	std::vector<char>().swap(g_pcm); // free the track
	audioReadPos = 0;
	return detached;
}

// Blocking device teardown. Must not be called with audioLock held. Call with deviceLock held.
static void TeardownAudio(HWAVEOUT device)
{
	if (!device) return;
	waveOutReset(device); // returns every queued buffer
	for (int b = 0; b < NUM_BUFFERS; b++) {
		if (waveHdrs[b].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(device, &waveHdrs[b], sizeof(WAVEHDR));
		waveHdrs[b] = {};
	}
	waveOutClose(device);
}

void StopAudio() {
	EnterCriticalSection(&deviceLock);
	EnterCriticalSection(&audioLock);
	HWAVEOUT device = DetachAudio_Locked();
	LeaveCriticalSection(&audioLock);
	TeardownAudio(device);
	LeaveCriticalSection(&deviceLock);
}

static void PlayWav(const char* path) {
	StopAudio();

	WAVEFORMATEX wfx = {};
	std::vector<char> pcm;
	if (!LoadWav(path, wfx, pcm)) { // read the file without holding any lock
		Log("PlayWav: %s is missing or not 16-bit PCM", path);
		EnterCriticalSection(&audioLock);
		dwStartTime = 0;
		currentTrackLength = 0;
		LeaveCriticalSection(&audioLock);
		return;
	}
	if (!EnsureAudioThread()) return;

	EnterCriticalSection(&deviceLock);
	EnterCriticalSection(&audioLock);
	HWAVEOUT stale = DetachAudio_Locked(); // in case another play started while the file was loading
	LeaveCriticalSection(&audioLock);
	TeardownAudio(stale);

	EnterCriticalSection(&audioLock);
	HWAVEOUT device = NULL;
	if (waveOutOpen(&device, WAVE_MAPPER, &wfx, (DWORD_PTR)g_audioEvent, 0, CALLBACK_EVENT) == MMSYSERR_NOERROR) {
		// Older versions scaled the whole process's audio session with waveOutSetVolume (see MCI_STATUS), and Windows
		// can remember that level. Put the session back to full once, the game and the music scale themselves.
		static bool sessionVolumeReset = false;
		if (!sessionVolumeReset) {
			waveOutSetVolume(device, 0xFFFFFFFF);
			sessionVolumeReset = true;
		}
		g_pcm.swap(pcm);
		audioReadPos = 0;
		currentTrackLength = (uint32_t)((double)g_pcm.size() / wfx.nAvgBytesPerSec * 1000.0);
		hWaveOut = device;
		for (int b = 0; b < NUM_BUFFERS; b++) {
			waveHdrs[b] = {};
			waveHdrs[b].lpData = g_chunks[b];
			waveHdrs[b].dwBufferLength = CHUNK_SIZE;
			waveOutPrepareHeader(device, &waveHdrs[b], sizeof(WAVEHDR));
			QueueBuffer_Locked(b); // prime both buffers
		}
		dwStartTime = GetTickCount();
		totalElapsedBeforePause = 0;
		isPaused = false;
	}
	else {
		Log("PlayWav: waveOutOpen failed for %s", path);
		dwStartTime = 0;
		currentTrackLength = 0;
	}
	LeaveCriticalSection(&audioLock);
	LeaveCriticalSection(&deviceLock);
}

// lastFocusEventTick is written only inside audioLock (in WndProcHook) and read
// only inside audioLock (in _ciSendCommandA MCI_STOP), so no separate guard needed.
DWORD lastFocusEventTick = 0;

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (HideSystemCursorInClient(hwnd, msg, lParam)) return TRUE; // stops both cursors showing after alt-tab, see GameCommon.h
	if (msg == WM_APP_FORCE_STOP_PRESS) {
		ForceStopButtonPress();
		return 0;
	}
	// --- ESCAPE KEY MUSIC LATCH ---
	if (msg == WM_KEYDOWN && wParam == VK_ESCAPE && pCDPlayerMenuThis == NULL) {
		EnterCriticalSection(&audioLock);
		lastEscapeTick = GetTickCount();
		Log("HOOK: Escape key down registered at tick %lu", lastEscapeTick);
		LeaveCriticalSection(&audioLock);
	}
	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if (!CurrentCdState() && pCDPlayerMenuThis == NULL) {
		if (hWaveOut) StopAudio(); // unlocked read is only a hint, StopAudio re-checks under the lock
		return CallWindowProc(origWndProc, hwnd, msg, wParam, lParam);
	}
	if (msg == WM_ACTIVATEAPP) {
		resumingFocus = true;
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
			// Only restart if we still have a valid handle - PlayWav may have
			// raced ahead and already opened a new one.
			if (hWaveOut && isPaused && !musicFocus) {
				isPaused = false;
				dwStartTime = GetTickCount() - totalElapsedBeforePause;
				waveOutRestart(hWaveOut);
			}
		}
		LeaveCriticalSection(&audioLock);
		resumingFocus = false;
	}
	// --- MOUSE CLICK MUSIC LATCH ---
	if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
		EnterCriticalSection(&audioLock);
		lastClickTick = GetTickCount();
		LeaveCriticalSection(&audioLock);
	}
	return CallWindowProc(origWndProc, hwnd, msg, wParam, lParam);
}

// The game's main window: a visible, unowned top-level window of this process that isn't one of the overlays
// Smackw32.dll creates. Found this way rather than by title, which is translated in the regional releases.
static BOOL CALLBACK FindGameWindow(HWND hwnd, LPARAM lParam)
{
	DWORD processId = 0;
	GetWindowThreadProcessId(hwnd, &processId);
	if (processId != GetCurrentProcessId() || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
	if (GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
	char className[64] = {};
	GetClassNameA(hwnd, className, sizeof(className));
	if (strcmp(className, "SmackOverlay") == 0 || strcmp(className, "WoWCreditsOverlay") == 0) return TRUE;
	*(HWND*)lParam = hwnd;
	return FALSE; // stop at the first match
}

static bool HookGameWindow()
{
	HWND found = NULL;
	EnumWindows(FindGameWindow, (LPARAM)&found);
	if (!found) return false;
	char title[256] = {};
	GetWindowTextA(found, title, sizeof(title));
	EnterCriticalSection(&audioLock);
	if (!gameWindow) {
		// WndProcHook passes messages on to origWndProc, so it has to be valid before the hook can receive anything
		origWndProc = (WNDPROC)GetWindowLongPtr(found, GWLP_WNDPROC);
		WNDPROC previous = (WNDPROC)SetWindowLongPtr(found, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
		if (previous) origWndProc = previous;
		gameWindow = found;
		Log("Hooked game window \"%s\"", title);
	}
	LeaveCriticalSection(&audioLock);
	return true;
}

// Hooks the game window as soon as it exists, starts Discord rich presence (retrying while Discord isn't running,
// so starting Discord after the game works) and keeps the presence up to date every couple of seconds. None of this
// depends on the game opening its music device or on window messages arriving.
volatile bool g_presenceShutdown = false;
DWORD WINAPI PresenceThread(LPVOID)
{
	DWORD lastDiscordAttempt = 0;
	bool attempted = false;
	while (!g_presenceShutdown) {
		if (!gameWindow) HookGameWindow();
		if (gameWindow && !g_discordInitialized && (!attempted || GetTickCount() - lastDiscordAttempt > 15000)) {
			attempted = true;
			lastDiscordAttempt = GetTickCount();
			InitDiscordRPC();
		}
		if (g_discordInitialized) UpdateDiscordState();
		Sleep(gameWindow ? 2000 : 250);
	}
	return 0;
}

extern "C" DLLEXPORT MCIERROR WINAPI _ciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
	Log("IN: uMsg=0x%X fdwCommand=0x%llX", uMsg, (unsigned long long)fdwCommand);

	// --- IN-GAME MUSIC OVERRIDE ---
	// If the user disabled music in the UI, kill any active audio and ignore all MCI spam.
	// EXCEPTION: If the CD Player menu is currently open, allow MCI commands to pass through.
	if (!CurrentCdState() && pCDPlayerMenuThis == NULL) {
		if (hWaveOut) StopAudio();
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
		bool isCDPlayerOpen = (pCDPlayerMenuThis != NULL);
		if (currentTick - lastClickTick < 250 && !isCDPlayerOpen && CurrentCdState()) {
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
		notifyPending = false;
		LeaveCriticalSection(&audioLock);
		StopAudio(); // tears the device down without holding audioLock
		Log("MCI_STOP/MCI_CLOSE");
		return 0;
	}

	// 3. SEEK
	if (uMsg == MCI_SEEK) {
		LPMCI_SEEK_PARMS lpSeek = (LPMCI_SEEK_PARMS)dwParam;
		if (!lpSeek) return 0;
		// currentTrack is only written here and read in MCI_PLAY/STATUS - both on
		// the same game thread - so no lock needed for this assignment.
		currentTrack = (int)lpSeek->dwTo;
		if (isNetworkVersion) seekAfterOpen = true;
		Log("MCI_SEEK to: %d", (int)lpSeek->dwTo);
		return 0;
	}

	// 4. OPEN
	if (uMsg == MCI_OPEN) {
		// Discord and the window hook are set up by PresenceThread, which doesn't depend on the game opening the
		// music device (it doesn't while in-game music is off, except in the CD Player).
		EnterCriticalSection(&audioLock);
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
		if (resumingFocus && !musicFocus && isNetworkVersion) return 0;
		EnterCriticalSection(&audioLock);
		// Record whether the caller wants MM_MCINOTIFY when this track ends.
		// MCI_PLAY_PARMS.dwCallback holds the window handle when MCI_NOTIFY is set.
		LPMCI_PLAY_PARMS lpPlay = (LPMCI_PLAY_PARMS)dwParam;
		if ((fdwCommand & MCI_NOTIFY) && lpPlay) {
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
		forceStopFired = false; // allow the end-of-track Stop press to fire again for this track

		if (isNetworkVersion) {
			lastPlayTime = GetTickCount();
			Log("MCI_PLAY Tick: %d", lastPlayTime);
		}

		// only play track values within range to prevent seeking to tracks that dont exist
		if (currentTrack < 2 || currentTrack > 5) {
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		if (isPaused && hWaveOut) {
			// Resume: hWaveOut is still valid (we hold the lock, nobody can close it)
			isPaused = false;
			dwStartTime = GetTickCount() - totalElapsedBeforePause;
			waveOutRestart(hWaveOut);
			LeaveCriticalSection(&audioLock);
			return 0;
		}
		isPaused = false;
		int track = currentTrack;
		LeaveCriticalSection(&audioLock);

		char path[MAX_PATH];
		wsprintfA(path, "Music%s\\%02d Track%02d.wav", UseMartianAssets() ? "-Martian" : "-Human", track, track);
		PlayWav(path); // loads the file and opens the device without holding audioLock
		return 0;
	}

	// 6. STATUS
	// Master volume is applied to the music samples as they are queued (QueueBuffer_Locked). It is no longer also
	// applied with waveOutSetVolume: on Vista and later that sets the volume of the whole process's audio session,
	// so it applied master volume a second time to the music and to every game sound effect.
	if (uMsg == MCI_STATUS) {
		LPMCI_STATUS_PARMS lpStatus = (LPMCI_STATUS_PARMS)dwParam;
		if (!lpStatus) return 0;
		if (lpStatus->dwItem == MCI_STATUS_POSITION) {
			// Read dwStartTime and currentTrackLength inside the lock so a concurrent
			// MCI_STOP can't zero dwStartTime mid-calculation and produce a wrapped
			// elapsed value.
			EnterCriticalSection(&audioLock);
			DWORD elapsed = (dwStartTime > 0) ? (isPaused ? totalElapsedBeforePause : GetTickCount() - dwStartTime) : 0;
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
		return 0;
	}

	// 7. PAUSE
	if (uMsg == MCI_PAUSE) {
		EnterCriticalSection(&audioLock);
		if (!isPaused && hWaveOut) {
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
