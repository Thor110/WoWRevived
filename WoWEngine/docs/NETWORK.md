# The network game: what the original did, and the plan

PLAN.md A2 (the players and their commands) and A9 (the network game), planned at update 277; the owner's answers
added and the task list drawn out in full at update 278. What the original
does is read out in two notes beside this one, with addresses: `network/ORIGINAL-NETWORK.md` (DeePlay, the player
objects, the command records, the menus) and `network/ORIGINAL-CLOCK.md` (the timer, the war map's clock, time in a
network game). Both were read from the retail listing; *provisional* marks what wasn't traced to the end.

## What the original does, in short

- **Two players, lockstep.** One Human install hosts, one Martian install joins (DirectPlay 3A, at most two
  players). Both machines run the whole game; only the players' **command records** travel, one packet a game tick
  each way. Nothing else is sent: no world, no start file, no checksum, no cycle number that's checked.
- **The players** (WPlayer 0x438B20): the local player, the computer player (0x442890) and the network player
  (0x445EA0). The opponent (dword_4E9B78) is the computer player in a single-player game and the network player in a
  network game. Each has a 1 KB buffer of records for the tick.
- **The records**: a four-byte header (kind, kind, code, length) and up to 28 bytes. Selection (clear, select,
  deselect), cancel, the war map's send / move (8, 0x18), time on / off (0xA / 0xB, 0xC / 0xD), build orders (0x16 /
  0x17), and the battle map's orders through the selected group (move, force fire, keys, group orders). They're
  carried out by one routine (0x438C90) on every machine.
- **The tick** (0x403680): frame counter up; the opponent's packet in; **if it hasn't come and ours is sent, the
  tick stops there** (the wait); else ours goes out, ours is carried out, then the opponent's, then the game's
  message queue and the screen.
- **Time**: 33 ms updates from a multimedia timer, one update per tick, none made up after a stall. The war map's
  clock moves speed / Game Frequency game hours an update (speed 0 to 10: up to 10 game hours a real second), and
  only while its calendar is in the world's ticker list. **Time on / off are records**: the world counts the "on"s,
  and it runs only when both players have it on (one on: the calendar flashes, waiting for the other). In a network
  game both on forces speed 10. The speed slider, the digits and Advance Time act on the local clock only and aren't
  sent. The clock is stopped for a battle and isn't restarted by coming back: a player turns it on again.
- **Why it didn't work** (from the listing): the two machines pair the ticks one apart and carry out the two
  players' records in opposite orders; each loads its own side's start file; the random table is seeded from the
  clock on each machine; the speed isn't shared; delayed messages fall due in raw ticks, which count the waits; the
  buffers have no bounds (a large box select overflows); one Receive a wake can leave a packet waiting for good; a
  lost connection is never noticed; and "Load Network Campaign" never opens a session.

## The plan's principles

1. **One path for every game.** Every player action that changes the game becomes a command record, carried out
   by one executor at a known tick, in single player as in a network game. The single-player game is the network
   game with the opponent played by the computer and no wait. A replay is the same stream read from a file. Nothing
   in the war map or the battle map acts on a mouse click directly any more.
2. **The original's model, its faults fixed.** Lockstep with the original's records and codes; the fixes are marked
   MODERN FIX as elsewhere: every tick carries both players' records for that tick, carried out Human first (the
   host's side first in a civil war); both machines start from the same bytes (the host's start file or save); one
   seed for everything that plays; the speed and pause are records; delayed work counts game ticks, not frames;
   bounded buffers; a tick number and a checksum in every packet; a lost connection noticed and reported.
3. **The code kept together.** `src/net/` holds the transport (direct and relayed), the session and the lockstep;
   `src/game/` (new) the player objects, the records, the executor and the match rules, shared by the war map, the
   battle map and the front end. The maps gain no `if (network)` branches: they take commands and run ticks. The
   relay server is its own small program in `src/relay/`, sharing `src/net/`'s message code.
4. **Plain sockets, no DirectPlay, no new library.** TCP for the game (in order and reliable, which lockstep needs),
   UDP broadcast to find games on a LAN; the few socket calls for Windows, Linux and macOS in one file.

## The match options (the owner, update 278)

Set by the host before the match, shown to the joining player before they accept, and kept in the campaign's save
so a loaded match plays by them. Each also stands in the ini as the host's default.

**Speed control** (the war map's speed slider and Advance Time):
- *Original*: as the retail game: time runs only when both have it on, then at speed 10; the slider does nothing.
- *Host*: the host's slider sets both games' speed; the joiner's slider is shown but locked.
- *Either*: either player's change takes effect on both at once.
- *Agreed*: a change is a proposal: the other player is asked ("Martian Player asks: speed 5. Accept / Decline"),
  and it takes effect on both only when accepted. Time on / off is agreed the same way: one side turning time off
  asks the other, and so does turning it back on (the original's vote, made into a question with an answer).

**Pause** (the war map's and the battle map's menus, and a pause key):
- *Off*: opening a menu doesn't pause a network game; the game goes on underneath (the usual for network games).
- *Either*: either player can pause both; either can resume.
- *Agreed*: pausing is asked and answered as above, and resuming too: both must agree to pause, and both to
  resume. While a request waits, the game goes on; a request not answered in 30 seconds lapses.

The agreement works the same for every option it covers: a *proposal* record from one player; the other's screen
shows the question with Accept and Decline (a key for each); an *answer* record back; on acceptance the change is
carried out by both at the same tick (the answer's tick + the input delay), on a decline the asker is told. One
proposal at a time per kind; a new one replaces the player's own earlier one.

Other match options, from the same screen: the campaign (regular, a civil war and its sides, a custom campaign, or
a save), the host's side, the Modern Gameplay rules (the host's, for both), Skip briefings (both or neither), and
the connection (below).

## Connecting: direct and relayed (the owner, update 278)

**Direct connect** is always offered: the host listens on a port (default 29898, settable), the joiner types the
host's address or picks it from the LAN list. Over the internet the host forwards that port on their router.

**Relayed**: both players connect out to a relay server, so neither forwards a port. The relay:
- is a small separate program (`wowrelay`), headless, for a Linux server, Windows or a Mac; anyone can run one, and
  the project can run a public one; the engine's relay address is a setting (`[Network] Relay`), with the project's
  as the default once there is one;
- keeps a list of open games (name, host's side, campaign, engine version, whether it's full); a host registers its
  game and gets a short code (e.g. `K7QX`) to give the other player, or the game is listed for anyone;
- forwards the two players' messages unchanged, with size and rate limits; it runs no game logic and never sees
  inside a tick's records;
- notices a player gone (closed connection or no message in 15 seconds) and tells the other.

The two are one code path in the engine: a `net::Link` that's either a direct TCP connection or a relayed one;
the session and the lockstep above it don't know which.

**Mixed systems.** Lockstep needs both machines to compute exactly the same results. The same build of the engine
on the same system will; Windows, Linux and macOS builds are made by different compilers, whose floating-point sums
can differ in the last place, and in lockstep a difference grows until the games disagree. Phase 0 checks this and
fixes what it can (the same operation order, no fused multiply-add). What can't be fixed is **documented as a known
limit**: play between different systems may go out of step. A relay doesn't change that (it only passes messages
on), but the out-of-step handling below lets a game carry on from the host's state when it happens:
- on the war map, the host's state is sent and the joiner's game rebuilt from it (the war map's save form), and
  play goes on;
- in a battle, the battle is ended and settled from the host's result (its losses, as the war map takes them).
Each machine's system and build are exchanged in the hello, and the menu warns when they differ.

## The task list

Each item is an update or a few. Every update leaves the single-player game as it was to the player.

### Phase 0: determinism (nothing seen changes)
- N0.1 A game tick counter (the simulation's), apart from the frame counter (dword_4D2558): the periodic work of
  both maps and the delayed messages counted in game ticks, which don't move while waiting or paused.
- N0.2 Every `random::Rand()` and `Random(n)` call read against the listing and sorted: the game's stream (what
  plays) or a new presentation stream (what's only seen or heard: the music step 0x41B070, water twinkles
  0x43A670, emitters 0x47F050, the counter blink 0x42B2F0, the drawing's jitter, the UI). 60 calls in 12 files.
- N0.3 The random table (0x402630) filled from the game's seed, not the start-up clock; its index reset with it.
- N0.4 The other inputs to the game found and fenced off: the screen size, the zoom, the HUD scale, sound on or
  off, smoke and lights on or off, the fog's drawing, anything read from the ini during play. Settings that change
  the game itself (the Modern Gameplay rules, Game Frequency) become match settings (Phase 4).
- N0.5 A state checksum: the war map (counters, sectors, owners, research, factories' queues, the clock, the rand
  state) and the battle map (units' places, headings, health, orders, targets, the groups, the rand state and
  table index), a 64-bit hash cheap enough for every tick.
- N0.6 A debug dump beside the checksum: the state as text, for comparing two games tick by tick.
- N0.7 The determinism test: a campaign and a battle run twice from the same input, headless, the checksums
  compared every tick; then with different presentation settings to show they don't matter.
- N0.8 Floats across builds: the same run on Windows (MSVC) and Linux (GCC, Clang) compared; the compilers told to
  keep to the written order (`-ffp-contract=off`, MSVC `/fp:precise`); sums that still differ found and made exact
  where it can be done. What remains is written up (above, "Mixed systems").

### Phase 1: the players and their commands (A2; single player through the new path)
- N1.1 `game::Command`: the original's record (header, code, length, up to 28 bytes) and its codes
  (ORIGINAL-NETWORK.md), serialised little-endian; bounded buffers (the original's overflow fixed).
- N1.2 The player objects: local, computer and remote; each tick's records; the executor (0x438C90) and the tick's
  order (0x403680) in `game::Tick`. Per-player selection (the original's +0x430 / +0x434) kept in the player.
- N1.3 New codes for what the original didn't send, each documented with its fields: speed, Advance Time, pause /
  resume, proposals and answers, research pick, production (Build List add, remove, promote, repeat, suspend /
  resume), repairs, retreat, cancel move, information pages (local only, not sent), Return to Warmap, Auto Resolve,
  chat.
- N1.4 The war map moved onto commands, one action at a time with a screenshot check each: selection; send and
  move (8, 0x18); attack; retreat; cancel move; time on / off; speed and Advance Time; the Build List; research;
  repairs; the popup's items. What's only looking (view modes, the minimap, scrolling, information pages, the
  research screen's browsing) stays local.
- N1.5 The battle map moved onto commands: selection by object id (box select as a list); move and waypoints;
  attack and force fire; stances; the group keys; build and place (and fence lines); the specials; retreat through a
  reinforcement point; Return to Warmap; Auto Resolve. Camera, zoom, group icons and info pages stay local.
- N1.6 The computer player as a player object: the war map AI (WarMapAi) and the battle AI (BattleAi) give their
  orders as commands, as the original's 0x442890 does.
- N1.7 The front end's game-changing actions (start a battle, leave it, the debriefing's return, the briefing's
  begin) on the same path where they change the game.
- N1.8 Playback recording the command stream (the original's raw-input demo files still read); the determinism
  test runs from recordings.
- N1.9 The whole single-player game checked: the regression screenshots, a campaign played through a few battles,
  the tests.

### Phase 2: lockstep in one process
- N2.1 The scheduler: tick k carries both players' records for k; a player's records are sent for tick k + d (d,
  the input delay, from the measured round trip, 2 to 8 ticks); a tick runs only when both are in, the host's side
  first. While waiting the screen goes on drawing and nothing moves (the original's wait).
- N2.2 The match options' rules (speed, pause, proposals and answers) in `game::MatchRules`, used by the executor.
- N2.3 A loopback session: two games in one process, each with its own world, joined by an in-memory link with
  delay and jitter; commands from a script; checksums compared each tick.
- N2.4 Out of step: a checksum in each packet; on a mismatch, a report (the tick, both checksums, both dumps, the
  last records) in the log, then the recovery above (the host's war map state, or the battle settled from the
  host's result).
- N2.5 Tests: the scheduler, the proposals, a desync forced and recovered, all in `tests/`.

### Phase 3: transport, session and relay
- N3.1 `net::Socket`: TCP listen / connect / send / receive, non-blocking, IPv4 and IPv6, Windows and POSIX; UDP
  broadcast for the LAN list.
- N3.2 `net::Link`: direct (a TCP connection) or relayed (a connection to the relay with a game code); one
  interface for the session.
- N3.3 The protocol: length-prefixed messages with a type; hello (engine version, protocol version, system and
  build, a check of the game data), the host's setup (match options, side, campaign, the start bytes, seed, Game
  Frequency, Modern Gameplay rules), ready, tick packets (tick, records, checksum), proposals and answers as
  records, chat, leave; every size checked; a version mismatch refused with a message.
- N3.4 Keeping alive and connection loss: a message at least every second; "Waiting for <player>..." after 3
  seconds of silence, with Wait and Leave; after 60 seconds the game is saved and the menu shown with a message.
- N3.5 The relay server `wowrelay` (`src/relay/`, its own CMake target, no SDL): registering a game, the open games
  list, joining by code or from the list, forwarding, limits, logging; a `--port` and a config file.
- N3.6 The engine's relay client: the address setting, the list and codes, falling back to direct connect.
- N3.7 Release: `wowrelay` built for Windows, Linux and macOS with the engine; how to run one (README-RELAY.txt).

### Phase 4: menus and setting up a match
- N4.1 **Multiplayer** on the main menu (no "Enable Network Version" needed; MODERN.md "Game modes"): Host Game,
  Join Game, Load Multiplayer Campaign.
- N4.2 Host Game: the match options screen (campaign, side, speed control, pause, Modern Gameplay rules, Skip
  briefings, direct or relayed, the port or the game's name), then Waiting for Player (the original's 0x406B30),
  showing the address and port or the relay code.
- N4.3 Join Game: the LAN list (the original's session list, 0x407110, refreshed every second), the relay's list,
  and a field for an address or a code; then the host's options shown with Accept and Back.
- N4.4 The start: the host's start file or save sent whole and both games built from those bytes; a civil war's
  two sides as the two players; each player's side's screens, sounds and texts on their own machine.
- N4.5 Saving: both games save the match in a slot of their own, in the engine's save form with a multiplayer tag
  and the match options; the war map's saves and the save after a battle as in single player.
- N4.6 Load Multiplayer Campaign: the host picks a multiplayer save, hosts it, and sends it; the joiner plays the
  side the save says.
- N4.7 The new texts in the five languages (menus, options, questions, messages).

### Phase 5: the war map in a network game
- N5.1 The clock by the speed option (Original, Host, Either, Agreed), with the calendar's flash while waiting and
  the question box for proposals.
- N5.2 Pause by the pause option; the war map's menus, the research screen and the information pages local.
- N5.3 Each side's own view: the fog and intel, its research, its messages and critical events, its counters.
- N5.4 Cheats off (byte_4D256C), the developers' switches off; the Modern Gameplay rules the host's for both.
- N5.5 The campaign's rules as in single player from the shared seed: the pods, the endings (the winner's and
  loser's movies on each machine), a civil war's research rules.

### Phase 6: battles in a network game
- N6.1 Entering together: the briefing on each screen (or both skipped); the battle map starting on the same tick
  once both have left the briefing.
- N6.2 Each player commands their own side; the battle AI plays neither side; the reinforcements and retreats as
  commands.
- N6.3 Pause in battle by the pause option; the Battle Options menu local otherwise.
- N6.4 Return to Warmap: one player leaving asks the other (or both leave, as the original's battle end);
  Auto Resolve by agreement in a network game.
- N6.5 Leaving together: the debriefing on each screen, back to the war map on the same tick, the save.

### Phase 7: finishing
- N7.1 Chat: a line typed after Enter (T in battle), shown with the messages on both maps.
- N7.2 The headless two-player test: two engine processes and a relay on one machine, host and join by script, a
  campaign started, the clock run, a battle fought, checksums compared to the end.
- N7.3 Docs: NETWORK.md as built; README-MULTIPLAYER.txt for players (direct connect and ports, the relay and codes,
  the match options, mixed systems); TESTING.md.
- N7.4 Later, if wanted: more than two players (a civil war's sides shared, or a third side), joining from a save
  mid-match, spectators.

## Order and size

Phases 0 and 1 are the bulk (every action in both maps moved onto commands) and change nothing that's seen; they
make the single-player game, replays and the network game one thing. Phase 2 is tested inside one process before
any socket exists. Phases 3 and 4 are the visible network game, the relay among them; 5 and 6 make a campaign and a
battle playable; 7 finishes it.
