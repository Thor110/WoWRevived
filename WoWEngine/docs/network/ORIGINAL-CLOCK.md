# Time and the clock in WoW (1998): research for the network game

Source: /home/user/ida/IDA-Output.txt. Everything below was read from the listing unless marked *provisional*
(inferred, not traced to the end) or *speculation*.

## 1. The timer and one update

### Settings (static initialisers 0x401770-0x4018C0, bool class 0x4115E0 stores the value at +0x11)
| Setting | Object | Value byte | Default |
|---|---|---|---|
| "Game Frequency" | 0x4D1160 | dword_4B84C8 | 0x1E = 30 |
| "Thread Enable" | 0x4D1200 | byte_4D1211 | 1 |
| "Timer Enable" | 0x4D1280 | byte_4D1291 | 0 |
| "Sleeper Enable" | 0x4D12B8 | byte_4D12C9 | 0 |
| [System] "Game max time (ms)" | 0x4D11C0 | (no reader found) | 25 |

### Default path (Thread Enable 1, Timer Enable 0, Sleeper 0)
- WinMain makes two **auto-reset** events (CreateEventA with bManualReset 0, 0x401C0D/0x401C13): `hObject`
  (timer -> game thread) and `hEvent` (game -> render thread), then starts GameThread 0x402180 and RenderThread
  0x402230 (0x401C3B / 0x401C5A).
- 0x401C9C-0x401CBD: `timeSetEvent(1000 / GameFrequency (integer idiv, 33 ms at 30), resolution 0, 0x4020D0,
  dwUser = ++dword_4D1300, TIME_PERIODIC)`; id in dword_4B84CC. Same code again in 0x401FF0 (restart), killed by
  0x402050 (timeKillEvent) — 0x44DE20 / 0x44E060 stop and restart it (*provisional*: around mode changes).
- TimeProc 0x4020D0: rejects a stale timer id ("TimeProc called by wrong timer!"); calls 0x403890 (input/sound
  poll: 0x40CDC0 and 0x462FF0 on dword_5891D8, *provisional*); then, with Timer Enable off, just `SetEvent(hObject)`.
- GameThread 0x402180: `WaitForSingleObject(hObject, 4000)` ("No Timer event yet!" on time-out, then runs anyway);
  EnterCriticalSection; timeGetTime() (result discarded); `app->vtbl+0x10` = **0x403680, the game frame**;
  SetEvent(hEvent); leave.
- RenderThread 0x402230: waits on hEvent (2000 ms), draws 0x403920 inside the same critical section (UI queue
  unk_4DAF28 pumped there), then 0x40AF20 (flip, *provisional*).
- Timer Enable 1: the frame runs inside the TimeProc itself, skipped while byte_4D12F0 (busy) is set.
- Sleeper Enable 1: a thread 0x402080 loops `0x403890; SetEvent(hObject); Sleep(1000/freq)` instead of the MM timer.
- Thread Enable 0: WinMain runs a PeekMessage loop that calls the frame and the draw back to back with no timer
  (0x401E2B-0x401F31) — as fast as it can.

**Catch-up: none.** One timer period = at most one frame. Ticks that arrive while the frame is still running set an
already-set auto-reset event and are lost (Timer Enable mode: dropped by the 0x4D12F0 busy flag). A slow machine
simply runs slower in game time. (The engine's main.cpp loop catches up to 5 ticks after a stall: a deliberate
difference, harmless for single player, but in lockstep the frame rate is set by the network anyway.)

### The frame, 0x403680 (WGame::Tick)
1. timeGetTime() — result unused.
2. If byte_4EA3E0 (active; cleared by WGame::Pause 0x42CD40) is 0: return (nothing else, counter does not move).
3. Mouse (GetCursorPos, 0x44C700) unless byte_4D253C (quitting).
4. Re-entry guard byte_4D2534.
5. 0x42D2D0, then **`++dword_4D2558`** — the global frame counter (used by the message queue, the calendar's
   flash bit, the battle's fog/smoke/AI, etc.). It advances before and regardless of the network wait below.
6. If both players exist (dword_4E9B74 local, dword_4E9B78 opponent):
   - `opp->vtbl+0x24()` (network: receive, see 3);
   - **if `opp+0x438 == 0` (no packet) and `dword_4F73D0 != 0` (ours already sent): skip to 9 — the whole game
     update is skipped this frame.** This is the lockstep wait.
   - else `opp->vtbl+0x28(local)` (network: send our commands), `local->vtbl+0x2C()` and `opp->vtbl+0x2C()`
     (0x438BF0: carry out each buffered command via 0x438C90, then empty the buffer).
7. Pump the game message queue unk_4D24C0 (0x434FA0 returns messages whose due frame `[msg+8] <= dword_4D2558`;
   delayed posts are 0x4355A0 -> 0x435050, due = dword_4D2558 + delay); each to its target's vtbl+0x1C.
8. `0x435430(app)`: the current screen's tick (app+0x20 -> vtbl+0x10) unless app+0x44 — the war map / battle
   update.
9. Every 60 frames 0x415410 (memory stats print).

Without an opponent object (menus) step 6 is skipped. Pause bookkeeping: 0x4038D0/0x403900 store
`dword_4D2558 - n` in dword_4D2570/4D14A4; 0x4038C0/0x4038F0 read frames since.

### Battle map vs war map
One timer, one frame, one counter for everything. The frame only calls whichever screen is current (step 8);
the lockstep (step 6) is in the frame, so it covers the war map and the battle map alike.

## 2. The war map clock — the calendar document (0x41F550, "WDocument", dword_4E9B80)

Fields: +0x20 its view (calendar 0x426C10), **+0x24 speed index**, **+0x28 game time in hours (float)**,
**+0x2C hours per update (float)**, **+0x30 forced hours pending**, +0x34 (sound object, *provisional*),
**+0x38 "waiting" flash**. All zero on creation; the date object unk_4EA0B0 gets day 0, month 8, year 1898
(word_4EA0BA/BC/BE).
Vtable 0x4A9B18: +8 load 0x41F720, +0xC save 0x41F760, +0x10 **tick 0x41F6C0**, +0x14 show 0x41F7A0 (makes the
view), +0x18 hide 0x41F820.

- **Set speed 0x41F640(i)**: +0x24 = i (with the clock sound 0x41F860 on a change); tells the view (0x8073 with
  i << 16); **+0x2C = Speeds[i] / (float)GameFrequency** (flt_4BA470: 0.0, 1.0 … 10.0); if that is 0.0,
  0x41A6C0(0) (time off). So per update: speed i adds i/30 game hours at 30 Hz; speed 10 is an hour every 3
  updates; one real second = i game hours. Note the divisor is the *ini value*, not 30.
- **Tick 0x41F6C0**: h0 = (int)+0x28; if +0x30 > 0 then `+0x28 -= -1.0` (one whole hour) and +0x30-- ; else
  `+0x28 += +0x2C`. d = (int)+0x28 - h0; if d != 0: date 0x41B770(d hours, 0 minutes) and the world (dword_4E9B70)
  gets **0x802D (d)** synchronously (vtbl+0x1C).
- **When the tick runs** (*provisional* on the list walk itself): only while the clock is in the world's ticker
  list (world+0x20, linked through +0x14). Added by 0x41F6B0 (0x440060 on world+0x20) when the time-on count
  reaches 2; removed by 0x4393D0 on every time-off (0x419D8A) and by hide (0x41F820 -> 0x4393D0, from the world's
  hide 0x419030). Show (0x41F7A0) does **not** put it back. So "time running" = in the ticker list. The speed
  alone does not run the clock (the engine's ClockTick runs every update and relies on the speed being 0 when time
  is off — equivalent except while the slider is dragged before it is released).
- **Time running, 0x41A6C0(on)** (world+0x64): no-op if unchanged; else records action 0xA (on) / 0xB (off) in the
  *local* player's command stream ({01,01,code,04} via 0x4460A0/0x4460C0) and calls `opponent->vtbl+0x30(on)`.
  Callers: 0x419A51 (research 0x7D), 0x41A4CB/0x41A532 (digit keys / Backspace 0x41A450), 0x41C841/0x41C892
  (map presses), 0x41E53C (InfoBox), 0x41F6A2 (speed 0), 0x4278FC (slider released, 0x4E+0x202, if speed != 0),
  0x4279AB (Advance time), 0x42C170.
- **Carrying out** (0x438C90): 0xA -> world 0x8050, 0xB -> 0x8051, 0xC -> 0x8054, 0xD -> 0x8055. World 0x4198C0:
  - 0x8050/0x8054 (0x419D2A): ++world+0x68; if == 1: calendar +0x38 = 1 (flash, waiting); else +0x38 = 0,
    **if byte_4D256C (network game) set speed 10 (0x41F640(10))**, and start the clock (0x41F6B0).
  - 0x8051/0x8055 (0x419D70): --world+0x68; speed 0 (0x41F640(0)); clock out of the ticker (0x4393D0);
    +0x38 = (+0x68 == 1).
- **Advance time** (calendar view 0x4278A0, 0x201 at 0x427987): refused unless every sector allows it (0x41AEA0 ->
  0x43D010); 0x41A6C0(0); clock +0x30++; then the clock's tick is called directly ([+0x10] at 0x4279C5), so the
  hour passes at once, locally.
- **Slider** (0x114/0x115 at 0x427932): 0x41F640(position) if 0x41AEA0 allows, else 0x41F640(0) — **directly on
  the local clock; no command is recorded.** Digit keys (0x41A450 -> 0x41A4BE) likewise, then 0x41A6C0(1).
- **Hourly work 0x802D** (0x419E9D): forwarded with the hours to world+0x90 (routes), the local and the opponent
  player, 0x458BD0 on dword_4E9B88 / 4E9B8C, the world's own 0x41A330, and each of the 30 sectors (+0x98…).
- **The date** (unk_4EA0B0, 0x41B770): words +4 total hours (16-bit), +6 minutes, +8 hour of day, +0xA day,
  +0xC month (0-11), +0xE year; minutes carry at 60, hours at 24, days by the month table word_4B9FE0 with
  February 29 for year%4==0 && (year%100 != 0 || year%400 == 0).
- **Saved** (0x41F760): tag 'CALN' (0x4E4C4143), the float +0x28, then the date words (0x41B930). Loaded by
  0x41F720 / 0x41B8D0. The speed (+0x24), +0x30, +0x38 and the world's +0x64/+0x68 are not in the CALN record
  (*provisional*: not checked in the world's own save 0x41AD80).

## 3. The network game

- **Players** (0x4185F0): local = 0x438B20 (dword_4E9B74); opponent dword_4E9B78 = network player 0x445EA0
  (vtable 0x4AA740) in a network game, else the AI 0x442890 (vtable 0x4AA680). The side comes from
  [Network] "I Am Human" (byte_4B84C4). byte_4D256C is set to 1 by app command 0xA2 (0x402E3D) and 0 at 0x402D01
  (*provisional*: "network game started" / "game over").
- Player object: command buffer +0x30 (up to 0x400), length +0x2C, write pointer +0x28, +0x438 "have commands"
  (starts 1). dword_4F73D0 = "our packet for this round already sent" (0x445EA0 clears it, 0x445F80 sets it).
- **Network player**: +0x24 = 0x445F20: 0x44A130 takes the received packet if byte_586C4C is set, copies it to
  +0x30, +0x438 = 1; else +0x438 = 0. +0x28 = 0x445F80: if the local stream is empty adds a no-op (action 1,
  skipped at 0x438C9D) so **every round sends a packet**; 0x44A0E0 copies it after a 16-bit packet number
  (dword_586C58++) into dword_586820 and wakes the send thread; dword_4F73D0 = 1. +0x2C = 0x438BF0 (carry out).
  +0x30 = 0x445FD0: on time-off records 0xD into the opponent's own buffer (looks like a stand-in for the remote's
  answer; *provisional*: it is overwritten by the next received packet before it is carried out, so it seems to do
  nothing).
- **AI**: +0x24 returns 1 (always ready), +0x28 nothing, +0x30 = 0x442A80 records 0xC/0xD for every 0xA/0xB, so the
  count goes 0 <-> 2 at once.
- **DeePlay** (DirectPlay 4): send thread 0x44A160 waits on dword_586C38 (10 s time-out, "SendThread - Timed out")
  and calls IDirectPlay4::Send (vtbl+0x68) from dword_586C28 to DPID 0 (all), flags 1 (guaranteed) if byte_586C5C
  else 0. Receive thread 0x44A290 waits on dword_586C34 (10 s), Receive (vtbl+0x64) into dword_586380 (0x400);
  system messages: type 3 (create player) stores the player id in dword_586C2C (and posts 0x111/0xAB to dword_4D2538
  if byte_586C40), type 5 (destroy) clears it when it matches; a game message sets byte_586C4C. One slot, packet
  number logged but not checked. Logs when byte_4D1231 ("DeePlay::SEND/RECEIVED packet # %d of %d bytes on cycle %d").
- **The lockstep** (frame step 6): a machine advances one game update only when it has the opponent's packet for
  the previous round; while waiting the frame skips the game update and the queue pump, but the frame counter
  dword_4D2558 still goes up and the screen still draws. There is no time-out or "waiting for player" screen in the
  frame; the only visible sign is the calendar flash when exactly one side has time on.
- **Who controls time**: both. The clock runs only while world+0x68 == 2, i.e. both players have turned time on;
  either player turning it off (any of the many 0x41A6C0(0) callers: a map click, a box, Backspace…) stops it on
  both machines, and the other then sees the calendar flash (+0x68 == 1) until both are on again.
- **Speed in a network game**: when the count reaches 2 each machine sets speed 10 itself (0x419D5B). The slider,
  the digits and Advance time act on the local clock only, sending nothing. *Provisional/likely a desync in the
  original*: a slider move or Advance-time click in a network game changes only one machine's hours. Nothing found
  disables them in a network game (0x41A450 and 0x4278A0 have no byte_4D256C test).
- **How hours stay equal**: only by both machines running the same number of ticks of the same step: equal
  Game Frequency in both ini files (the step is 10 / dword_4B84C8), start/stop through the command streams, and
  one tick per lockstep round. Hours are never sent or compared.
- **One-round skew (*provisional*, from the order in 0x403680)**: in round k a machine carries out its own packet k
  and the opponent's packet k-1 (the first round runs with an empty opponent buffer). So a command takes effect one
  round earlier on the machine that issued it. For the clock: the count reaching 2 / dropping happens one round
  apart on the two machines unless the same side starts and stops, so the two clocks can differ by one step
  (10/30 h) and an hour boundary (0x802D, routes) can fall a round apart. Unit orders would have the same skew.
  The rebuild should carry out both sides' packet k in round k (a fixed input delay) instead of copying this.
- 0x5432D0: no named references in the listing; nothing found.

## 4. The battle map's time
- Going to battle (0x8019 -> 0x41A13C) hides the world (vtbl+0x18 = 0x419030), which hides the calendar (0x41F820
  -> 0x4393D0): **the war map clock is out of the ticker for the whole battle** (confirmed). The critical-event box
  before it has already turned time off (0x41E53C). Coming back, show (0x418980 -> 0x41F7A0) does not restart it:
  time stays stopped until a player turns it on again (in a network game, both).
- The battle runs on the same frame: one battle update per game update (0x43A980 and the objects' updates), with
  periodic work keyed to the global counter dword_4D2558 (fog when it is a multiple of the update frequency, smoke
  every 15th, see BATTLEMAP.md 526 / 1611 / 1916) and durations in multiples of dword_4B84C8 (0x4864xx, 0x48E1xx,
  0x494Exx).

## 5. Real time and other non-determinism
- timeGetTime: 0x401AE1 seeds srand at start-up; 0x402127, 0x4021CE, 0x402282, 0x401EC4, 0x401F13, 0x40369B,
  0x403941, 0x442F19 — all with the result discarded (eax overwritten). GetTickCount, QueryPerformanceCounter are not
  imported; GetLocalTime is imported (CRT). The game state does not read the wall clock.
- **srand(1)** when the war map world is made (0x417DA3): both machines start the campaign with the same rand
  sequence. Any rand() made by something local (e.g. the music step 0x41B17C, which depends on the CD drive's
  state; 0x402785) would split the sequences (*provisional* risk).
- Real-time-dependent but update-counted: the frame counter dword_4D2558 keeps counting while a machine waits for
  the network (and its pause base is adjusted on Pause/Resume), and delayed messages (0x435050) and the battle's
  "every n-th tick" work are keyed to it. Two machines that waited different numbers of frames see different
  values, so these would not stay in step (*provisional* risk). For the rebuild: key simulation work to a lockstep
  round counter, not the frame counter.
- Game Frequency is per machine (ini) and enters the clock step and many durations: the two must agree (send it
  in the session set-up).
