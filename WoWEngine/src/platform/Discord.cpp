#include "platform/Discord.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdint>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace wow
{
    namespace
    {
        // discord-rpc's own structures (discord_rpc.h)
        struct DiscordRichPresence
        {
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
        struct DiscordUser
        {
            const char* userId;
            const char* username;
            const char* discriminator;
            const char* avatar;
        };
#ifdef _WIN32
#define WOW_DISCORD_CALL __cdecl
#else
#define WOW_DISCORD_CALL
#endif
        struct DiscordEventHandlers
        {
            void (WOW_DISCORD_CALL* ready)(const DiscordUser* request);
            void (WOW_DISCORD_CALL* disconnected)(int errorCode, const char* message);
            void (WOW_DISCORD_CALL* errored)(int errorCode, const char* message);
            void (WOW_DISCORD_CALL* joinGame)(const char* joinSecret);
            void (WOW_DISCORD_CALL* spectateGame)(const char* spectateSecret);
            void (WOW_DISCORD_CALL* joinRequest)(const DiscordUser* request);
        };
        using InitializeFn = void (WOW_DISCORD_CALL*)(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId);
        using UpdatePresenceFn = void (WOW_DISCORD_CALL*)(const DiscordRichPresence* presence);
        using RunCallbacksFn = void (WOW_DISCORD_CALL*)(void);
        using ShutdownFn = void (WOW_DISCORD_CALL*)(void);

        InitializeFn initialize = nullptr;
        UpdatePresenceFn updatePresence = nullptr;
        RunCallbacksFn runCallbacks = nullptr;
        ShutdownFn shutdown = nullptr;
        bool refresh = false;       // Discord (re)connected: the presence is sent again

        void WOW_DISCORD_CALL OnReady(const DiscordUser* user)
        {
            SDL_Log("Discord: connected%s%s", user && user->username ? " as " : "", user && user->username ? user->username : "");
            refresh = true;
        }
        void WOW_DISCORD_CALL OnDisconnected(int code, const char* message) { SDL_Log("Discord: disconnected (%d: %s)", code, message ? message : ""); }
        void WOW_DISCORD_CALL OnErrored(int code, const char* message) { SDL_Log("Discord: error %d: %s", code, message ? message : ""); }
        DiscordEventHandlers handlers = { OnReady, OnDisconnected, OnErrored, nullptr, nullptr, nullptr };

        // WoWRevived's application
        constexpr const char* ApplicationId = "1527400154154402022";

        enum { English, French, German, Italian, Spanish, Languages };

        // The texts are WoWRevived's (UTF-8, as Discord expects; the sources are UTF-8).
        struct Text { const char* state; const char* details; const char* large; };
        constexpr Text MenuText[Languages] = {
            { "Deciding the Fate of Earth", "Main Menu", "Jeff Wayne's 'The War Of The Worlds'" },
            { "Décision du destin de la Terre", "Menu principal", "Jeff Wayne's 'La Guerre des mondes'" },
            { "Das Schicksal der Erde entscheiden", "Hauptmenü", "Jeff Wayne's 'Der Krieg der Welten'" },
            { "Decidere il destino della Terra", "Menu principale", "Jeff Wayne's 'La guerra dei mondi'" },
            { "Decidiendo el destino de la Tierra", "Menú principal", "Jeff Wayne's 'La Guerra de los Mundos'" },
        };
        constexpr Text HumanText[Languages] = {
            { "Defending Earth", "Human Campaign", "The chances of anything coming from Mars..." },
            { "Défense de la Terre", "Campagne humaine", "Les chances que quelque chose vienne de Mars..." },
            { "Verteidigung der Erde", "Menschen-Kampagne", "Die Chancen, dass etwas vom Mars kommt..." },
            { "Difesa della Terra", "Campagna umana", "Le probabilità che qualcosa arrivi da Marte..." },
            { "Defendiendo la Tierra", "Campaña humana", "Las posibilidades de que algo venga de Marte..." },
        };
        constexpr Text MartianText[Languages] = {
            { "Invading Earth", "Martian Campaign", "No one would have believed..." },
            { "Invasion de la Terre", "Campagne martienne", "Personne n'aurait cru..." },
            { "Invasion der Erde", "Marsianer-Kampagne", "Niemand hätte geglaubt..." },
            { "Invasione della Terra", "Campagna marziana", "Nessuno avrebbe mai creduto..." },
            { "Invadiendo la Tierra", "Campaña marciana", "Nadie habría creído..." },
        };
        constexpr Text NetworkText[Languages] = {
            { "Battling for Dominance", "Multiplayer Skirmish", "Commanding forces online..." },
            { "Bataille pour la domination", "Escarmouche multijoueur", "Commandement des forces en ligne..." },
            { "Kampf um die Vorherrschaft", "Multiplayer-Gefecht", "Befehl über Truppen online..." },
            { "Battaglia per il dominio", "Schermaglia multigiocatore", "Comando delle forze online..." },
            { "Batalla por el dominio", "Escaramuza multijugador", "Comandando fuerzas en línea..." },
        };
        constexpr Text CdText[Languages] = {
            { "Listening to the Score", "CD Player", nullptr },
            { "Écoute de la bande originale", "Lecteur CD", nullptr },
            { "Hören des Soundtracks", "CD-Spieler", nullptr },
            { "Ascolto della colonna sonora", "Lettore CD", nullptr },
            { "Escuchando la banda sonora", "Reproductor de CD", nullptr },
        };
        constexpr const char* Sides[Languages][2] = {
            { "Human", "Martian" }, { "Humain", "Martien" }, { "Mensch", "Marsianer" }, { "Umano", "Marziano" }, { "Humano", "Marciano" },
        };
        // by language, then soundtrack (0 Human, 1 Martian), then CD tracks 2 to 5
        constexpr const char* TrackNames[Languages][2][4] = {
            { { "Dead London", "The Spirit of Man", "Brave New World", "The Eve of the War (Human)" },
              { "The Red Weed", "Horsell Common", "The Eve of the War (Martian)", "The Fighting Machine" } },
            { { "Londres Mort", "L'Esprit de l'Homme", "Un Monde Nouveau", "La Veille de la Guerre (Humain)" },
              { "L'Herbe Rouge", "Horsell Common", "La Veille de la Guerre (Martien)", "La Machine de Combat" } },
            { { "Totes London", "Der Geist des Menschen", "Schöne Neue Welt", "Der Vorabend des Krieges (Mensch)" },
              { "Das Rote Unkraut", "Horsell Common", "Der Vorabend des Krieges (Marsianisch)", "Die Kampfmaschine" } },
            { { "Londra Morta", "Lo Spirito dell'Uomo", "Un Mondo Nuovo", "La Vigilia della Guerra (Umano)" },
              { "L'Erba Rossa", "Horsell Common", "La Vigilia della Guerra (Marziano)", "La Macchina da Combattimento" } },
            { { "Londres Muerta", "El Espíritu del Hombre", "Un Mundo Nuevo", "La Víspera de la Guerra (Humano)" },
              { "La Hierba Roja", "Horsell Common", "La Víspera de la Guerra (Marciano)", "La Máquina de Combate" } },
        };
        // the image keys for CD tracks 2 to 5 (cd_bd5 is in both soundtracks)
        constexpr const char* TrackImages[2][4] = {
            { "cd_bd3", "cd_bd7", "cd_bd2", "cd_bd5" },     // Human
            { "cd_bd6", "cd_bd5", "cd_bd4", "cd_bd1" },     // Martian
        };

#ifdef _WIN32
        HMODULE library = nullptr;
#endif
    }

    Discord::~Discord() { Stop(); }

    void Discord::Start(const std::string& name)
    {
        if (started) return;
        std::string lower;
        for (char c : name) lower += char(std::tolower(static_cast<unsigned char>(c)));
        language = lower.rfind("fr", 0) == 0 ? French : lower.rfind("ge", 0) == 0 || lower.rfind("de", 0) == 0 ? German :
                   lower.rfind("it", 0) == 0 ? Italian : lower.rfind("sp", 0) == 0 || lower.rfind("es", 0) == 0 ? Spanish : English;
#ifdef _WIN32
        // next to the program first, then wherever Windows finds it
        std::string beside = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + "discord-rpc.dll";
        library = LoadLibraryA(beside.c_str());
        if (!library) library = LoadLibraryA("discord-rpc.dll");
        if (!library)
        {
            // 193 (ERROR_BAD_EXE_FORMAT): the DLL is there but built for another processor (a 32-bit DLL and a
            // 64-bit engine, or the other way round); 126: not found
            const DWORD why = GetLastError();
            if (why == 193) SDL_Log("Discord: discord-rpc.dll is built for another processor than the engine (which is %d-bit), so no rich presence", int(sizeof(void*) * 8));
            else SDL_Log("Discord: discord-rpc.dll wasn't found, so no rich presence (error %lu)", static_cast<unsigned long>(why));
            return;
        }
        initialize = reinterpret_cast<InitializeFn>(GetProcAddress(library, "Discord_Initialize"));
        updatePresence = reinterpret_cast<UpdatePresenceFn>(GetProcAddress(library, "Discord_UpdatePresence"));
        runCallbacks = reinterpret_cast<RunCallbacksFn>(GetProcAddress(library, "Discord_RunCallbacks"));
        shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(library, "Discord_Shutdown"));
        if (!initialize || !updatePresence || !runCallbacks || !shutdown)
        {
            SDL_Log("Discord: discord-rpc.dll doesn't have the functions needed");
            FreeLibrary(library);
            library = nullptr;
            initialize = nullptr; updatePresence = nullptr; runCallbacks = nullptr; shutdown = nullptr;
            return;
        }
        initialize(ApplicationId, &handlers, 1, nullptr);
        sessionStart = static_cast<long long>(std::time(nullptr));
        started = true;
        SDL_Log("Discord: rich presence started");
#else
        (void)handlers;
        (void)ApplicationId;
#endif
    }

    void Discord::Stop()
    {
        if (!started) return;
        started = false;
        if (shutdown) shutdown();
#ifdef _WIN32
        if (library) FreeLibrary(library);
        library = nullptr;
#endif
        initialize = nullptr; updatePresence = nullptr; runCallbacks = nullptr; shutdown = nullptr;
    }

    // WoWRevived's rule: the presence is sent when the state changes, when the CD player's track changes, when
    // Discord (re)connects, and every 16 seconds otherwise.
    void Discord::Update(State state, int track, bool human)
    {
        if (!started) return;
        const unsigned long long now = SDL_GetTicks();
        if (now - lastCallbacks >= 500) { lastCallbacks = now; if (runCallbacks) runCallbacks(); }
        const bool changed = int(state) != lastState || refresh;
        const bool trackChanged = state == State::CdPlayer && track != lastTrack;
        if (!changed && !trackChanged && now - lastSent <= 16000) return;
        refresh = false;
        Send(state, track, human);
        lastState = int(state);
        lastTrack = track;
        lastSent = now;
    }

    void Discord::Send(State state, int track, bool human)
    {
        DiscordRichPresence presence{};
        presence.startTimestamp = sessionStart;
        const int l = language >= 0 && language < Languages ? language : English;
        const Text* text = &MenuText[l];
        switch (state)
        {
        case State::Menu: presence.largeImageKey = "cover"; break;
        case State::Human: presence.largeImageKey = "human"; text = &HumanText[l]; break;
        case State::Martian: presence.largeImageKey = "martian"; text = &MartianText[l]; break;
        case State::Network:
            presence.largeImageKey = "network";
            text = &NetworkText[l];
            // network games also show which side is being played
            presence.smallImageKey = human ? "human" : "martian";
            presence.smallImageText = Sides[l][human ? 0 : 1];
            break;
        case State::CdPlayer:
            text = &CdText[l];
            presence.largeImageKey = track >= 2 && track <= 5 ? TrackImages[human ? 0 : 1][track - 2] : "cd_bd1";
            presence.largeImageText = track >= 2 && track <= 5 ? TrackNames[l][human ? 0 : 1][track - 2] : "Unknown Track";
            break;
        }
        presence.state = text->state;
        presence.details = text->details;
        if (text->large) presence.largeImageText = text->large;
        if (updatePresence) updatePresence(&presence);
    }
}
