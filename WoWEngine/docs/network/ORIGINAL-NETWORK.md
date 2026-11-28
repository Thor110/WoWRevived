# Network game of the retail WoW.exe (IDA listing): research notes

Source: /home/user/ida/IDA-Output.txt (retail build). Everything below is read from the listing. Items marked
*provisional* are inferences that the listing supports but does not prove. The survey notes (docs/survey/part07.md
2.3 and 2.9) were checked against the code and are mostly right; corrections are flagged.

## 1. Transport: DeePlay (0x449EE0-0x44AD07), DirectPlay 3A

**Imports**: DPLAYX.dll `DirectPlayEnumerateA` (thunk 0x47AEF8, called once at 0x406CBC) and `DirectPlayCreate`
(0x47AEFE, called once at 0x44ABD5). No lobby (no IDirectPlayLobby, no DirectPlayLobbyCreate).

**Interface**: `Create(spGuid*)` 0x44ABB0: stores the GUID *pointer* in dword_586C24, Releases the old object,
`DirectPlayCreate(guid, &dp1, NULL)`, then `QueryInterface(IID 0x4AC980 = {133EFE41-32DC-11D0-9CFB-00A0C90A43CB},
IID_IDirectPlay3A)` into dword_586C20. The IDirectPlay (v1) object is never Released (leak). Vtable slots used
(IDirectPlay3A order): +0x08 Release, +0x10 Close, +0x18 CreatePlayer, +0x24 DestroyPlayer, +0x34 EnumSessions,
+0x60 Open, +0x64 Receive, +0x68 Send.

**Application GUID** (0x4BEFE0..0x4BEFEC): {A67DB5E1-6C8D-11D1-A3D9-0080C82C00B2}.

**Singleton** `DeePlay::Get` 0x449F40: object at 0x586284 (vtable 0x4AA8A4; it holds no state, everything is in
globals). Creates two auto-reset events: dword_586C38 (send event, game side) and dword_586C34 (receive event,
handed to DirectPlay in CreatePlayer). atexit 0x449FA0 / scalar dtor 0x44A020: DestroyPlayer, Close, Release,
CloseHandle both events.

Globals: 586C20 IDirectPlay3A*; 586C24 SP GUID ptr; 586C28 my DPID; 586C2C remote DPID; 586C30 receive thread
handle; 586C3C running; 586C40 is host; 586C44 session count; 586C48 enum thread handle; 586C4C packet ready;
586C50 received size; 586380 receive buffer (0x400); 586820 send buffer (u16 seq + payload); 586C54 send size;
586C58 send sequence; 586C5C guaranteed (from [Network] Messages Guaranteed, copied at 0x402826, default 0);
4D1231 debug logging (logs go to nullsub_4, i.e. nowhere); 586288-586294 copy of a session instance GUID (written,
never read); 5862A0 `[Network] "Session Name"` setting (0x44A5B0, default "War of the Worlds - session name";
string ptr at 5862B4).

**Service providers** (menu 0xA4 "Select Service Provider", 0x406C10): `DirectPlayEnumerateA(cb 0x406E20, menu)`.
The callback (0x406E20) keeps up to 10: `menu+0x98[i] = lpguidSP` (**the pointer, not a copy**, 0x406E3E) and the
name into the std::string at menu+0xC0+16i; count at +0x160. Picking i (0x406F00): `Create(menu+0x98[i])`, then the
menu's next command (+0x164). Next command = 0x9F (Network Host) when `I Am Human` (byte_4B84C4), else 0xA6
(Game session) (0x403D70).

**Enumerate sessions** `0x44A400`: if no enum thread (586C48 == 0) starts thread 0x44A430 (stack 0x100), which
zeroes the count and calls `EnumSessions(desc{dwSize 0x50, guidApplication}, timeout 0, cb 0x44A4B0, ctx, flags 0)`,
then sets 586C48 = 0. Callback 0x44A4B0: returns FALSE on DPESC_TIMEDOUT; skips sessions with dwCurrentPlayers >=
dwMaxPlayers (desc +0x2C / +0x28); stores the instance GUID at 0x586780+16n and the name at the std::string array
0x5862E0 (10 x 16, built 0x449EE0, freed 0x449F10). **No check that n < 10** (overflow past 10 sessions).

**Host** `0x44A5F0` (called only from WGame 0xA1, 0x402E7A; return value ignored): host flag = 1; closes any old
session; `Open(desc, DPOPEN_CREATE=2)` with dwSize 0x50, dwFlags 0x80 (DPSESSION_NODATAMESSAGES), guidApplication,
dwMaxPlayers 2, session name from the setting. `CreatePlayer(&586C28, DPNAME{0x10, 0, "Human player", "Human
Player"}, hEvent 586C34, NULL, 0, 0)`. On success: running = 1, seq = 0, starts receive thread 0x44A290 (stack 0x100)
and send thread 0x44A160 (stack 0x800). Returns 1 for most unknown HRESULTs; 0 for a listed set of DPERR codes.

**Join** `0x44A8D0(sessionGuid*)` (called only from the session menu 0x4073D0): host flag = 0; `Open(desc{dwSize
0x50, guidInstance}, DPOPEN_JOIN=1)`; `CreatePlayer` "Martian player" / "Martian Player" with the same event; starts
the same two threads.

**Close** `0x44AB40`: running = 0, DestroyPlayer(my id), my id = 0, Close. Callers: 0x402880 (WGame dtor), 0x402D46
(command 0x79), 0x403AB0 (0x90 main menu), 0x403C10 (0x9F), 0x403E40 (0xA6). The threads are not joined or waited
for; they notice running == 0 only after their current wait (up to 10 s).

**Send thread** 0x44A160: loop while running: `WaitForSingleObject(586C38, 10000)`; on timeout, log only. Else
`Send(idFrom = my id, idTo = 0 (DPID_ALLPLAYERS), flags = guaranteed ? 1 : 0, 586820, 586C54)`. On S_OK it calls
`LeaveCriticalSection(stru_5862C4)` (the CS object 0x5862C0 is built by 0x44A0B0) — **nothing ever enters it**.

**Send** `0x44A0E0(data, len)` (main thread): `++586C58`; writes it as the first u16 of 586820; copies the payload to
586820+2; size = len + 2; SetEvent(586C38). Returns 1. No waiting for the previous send.

**Receive thread** 0x44A290: loop while running: `WaitForSingleObject(586C34, 10000)`; on timeout log only (no
Receive call). Else one `Receive(&from, &to, DPRECEIVE_ALL=1, 586380, &size(=0x400))` per wake. If from == 0
(system message): type 3 DPSYS_CREATEPLAYERORGROUP with dwPlayerType 1: remote id = dpId; **if host, Send(WGame,
0x111, 0xAB, 0)** (0x44A37F) — the opponent joined, start. Type 5 DESTROYPLAYERORGROUP of the remote id: remote id =
0. Every other system message (session lost, host migration...) is ignored. If from != 0: packet ready = 1 (0x44A3BF).

**Take packet** `0x44A130(&p, &len)` (main thread): if ready: ready = 0, p = 586380+2, len = size-2 (the sequence is
skipped, never checked).

**How it reaches the game**: not through the message queue. The network player polls 0x44A130 once per game tick
(below). Only the "player joined" event goes through WGame (0xAB).

## 2. The player objects

All three derive from WPlayer **0x438B20** (`ctor(name, notLocal, side)`, base 0x4354C0 = WCmdTarget):
+0x1C = (notLocal == 0) ("is the local UI player": the command executor refreshes the UI, 0x402950, only for it);
+0x20 = 2 (1 for the network player); +0x24 side (0 Human, 1 Martian); +0x28 write pointer; +0x2C used bytes;
+0x30..+0x42F command buffer (0x400); +0x430 selection being built; +0x434 current selection group; +0x438 "has
commands this cycle" = 1. It also sets **dword_4F73D0 = (I Am Human ? 0 : 1)** (0x438B8B..0x438BA0).

Vtables (slot offsets): +0x24 think/receive, +0x28 send(localPlayer), +0x2C execute, +0x30 time-switch hook.

| Class | Ctor / size | Vtable | +0x24 | +0x28 | +0x2C | +0x30 |
|---|---|---|---|---|---|---|
| Local player | 0x438B20 / 0x43C | 0x4AA528 | stub (never called) | nullsub | **0x438BF0 execute** | nullsub |
| Computer (AI) | 0x442890 / 0x548 | 0x4AA68C.. (slot 0x4AA6B0 = 0x442A80) | AI think | (AI) | 0x442A70 -> 0x438BF0 | 0x442A80: records 0xC (on) / 0xD (off) into its own buffer |
| Network player | 0x445EA0 / 0x43C | 0x4AA740 | **0x445F20 Receive** | **0x445F80 Send** | 0x438BF0 | 0x445FD0: records 0xD into its own buffer when off |

- **0x445EA0** network ctor: base ctor, vtable 0x4AA740, dword_4F73D4 = 0, +0x20 = 1. Dtor 0x445F00.
- **0x445F20 Receive**: `0x44A130`; if a packet: copies it to +0x30, +0x2C = len, +0x438 = 1; else +0x438 = 0.
  It does not reset +0x28.
- **0x445F80 Send(local)**: if the local buffer is empty, appends `{01 01 01 04}` (code 1, a no-op); then
  `DeePlay::Send(local+0x30, local+0x2C)`; **dword_4F73D0 = 1** (never set back to 0 anywhere).
- **Creating them** 0x4185F0(isNetwork): Human install: local `0x438B20("Human",0,0)`, opponent
  `0x445EA0("Martian",1,1)` (network) or `0x442890("Martian",1,1)` (AI). Martian install: local
  `("Martian",0,1)`, opponent `("Human",1,0)`. Globals: **dword_4E9B74 local, dword_4E9B78 opponent**.
- dword_4F73D4 ("cycle", read by 0x438B00 for the DeePlay log lines) is only ever written 0, so the log's cycle is
  always 0.
- **0x5432D0** does not exist in this build. It is the network build's local DPID (WoWPatches notes, which also give
  that build's object as IDirectPlay4 at 0x5432C8); the retail equivalent is **dword_586C28**.
- dword_54604C (docs: "network player count > 1") is **never written** in this build (bss, 15 reads, no writes), so
  every branch gated on it (0x419306 shading base, 0x42165F war-map reveal, 0x4219E7 routes, 0x41A9F1, RFHS lists,
  AI 0x443555...) is dead code here. Treat the docs' "in a network game" readings of those as "never".

## 3. The command records and lockstep

**Record format** (built in a stack temporary, then appended with 0x4460C0 -> `0x438C50(target, len)`, a raw
memcpy at +0x28 with **no bound check** against the 0x400 buffer). Target player held in dword_566260.

| Builder | byte0 byte1 (kind) | byte2 | byte3 (len) | payload |
|---|---|---|---|---|
| 0x4460A0(target, code) | 01 01 | code | 04 | - |
| 0x4A4440(target, code, a) | 02 02 | code | 08 | +4 a |
| 0x4493E0(target, code, a) | 04 04 | code | 08 | +4 a |
| 0x449EB0(target, code, a, b) | 03 03 | code | 0C | +4 a, +8 b |
| 0x447D80(target, code, a, b) | 06 06 | code | 0C | +4 a, +8 b |
| 0x449A70(target, code, a, c, b) | 05 05 | code | 10 | +4 a, +8 b, +0xC c |
| 0x4A4400(target, code, a, blk) | 07 07 | code | 20 | +4 a, +8..+0x1F 24-byte block |

(0x447DB0, 0x449AB0, 0x4A4470 are thunks to the post 0x4460C0.) The executor ignores the kind bytes; it switches on
byte2 after `&= 0x7F` (0x438C07; who sets bit 7 is not found, *provisional*: unused).

**Executor** 0x438BF0 (vtbl +0x2C): walks +0x30 for +0x2C bytes calling 0x438C90 per record, then resets +0x2C = 0,
+0x28 = +0x30. **0x438C90** by code:

| Code | Effect | Where sent from |
|---|---|---|
| 1 | nothing (padding) | 0x445F80 |
| 2, 3 | cancel: 0x438F00 (drop +0x430 / +0x434 groups), UI refresh if local | 0x419A0F (world 0x73), 0x41D26B/0x41D452 (warmap menu), 0x4397CE (enter battle), 0x43AF53, 0x43C47B (after a move); 3 from 0x439F37 (leave battle map) |
| 4 | clear selection, start a new one (0x438F50, new 0x449450 at +0x430) | 0x41D372, 0x42C210/260/2F1, 0x43BE31, 0x48464F, 0x484982/9A4, 0x4859AA, 0x4976E7, 0x497828 |
| 5 | select object id a (additive b = Shift) (0x439000; sound 0x109) | 0x42C1EC/23D/291/31E, 0x43C3BB (box, one record per object), 0x47D5D1, 0x49774C |
| 0xA / 0xB | world (dword_4D2550) 0x8050 / 0x8051: time on / off | 0x41A6C0 (local) |
| 0xC / 0xD | world 0x8054 / 0x8055 (same, for the opponent) | 0x442A80 (AI), 0x445FD0 (network) |
| 0x16 | object a's +0x54 (its build/production owner) gets 0x8026 (0, &record+8: the 24-byte block) | 0x4601DE (build order with placement) |
| 0x17 | object a's +0x54 gets 0x8026 (1, b) | 0x46021E (build order) |
| 0x18 | world dword_4D2550 gets 0x8028 with the record | 0x43C431, 0x485B37 (send / move, 16-byte record) |
| 0x19 | object a gets 0x8026 with the record | (no builder call found with 0x19) |
| 0x1A | sector a (0x419240) gets 0x8043 with b | (no builder call found with 0x1A) |
| default (6-9, 0xE-0x15, >0x1A) | commit +0x430 into +0x434 (0x4496C0, 0x800F), then +0x434 vtbl+0x24(record): the selected group handles it | 6 deselect (0x42C1B9), 8 move group to sector (0x43C460), 0x10 (0x49786A), 0x11/0x12 battle-map keys (0x485610), 0x13 force fire (0x485CA0, 0x497805), 0xE (0x485CCC), 0x15 group orders from 0x4807F0 (0x80CE, 0x80F3 for the local player; 0x8118 / 0x8119 appended to the **opponent's** buffer, which in a network game the next Receive overwrites) |

(Jump table 0x438EA8 with index table 0x438EDC, indexed by code-2.)

So player actions that become records: selection (clear/select/deselect, box), cancel, war-map send (8, 0x18), time
on/off, battle-map orders through the selected group (move, force-fire 0x13, keys 0x11/0x12, 0xE), build orders
(0x16/0x17). *Provisional*: anything else the UI does directly (e.g. research picks, production, slider speed) is not
a record and so is not shared; the time speed is not a record (see 5).

**The lockstep loop** — WGame::Tick 0x403680 (once per main-loop tick):
1. `dword_4D2558++` (frame counter) — before the wait, so it counts stalled ticks too.
2. If both players exist: `opponent->vtbl+0x24()` (network: Receive; AI: think).
3. **If `opponent+0x438 == 0 && dword_4F73D0 != 0`: skip everything below** (0x403770-0x403781): no execute, no
   message-queue drain, no world update. This is the wait for the other side.
4. Else `opponent->vtbl+0x28(local)` (network: send the local buffer), `local->vtbl+0x2C()` (execute own records),
   `opponent->vtbl+0x2C()` (execute the opponent's), then drain the game queue unk_4D24C0 (0x434FA0: the first entry
   whose due tick <= dword_4D2558; target vtbl+0x1C) and 0x435430.

The AI's +0x438 stays 1, so single player never waits. One packet = u16 sequence + the local buffer, one per cycle
each way. So it *is* lockstep: both machines simulate, only command records travel. There is **no checksum, no
state sync, no turn number check** (the sequence is written but never read; the "cycle" in the logs is a constant
0), no resend or ack beyond DirectPlay's own guaranteed flag (off by default).

**Pairing of cycles (a defect, read from the code)**: the Human's dword_4F73D0 starts 0, the Martian's 1. Human tick
1 runs without a packet (sends H1, executes H1 + empty). Martian waits for H1, then sends M1 and executes M1 + H1.
Human tick 2 receives M1, sends H2, executes H2 + M1. In general **the Human machine executes H(k) with M(k-1) and the
Martian machine M(k) with H(k)**: the Martian's commands land one cycle later on the Human machine than on its own,
and the two machines run the two players' records in opposite order (local first). Nothing compensates.

**The 0x95 mechanism** (PLAN 4.5): WGame's command handler 0x402B80 and the world's 0x4198C0 store some commands
(world: 0x71, 0x7D, 0x97, 0x98, 0x99, 0xBD, 0x8017, 0x8019 in dword_4E9B64 / dword_4E9B5C) and post 0x95 to
themselves through the game queue (0x4355A0 -> 0x435050: entries {target, msg, w, l, due = dword_4D2558 + delay});
the 0x95 handler (world 0x419A24) dispatches the stored id. Because the game queue is drained only in step 4 above,
these run in the next *lockstep* pass. Note the due tick is in raw ticks (step 1), so a delayed post (timers such as
0x812A at 30) falls due after a different number of lockstep cycles on each machine whenever one of them stalls
(*provisional* that this matters for the simulation; the war map and battle map timers go through this queue).

## 4. Sides, hosting, start file

- Strictly two players, one each side: dwMaxPlayers 2, host always "Human player", joiner always "Martian player".
  The side is the install's: `[Network] I Am Human` / Human.cd vs Martian.cd (0x402630), so a game needs one Human
  install and one Martian install. Main Menu (network version) 0xA4 is labelled 0x3FF (Human) / 0x400 (Martian).
- **Nothing about the campaign is sent.** Host: 0xAB (from the receive thread) -> StartCampaign(slot 0, network 1)
  (0x402E90). Joiner: a successful Join -> the session menu's next command 0xA5 -> the same StartCampaign(0, 1)
  (0x4073D0 -> 0x40761C). StartCampaign 0x4048F0 -> WWarMapWorld 0x417CD0(game, 0, 1): `srand(1)` (0x417DA3), players
  (0x4185F0(1)), load 0x418500(0) -> 0x404C60(0) = `"Human.000"` or `"Martian.000"` (the install's own side's start
  file in the game folder). So each machine loads **its own side's start file**. In the owner's game folders the two
  differ (19312 vs 34172 bytes; ODDITIES says Martian.000 there was saved over). Whether pristine copies hold the same
  world is not shown by the listing (*provisional*: if they differ, the lockstep is wrong from tick 1). Leaving the
  war map then saves slot 0 back over the start file (ODDITIES).
- **Load Network Campaign** (0xA2, 0x402E3B): byte_4D256C = 1, Load Game menu; picking a save sends 0x80B3 ->
  StartCampaign(slot, byte_4D256C) (0x40321A). **No Host or Join is called on that path** (0x44A5F0 has one caller,
  0xA1; 0x44A8D0 one, the session menu), so a loaded network campaign has a network player but no session: the Human
  runs one tick and then waits forever. The joiner can only ever start slot 0.
- byte_4D256C is set only by 0xA2 and cleared by 0x6F (0x402D01). A **new** network game (0xA1/0xA5/0xAB) leaves it
  as it was (normally 0), so cheats stay on (0x401000) and the network time rule below is off.

## 5. Keeping the maps in step

- Same simulation on both machines from the same records; selection state is per player object (+0x430/+0x434), so
  the remote player's records select and order its own units on each machine. Objects are named by id (0x440110),
  which assumes both machines built identical worlds.
- **Time**: on/off are records (0xA/0xB local; 0xC/0xD for the opponent). World +0x68 counts "on" votes (0x419D2A /
  0x419D70): at exactly 1 the calendar flashes (waiting for the other side, +0x38); at 2, with byte_4D256C set, the
  speed is forced to 10 (0x41F640(10)); an "off" sets speed 0. In a network game each machine's 0xA/0xB reaches the
  other in its packet, so two "on"s are needed to run. The network player's own 0xD (0x445FD0, when we turn time off)
  goes into the network player's buffer, which the next Receive overwrites (0x445F20 copies over +0x30 and sets
  +0x2C), so it is lost (*provisional*: it looks intended for the AI case only). Because byte_4D256C is 0 in a new
  network game, the speed is each player's own slider value, which is not a record: the two calendars can run at
  different speeds (*provisional* consequence, but the speed is not exchanged anywhere).
- **Random numbers**: the CRT rand() is seeded 1 at each WWarMapWorld (0x417DA3) and per tree circle with a stored
  seed (0x48F42A), so it is reproducible — but it is one stream shared by gameplay (research 0x458A60, pods 0x41A330,
  deployment 0x4222C0, AI 0x443340-0x444C10, units 0x48xxxx/0x49xxxx) and option-dependent presentation (music step
  0x41B070 with CD music on; water twinkles 0x43A670 with "Show Smoke & particles"; emitters 0x47F050; counter icon
  blink 0x42B2F0). Different options on the two machines shift the stream (*provisional* that each is reached only
  under those options, as the docs say). The **random table** dword_4D14A8[1024] is filled with rand() in WGame's ctor
  (0x402630) **after WinMain's `srand(timeGetTime())` (0x401AE8)**, so it differs per machine; its index dword_4D2580
  is never reset. 0x4025F0 (table % (n+1)) is used by dither 0x410F80, shot quads 0x495280, rockets 0x488980,
  0x48C650 and cascading explosions 0x48CDE0 (*provisional* whether any of these affects damage or positions).
- No other per-tick sync: no checksum of units, no resync, no desync detection.

## 6. Menus and pre-game messages

Host (Human install, network version on):
Main Menu 0x90 -> 0xA4 (0x402E53 -> 0x403D70) **Select Service Provider** (0x406C10, menu id 0xA4 registered with the
name "Main Menu", heading 0x430, lists DirectPlayEnumerate; bottom button 0x530 back) -> pick = Create(SP) -> 0x9F
(0x402DDF -> 0x403C10: shuts down networking first, then **Network Host** 0x406A10, heading 0x3FF; 0xA1 "Start New
Network Game", 0xA2 "Load Network Campaign", 0x8E; back 0x40F -> 0xA4) -> 0xA1 (0x402E75): **Host (0x44A5F0)**, then
**Waiting for Player** (0x403CD0 -> 0x406B30, menu id 0xAC, heading 0x433, bottom 0x40F back to 0x9F, which closes the
session) -> the joiner's DPSYS_CREATEPLAYER -> 0xAB -> StartCampaign(0, 1).

Joiner (Martian install): Main Menu -> 0xA4 -> Select Service Provider -> pick -> 0xA6 (0x402E64 -> 0x403E40: shuts
down networking, then **Game session** 0x407110, prev 0xA4, next 0xA5). Its timer 0x812A every 30 ticks (0x4073D0):
if the enum thread has finished, copy the sessions (GUIDs to menu+0xC8+16i, names to +0x168+16i, count +0x208),
rebuild (0x407260: heading 0x431 "Select Network Campaign" or 0x432 "Waiting for Network Campaign...", up to 10
buttons), restart the enumeration (0x44A400), re-arm the timer. Picking i: **Join (0x44A8D0)**; on success 0xA5 ->
StartCampaign(0, 1) at once.

Messages before the game: none of the game's own. The only exchange is DirectPlay's session create/join and the
host seeing DPSYS_CREATEPLAYERORGROUP. No version check, no side check, no start-file or seed exchange, no "ready"
handshake; the first game packet is the Human's tick-1 buffer.

Shut-down: 0x79 (0x402D31; sent from 0x43E4EA and 0x45A7D3) and 0x90 close the session (0x44AB40). A lost session or
a quitting opponent is not handled: DPSYS_DESTROYPLAYERORGROUP only clears the remote id, and the game keeps waiting
in step 3 (no timeout message, the 10 s waits only loop).

## 7. Why the retail network code "doesn't work" (what the listing shows)

Facts:
1. The cycle pairing is off by one between the machines (section 3): Human runs H(k)+M(k-1), Martian M(k)+H(k), and
   each runs its own records first.
2. Each machine loads its own side's start file; nothing is shared (section 4).
3. The random table is time-seeded per machine (0x401AE8 before 0x402630).
4. "Load Network Campaign" never opens a session; a joiner can only start slot 0.
5. byte_4D256C is not set for a new network game: cheats on, the speed-10 rule off; the slider speed is not a record.
6. Timers are due in raw ticks, which include stalled ticks (0x403749 before the wait).
7. The command buffer (0x400) and receive buffer (0x400) have no bound checks: a box select appends 12 bytes per
   object (0x43C3BB), so ~85 objects in one cycle overflows into +0x430/+0x434 (the selection pointers). (WoWRevived's
   WoW_network_edits.txt has a "network drag select bytes" patch, 80 02 -> 00 08, *provisional* link.)
8. Threading: one Receive per wake of an auto-reset event and none on timeout, so two messages arriving before one
   wake leave the second in DirectPlay's queue until another arrives (*provisional* deadlock: in lockstep nothing
   else arrives); the receive thread writes 0x586380 while the main thread may be copying it; system messages are
   received into the same buffer; the critical section is left but never entered; the threads aren't joined on close
   and can call into a released interface.
9. The service-provider GUID is a pointer kept from the DirectPlayEnumerate callback and used after it returns
   (0x406E3E / 0x406F2B) (*provisional*: whether DPLAYX's pointer stays valid is outside the listing).
10. The session enumeration stores past 10 sessions without a bound.

The WoWRevived network executable is a different, later build (5 Jul 2006, IDirectPlay4, no listing), so none of
the above says how that build fixed things.

## Notes for the rebuild (WoWEngine)

- Keep the record format and executor codes (the local player's queue is also playback's stream). Fix the pairing:
  both sides execute cycle k's records of both players in a fixed order (Human then Martian), e.g. by delaying
  execution of local records until the remote's cycle-k packet is in (classic input-delay lockstep).
- Share the start: host sends the start file (or its hash plus a name) and a seed; seed the random table from it,
  not timeGetTime; keep presentation-only randomness off the simulation stream (MODERN FIX, documented).
- Make speed and every state-changing UI action a record; count timer due-ticks in lockstep cycles.
- Add a cycle number and a per-cycle checksum (desync detection), and handle disconnect.
- Transport: SDL_net or plain TCP with the same two roles (host Human, join Martian), session discovery by UDP
  broadcast to replace EnumSessions.
