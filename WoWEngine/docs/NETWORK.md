# The network game: what the original did, and the plan

PLAN.md A2 (the players and their commands) and A9 (the network game), planned at update 277. What the original
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
   MODERN FIX as elsewhere: every tick carries both players' records for that tick, carried out Human first; both
   machines start from the same bytes (the host's start file or save); one seed for everything that plays; the
   speed is a record; delayed work counts game ticks, not frames; bounded buffers; a tick number and a checksum in
   every packet; a lost connection noticed and reported.
3. **The code kept together.** `src/net/` holds the transport, the session and the lockstep; `src/game/` (new) the
   player objects, the records and the executor, shared by the war map, the battle map and the front end. The maps
   gain no `if (network)` branches: they take commands and run ticks.
4. **Plain sockets, no DirectPlay.** TCP for the game (in order, reliable: what lockstep needs), a UDP broadcast to
   find games on the LAN, and an address to type for the internet (the host forwards one port). No new library:
   the few socket calls for Windows, Linux and macOS in one file.

## The task list

### Phase 0: determinism (no visible change; single player as before)
- N0.1 A **game tick** counter for the simulation, separate from the frame counter (dword_4D2558): the war map's and
  battle map's periodic work and delayed messages count game ticks, which don't move while waiting.
- N0.2 **Two random streams**: the game's (rand() and the table, as now) for what plays, and a presentation stream
  for what's only seen or heard (the music step, water twinkles, emitters, the counter blink, the drawing's
  jitter). Each use read against the listing and put in one or the other.
- N0.3 The random table filled from the game's seed, not the start-up clock (both machines alike).
- N0.4 A **state checksum**: the war map (counters, sectors, research, clock, rand state) and the battle map
  (units' places, health, orders, rand state, the table's index), cheap enough for every tick.
- N0.5 A **determinism test**: one campaign run twice from the same records, headless, compared tick by tick; then
  with different options (sound off, smoke off, another screen size, the zoom) to show they don't change the game.
- N0.6 Floats checked across builds: the same records on Windows (MSVC) and Linux (GCC / Clang) must give the same
  checksums; where they don't, the offending sums made exact (fixed order, no fused multiply-add: `-ffp-contract=off`
  and MSVC `/fp:precise`).

### Phase 1: the players and their commands (A2; single player through the new path)
- N1.1 `game::Command`: the original's record (header, code, up to 28 bytes), with the codes in
  ORIGINAL-NETWORK.md; new codes for what the original didn't send (speed, research pick, production, the Build
  List's queue changes, repairs, pause, Auto Resolve, retreat), each documented.
- N1.2 The player objects: local, computer and remote, each with its tick's records (bounded); the executor
  (0x438C90) and the tick's order (0x403680) in one place.
- N1.3 The war map's actions moved onto commands: selection, send / move, attack, retreat, cancel, time on / off,
  speed, Advance Time, build orders, the Build List (add, remove, promote, suspend / resume), research, repairs.
- N1.4 The battle map's actions moved onto commands: selection (by object id), move, attack, force fire, the group
  keys and stances, build and place, the specials, retreat, Return to Warmap and Auto Resolve.
- N1.5 The computer player's choices (war map and battle AI) given as its commands, as the original's AI player
  does, so the AI's moves are in the stream too.
- N1.6 Playback (core/Playback.h) recording the command stream instead of raw input (raw input stays for the
  original's own demo files); the determinism test (N0.5) runs on it.
- N1.7 The whole single-player game played through: nothing changed to see, the regression tests and screenshots as
  before.

### Phase 2: lockstep (in one process first)
- N2.1 The lockstep scheduler: each tick k carries both players' records for k; a player's records are sent for
  tick k + d (d, the input delay, a few ticks, set from the round trip); a tick runs only when both are in, Human
  first. The wait as the original's (the screen goes on drawing; the clock doesn't move).
- N2.2 A **loopback session**: two games in one process, each with its own world, joined by an in-memory link, for
  the tests: commands from a script, checksums compared every tick.
- N2.3 Desync handling: a checksum in each packet; on a mismatch both stop with a report (the tick, the checksums,
  the last records) in the log, and the host's save offered to start again from.

### Phase 3: transport and session
- N3.1 `net::Socket`: TCP listen / connect / send / receive, non-blocking, on Windows and POSIX; a UDP broadcast
  for LAN discovery.
- N3.2 The protocol: hello (engine version, protocol version, the game's data check), the host's setup (side,
  campaign, start bytes, seed, Game Frequency, the Modern Gameplay rules), ready, tick packets (tick, records,
  checksum), chat, leave. Length-prefixed messages; all sizes checked.
- N3.3 Connection loss: a time-out with "Waiting for <player>..." on screen, then the game saved and back to the
  menu with a message.

### Phase 4: menus and setting up
- N4.1 **Multiplayer** on the main menu (no "Enable Network Version" needed; MODERN.md "Game modes"): Host, Join,
  Load Multiplayer Campaign.
- N4.2 Host: choose the side (Human or Martian, either install), the campaign (the regular one, a civil war, a custom
  campaign), then wait for the other player (the original's Waiting for Player, 0x406B30).
- N4.3 Join: the games found on the LAN (the original's session list, 0x407110, refreshed every second), or an
  address and port typed in.
- N4.4 The start: the host's start file or save sent whole, both games built from those bytes (the original loaded
  each side's own file); a civil war's two sides as two players on one faction.
- N4.5 Saving and loading: both machines save the campaign in a slot of their own (the engine's save form, tagged as
  a multiplayer campaign); Load Multiplayer Campaign hosts a save and sends it (the original's never opened a
  session).

### Phase 5: the war map in a network game
- N5.1 The clock: time on / off as votes, as the original (the calendar flashes while one side waits); the speed a
  record, either player's change taking effect on both (in place of the original's forced speed 10, which the
  owner can choose instead); Advance Time a record, refused while a sector holds both sides as now.
- N5.2 Each side's own view: fog, intel, its research, its messages and events; the other side's counters as now
  for the computer's.
- N5.3 Cheats off (byte_4D256C), the developers' switches off, the Modern Gameplay rules the host's for both.
- N5.4 The pods and the other campaign rules as in single player, from the shared seed.

### Phase 6: battles in a network game
- N6.1 Both games enter the battle on the same tick: the briefing on each screen, the battle map's start waiting
  for both (or both briefings skipped together).
- N6.2 Each player commands their own side; the battle AI plays no one (both sides human).
- N6.3 Pause: the Battle Options menu doesn't stop a network battle (or pauses both, the owner's choice); Return to
  Warmap and Auto Resolve as commands both games carry out.
- N6.4 Leaving the battle, the debriefing and back to the war map together.

### Phase 7: finishing
- N7.1 Chat (a line typed with Enter, shown with the messages).
- N7.2 The headless two-player test: two engine processes on one machine, host and join by script, a campaign and a
  battle played to a checksum.
- N7.3 Docs: NETWORK.md brought up to date, a players' README-MULTIPLAYER.txt (ports, LAN and internet), TESTING.md.
- N7.4 Later, if wanted: more than two players (the war map is two sides; a civil war's sides could be shared),
  joining a game in progress from a save, a relay server for players who can't forward a port.

## Questions for the owner

1. The speed in a network game: shared, either player able to change it (the plan), or the original's forced
   speed 10 when both have time on?
2. Pause: should the Battle Options and war map menus pause both games, or not pause a network game at all?
3. Internet play: LAN discovery and a typed address with one forwarded port (the plan), or should a relay server be
   planned from the start?
4. Mixed systems: a Windows player with a Linux or Mac player is planned (Phase 0.6); if the floating point can't be
   made to agree, should it fall back to "same system only"?

## Order and size

Phase 0 and 1 are the bulk (every action in both maps moved onto commands) and change nothing that's seen; they
make the single-player game, replays and the network game one thing. Phase 2 is tested inside one process before
any socket exists. Phases 3 and 4 are the visible network game; 5 and 6 make a campaign and a battle playable; 7
finishes it. Each phase is its own run of updates, each update leaving the single-player game working.
