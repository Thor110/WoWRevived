# Part 01: 0x401000–0x40D210 (start-up, WGame, front-end screens, shade tables, lighting, renderer)

Source: `pieces/part01.txt` (213 IDA procs plus a few non-proc chunks, such as the "Charlie" setting initialiser at
0x401580 and the vtable destructor stubs `loc_402860`, `loc_4053C0`, `loc_405CD0`, `loc_405DB0` and `loc_408EF0`).
Notation: `obj+0xNN` is a field offset. "Post(msg, w, l, delay)" is `sub_4355A0` (an object posts a message to
itself, delivered `delay` ticks later through the game message queue). "Send" is a direct `vtbl+0x1C(msg, w, l)`
call. `sub_4355D0` posts to the desktop object `dword_4D254C`.

---

## 1. Overview

This piece is the top of the program. It contains:

1. **Static setting objects**: about 40 tiny `atexit`-registered constructors that bind INI or registry keys
   (`[Debug]`, `[System]`, `[Options]`, `[Network]`, `[Screen]`, `[BattleMap]`, `[Game]`, `[Stats]`) to globals.
2. **Cheat-code recogniser** (0x401000–0x4014F0): a typed-letter state machine for eight codes.
3. **WinMain, threads and the main loop** (0x401940–0x402230): DirectX check, language detection from `*.CD`
   files, `SaveGame` directory, game/render/sleeper threads or a single-threaded Peek loop, and the multimedia timer.
4. **WGame** (vtable 0x4A92C0, 0x48 bytes, `dword_4D2538` / `dword_4D130C`). This covers construction (0x402630),
   start (0x402A30), the per-tick update (0x403680) and the render-side update (0x403920). Its command handler
   0x402B80 is the whole front-end state machine. Saves (0x4042D0 / 0x4043D0 / 0x404660 / 0x404CD0), pause and
   resume (0x404BC0 / 0x404C10), the game-time helpers and the 1024-entry random table (0x4025F0) are also here.
5. **Damage Layer** (0x404EA0–0x4051B0): a compressed per-map byte grid that decays. It is used by the battle map.
6. **Front-end screens**: the Credits (0x4052B0), the Legal/GT splash screens (0x405BF0 / 0x405D00), and the
   Fullscreen View base class with its **fade-to-black transitions** (0x4061B0–0x406860). Also the menus: Main,
   Single Player, Network Host, Waiting, Service Provider, Game Session, CD Player, Insert CD, Game Options, an
   unused 5-page picture sequence, Save, Replace, Edit Campaign, Quit, Load, Display Settings and Critical Events.
   The WFullScreenMenu base and its **glow animation** (0x40A060–0x40AED0) and the menu control factories
   (0x40A320–0x40AC80) complete the set.
7. **Frame drawing**: the dirty-rect lists (0x40AEB0 / 0x40AED0) and the per-frame renderer list (0x40AF20).
8. **Time of day and lighting** (0x40AFF0–0x40B6B2, 0x40C930–0x40CB59): sun vector, `LANDR%d%s.int` intensity
   ramps, and point and directional light falloff used by ISOLib, WOFLib, WLandscape and RTypeISO.
9. **LoadShadeTables** (0x40B6C0, 4655 bytes), every palette group 0x2ED–0x2F9, plus the system colours (0x40B120 /
   0x40B3B0).
10. **Mouse cursor object** (0x40CB60–0x40CDC0) and the **Renderer** constructor (0x40CF00, "Renderer", 0xF4
    bytes, vtable 0x4A9920).

There is no hand-written assembly in this piece. Every function has normal compiler prologues and SEH frames. The
cursor drawer 0x40CC70 calls into the asm blitter entry points 0x44F890, 0x44F967 and 0x44FE7F (odd addresses,
outside this piece).

---

## 2. Subsystems

### 2.1 Settings (static constructors)

`sub_411540(section, key, default)` makes a bool setting object (value at +0x10 saved and +0x11 current). Two other
constructors bind a global: `sub_411890(ptr, section, key)` binds a bool, and `sub_411810` binds an int.
`sub_412160(section, key, default)` is an int setting (value at +0x14). `sub_411EE0` is a registry string under
`\0WOW\`. `sub_4115E0` and `sub_411660` are bools with no section.

| Init fn | Section / key | Default | Global (value) | Used by |
|---|---|---|---|---|
| 0x401580 | [Debug] Charlie | 0 | byte_4D1261 | debug-any flag, war map 0x41E4D0, 0x41EA1E, 0x41F098 |
| 0x4015C0 | [Debug] Dave | 0 | byte_4D1231 | debug output (CD errors 0x413F80) |
| 0x401600 | [Debug] Mat | 0 | byte_4D1279 | Time Of Day override (0x40B030), war map 0x41D24A, 0x43D171, 0x4972F6… |
| 0x401640 | [Debug] Edit functionality | 0 | byte_4D1249 | Renderer object list ×50 (0x40D150) |
| 0x401680 | Language (string, 0x14) | "English" | String1 | overwritten by WinMain from `*.CD` |
| 0x4016C0 | game message queue | – | unk_4D24C0 | `sub_434E50` (queue ctor), 0x402500/0x402520 |
| 0x4016F0 | Enable Network Version | 0 | byte_4D12FC | Main Menu 0x405ED0 |
| 0x401730 | [Network] I Am Human | 1 | byte_4B84C4 | the side being played, everywhere (0x404BA0) |
| 0x401770 | Game Frequency | 30 | dword_4B84C8 | timer period 1000/f ms |
| 0x4017B0 / 0x4017F0 | registry Install Path / CD Path | – | unk_4D1298 / unk_4D12D0 | `sub_412090` = get string |
| 0x401830 | Thread Enable | 1 | byte_4D1211 | WinMain |
| 0x401860 | Timer Enable | 0 | byte_4D1291 | WinMain / TimeProc |
| 0x401890 | Sleeper Enable | 0 | byte_4D12C9 | WinMain |
| 0x4018C0 | [System] Game max time (ms) | 25 | unk_4D11C0 | no reader found |
| 0x401900 | [System] Game min time (ms) | 20 | unk_4D1190 | no reader found |
| 0x4022C0 / 0x402300 | [System] Game Memory / Renderer Memory (MB) | 2 / 5 | dword_4D24FC / dword_4D252C | memory pools 0x414780 |
| 0x402340 / 0x402380 | [System] Game / Renderer Memory Handles | 0x2000 / 0x4000 | word_4D148C / word_4D13B4 | same |
| 0x4023C0–0x402480 | [Options] Show lights / Smoke & particles / gouraud shading / vehicle shadows | 1 | byte_4B8808 / 4B880C / 4B8810 / 4B8814 | Display Settings, battle map |
| 0x4024C0 | [Options] Show simple tooltips | 1 | unk_4B8818 | tooltips (0x433xxx) |
| 0x402540 | [Network] Messages Guaranteed | 0 | byte_4D1349 → byte_586C5C | DeePlay send 0x44A1A0 |
| 0x402580 | Skip (no section) | 0 | byte_4D1361 | 0x402A30: skip legal screens and intro, go to "Start New Campaign" |
| 0x4025B0 | [Debug] Save File ID | 5 | dword_4D142C | save-file version passed to 0x44E8A0 |
| 0x402AB0 / 0x402AF0 | [Stats] IRQ mem used / Non IRQ mem used | 0 | – | statistics only |
| 0x404E60 | [BattleMap] Damage reduction divisor | 500 | dword_4D259C | Damage Layer decay |
| 0x40AFF0 | [Game] Time Of Day | 0 | dword_4DA81C | only with [Debug] Mat |
| 0x40CE00 | [Screen] Use DDraw surfaces | – | byte_4B92D8 | Renderer back buffer |
| 0x40CE40 | [Screen] BLIT instead of FLIP | – | – | DeeDraw |
| 0x40CE80 | [Screen] Damage map VRAM | – | – | 0x410230 |
| 0x40CEC0 / 0x40CEE0 | render message queue | – | unk_4DAF28 | |

`byte_4D12F4 = Dave | Charlie | Mat` (0x4019A6) enables debug output (0x466910).

### 2.2 Cheat codes (typed letters)

0x401000 `Cheat_OnChar(c)` runs only when `byte_4D256C == 0`, so not in a network game. `dword_4D1130` selects the
code being typed, picked by its first letter, and `dword_4D1134` is the position in it. A wrong letter resets
`dword_4D1130` to 0. Every completed code sets `byte_4D1138` ("cheated"). `sub_4011D0` resets all the flags.

| First letter → handler | Code | Effect |
|---|---|---|
| y → 0x4011F0 | **youlikeit** | byte_58913C = 1. Read by the war map 0x41AA16, the research screen 0x45F82F and the build list 0x460717/0x460769. Guess: everything available or instant. |
| e → 0x401280 | **englishrose** | byte_4D25A4 = 1. The credits scroll to the hidden team picture (end at height−480, then a 300-tick hold). |
| s → inline | **shym** | byte_4D25A0 = 1. The next Damage Layer update (0x405039) clears the whole damage map once, repairing all damage. |
| d → inline | **datsun** | byte_4BAD38 = 0. **Fog of war off**: WFogOfWar 0x434744 checks it (default 1). |
| c → 0x401320 | **curtains** | byte_4D1279 = 1, which is the [Debug] Mat flag (Time Of Day override and other debug paths). |
| i → 0x4013A0 | **icomeback** | if the war map exists (dword_4E9B70), `sub_458460(dword_4E9B88)`. 0x458460 is in WResearch; guess: completes research. |
| p → 0x401440 | **punyhumans** | if the war map exists and `[world+0x18]==0`: Send(0x111, 0x806C) to the current map target dword_4D2550. Handled at 0x43B15C when `[dword_4E9B74+0x24]==0`, then 0x446370 / 0x421F50. Probably an instant win for the Martians. |
| a → 0x4014F0 | **atchooo** | as above with 0x806D, which applies when `[dword_4E9B74+0x24]==1`. Probably the Humans' "the Martians catch a cold" win. |

### 2.3 WinMain, threads, main loop

| Addr | Name | What it does |
|---|---|---|
| 0x401940 | WinMain | Creates the mutex "The War Of The Worlds". If it already exists, it brings the existing window forward (ShowWindow 9) and exits with 0x45. Requires DirectX ≥ 5 via 0x4122F0 ("DirectX 5 or later required"). Detects the language from `English.CD`, `French.CD`, `German.CD`, `Italian.CD` or `Spanish.CD` in the current directory: it copies the name into the Language buffer and sets dword_4D12F8 = 0–4. Seeds `srand(timeGetTime())`, runs `_mkdir("SaveGame")`, and deletes the file named by HKLM `SOFTWARE\Rage\Jeff Wayne's 'The War O…'` "Autoexec install path" before deleting that value. Creates WGame (0x48 bytes, 0x402630) and calls vtbl+0x14 (Start). With **Thread Enable** it creates two events, the game thread 0x402180 (only when Timer Enable is 0), the render thread 0x402230 and the sleeper thread 0x402080 if Sleeper Enable is set, otherwise `timeSetEvent(1000/GameFrequency, …, 0x4020D0)`, then a GetMessage loop. Without threads: a Peek loop that on each idle pass calls 0x403890, then under the critical section WGame vtbl+0x10 (the tick), then 0x403920 and 0x40AF20 (draw). On exit: `sub_404E10(1)`, stop the threads (1 s timeout, then TerminateThread), kill the timer, WGame vtbl+0x18, release. |
| 0x401FF0 | StartTimer | Restarts the sleeper thread or the multimedia timer (called by the desktop on resume). |
| 0x402050 | StopTimer | `timeKillEvent`. |
| 0x402080 | SleeperThread | Loop: 0x403890, SetEvent(hObject), Sleep(1000/freq). |
| 0x4020D0 | TimeProc | Checks the timer id. 0x403890, then either runs the tick directly (Timer Enable) or signals the game thread. |
| 0x402180 | GameThread | Waits on the event (4 s, "No Timer event yet!"), then under the critical section WGame vtbl+0x10 and SetEvent(hEvent) to wake the renderer. |
| 0x402230 | RenderThread | Waits on hEvent (2 s), then 0x403920 and 0x40AF20. |
| 0x403890 | PreTick | When not quitting: animates the cursor (0x40CDC0) and updates sound (SoundFXSystem 0x462FF0). |
| 0x404E10 | SetQuitting(b) | byte_4D253C = b. When set: PostQuitMessage and PostMessage(hWnd, 0x400). |

The engine replaces all of this with a fixed 30 Hz loop (see ENGINE-MAP). The `1000/freq` period and the order
pre-tick → tick → render-update → draw are what matter.

### 2.4 WGame (application object)

vtable 0x4A92C0: +0 dtor `loc_402860`, +0x10 **Tick** 0x403680, +0x14 **Start** 0x402A30, +0x18 **Shutdown**
0x402B30, +0x1C **OnMessage** 0x402B80, +0x24 0x4354B0. The base class is the WCmdTarget-like at 0x435310–0x4356B0.

| Addr | Name | What it does |
|---|---|---|
| 0x402630 | WGame::WGame(name, hInst) | Sets the side: `Human.cd` in CD Path → I Am Human = 1, else `Martian.cd` → 0. Creates Win95DeskTop (0x8C bytes, 0x42C6C0) → dword_4D254C. Opens the archives (0x414DE0). Creates the two memory pools (0x414780, MB<<20). **Fills the random table** dword_4D14A8[1024] with `rand()`. Loads Text.OJD (0x414410) → dword_4D2544. Then 0x41BF90, OBJ.ojd (0x437750), SFX.ojd (0x463D10), 0x435710, SoundFXSystem (0x462EF0) → dword_5891D8, and DeePlay init (0x449F40). Copies Messages Guaranteed. Sets CD enable from [Sound\Volume] CD (0x412690(dword_4B8A88)) and saves the CD state (0x412680 → dword_4B881C). |
| 0x402880 | WGame::~WGame | Stops CD (0x412C00), releases Text and SFX, shuts down DeePlay (0x449F40, 0x44AB40), clears the current view. |
| 0x402A30 | WGame::Start | 0x42C7E0. `sub_464B10(screenW, screenH)`. **Loads palette group 0x2F1 (F1)**. 0x42DDB0(1). If **Skip**: Send 0x8138 (Start New Campaign); otherwise Send **0x74** (legal splash screens). |
| 0x402B30 | WGame::Shutdown | 0x42CD10, clear views (0x435450 / 0x435570). |
| 0x402B80 | WGame::OnMessage | The state machine; see §4.1. |
| 0x403680 | WGame::Tick | Only while the app is active (byte_4EA3E0). Playback tick 0x42D610. If not quitting: GetCursorPos → 0x42DA50 (in client?) → DrawFlipCursor (0x44C700). Re-entrancy guard byte_4D2534. Recorder 0x42D2D0. **Frame counter dword_4D2558++**. If both players exist (dword_4E9B74 = local player 0x438B20; dword_4E9B78 = opponent: AI 0x442890 or remote 0x445EA0): opponent vtbl+0x24 (think). Unless `opponent[0x438]==0 && dword_4F73D0`: opponent vtbl+0x28(player), player vtbl+0x2C, opponent vtbl+0x2C. Then it drains the **game message queue** unk_4D24C0 (0x434FA0 pops, then target vtbl+0x1C(msg, w, l)) and runs 0x435430. Every 60 frames: memory statistics 0x415410. |
| 0x403920 | WGame::RenderUpdate | 0x42D640. Drains the **render queue** unk_4DAF28 the same way. 0x42D280. dword_4D257C++ (render frame counter). |
| 0x4038C0 / 0x4038D0 | GameTime / SetGameTime(t) | `frame − dword_4D2570`. The setter rebases dword_4D2570 and dword_4D14A4. Used by FMV 0x44E202 and the desktop 0x42CD82 (pause compensation). |
| 0x4038F0 / 0x403900 | UiTime / SetUiTime | `frame − dword_4D14A4`. Used by tooltips 0x433909 (hover delay). |
| 0x4025F0 | **TableRandom(n)** | `table[idx++ % 1024] % (n+1)`, an inclusive range 0..n. Used by vehicles (0x48898E, 0x48C6B0…), ISOLib 0x4110AF and puppets 0x4953C0. The table is filled with `rand()` after `srand(timeGetTime())`, so it is not deterministic between runs. |
| 0x402950 | PostToMap(0x800F) | Send(0x111, 0x800F) to dword_4D2550. Called by the battle map 0x438D3C… (meaning unclear). |
| 0x402970 / 0x4029C0 | ScreenSize / ScreenRect | Static {w, h} and {0, 0, w, h} from desktop+0x34/+0x38. |
| 0x404BA0 / 0x404BB0 | IsHuman / IsMartian | byte_4B84C4. |
| 0x404BC0 | WGame::Pause | On deactivate: saves the CD enable state ("WGame::Pause CD was %s") and disables CD music (0x412690(0)), then 0x44E440. |
| 0x404C10 | WGame::Resume | 0x44E470. Restores CD enable. 0x412670(1). Full redraw (0x40AEB0). |

**Screen builders.** Each one replaces the current view with `sub_435450(view, 0)`:

| Addr | Command | Builds |
|---|---|---|
| 0x4039D0 | 0x74 | Legal splash 0x405BF0(owner, 0, 0x8133, 0x8036) |
| 0x403A40 | 0x8035 | GT logo splash 0x405D00(owner, 0, 0x8133, 0x8036). Apparently unused: nothing sends 0x8035. |
| 0x403AB0 | 0x90 | Shuts down networking (byte_4D2530 = 1, 0x449F40, 0x44AB40), clears the views, Fullscreen View + Main Menu 0x405ED0, then 0x42DB00(1) |
| 0x403B70 | 0xA0 | Single Player Options 0x4068F0(view, prev 0x90) |
| 0x403C10 | 0x9F | Shuts down networking. Network Host 0x406A10(prev 0xA4) |
| 0x403CD0 | 0xA1 | Waiting for Player 0x406B30(prev 0x9F) |
| 0x403D70 | 0xA4 | Service Provider 0x406C10(next 0xA6, prev 0x90). It allocates 0x168 bytes. The extra `new(0x9F)` there looks like a bug or a stray allocation. |
| 0x403E40 | 0xA6 | Shuts down networking. Game Session 0x407110(prev 0xA4, pick → 0xA5) |
| 0x403F00 | 0x803E | Fullscreen View(GType 0xAF, group 0x2F9) + CD Player 0x407990(prev 0x90) |
| 0x403FA0 | 0x8133 | Disc check 0x408B70: on success Send(lParam) at once; otherwise Insert CD 0x4089D0 |
| 0x404090 | 0x8E | Game Options 0x408CF0 |
| 0x404130 | 0x8083 | Picture sequence 0x408E30(exit 0x8076). See §2.6. |
| 0x4041B0 | 0x80CA | Credits 0x4052B0(owner, prev, exit **0x80C9**) |
| 0x404230 | 0x8138 | Save menu as "Select Campaign to Replace" 0x4090A0 |
| 0x4043D0 | 0x8142 (lParam = slot) | Selects the new-campaign slot (dword_4D2578). An existing save copies its name and asks with Replace? 0x4091E0(back 0x8138); otherwise the name is cleared and Send 0x8140 |
| 0x4045C0 | 0x8140 | Edit Campaign 0x4092D0 |
| 0x4047B0 | 0x80B6 | Quit Game 0x409460 |
| 0x404850 | 0x6F / 0xA2 | Load Game 0x409560 (byte_4D256C = network load) |
| 0x4048F0 | 0x8A / 0x80B3 / 0xA5 / 0xAB / 0x8076 | **StartCampaign(slot, network)**. Clears the movie-continuation commands, byte_4D2530 = 1, clears the views, creates **WWarMapWorld** (0x134 bytes, 0x417CD0(owner, slot, network)) → dword_4D2548, 0x42DB00(1). It brackets the build with 0x44E5B0(1)/restore (guess: busy cursor). |
| 0x4049C0 | 0x3EF | Display Settings 0x4096D0 |
| 0x404A60 | 0x3ED | Sound Control 0x4057F0 |
| 0x404B00 | 0x8117 | Critical Event options 0x409DE0 |

**Save and load** (file class 0x44E7D0 ctor(name), 0x44E8A0 open-write(version), 0x44E8F0 open-read,
0x44E930 set-name, 0x44EBB0 copy-body, 0x44ED20 close, 0x44E890 dtor):

| Addr | Name | What it does |
|---|---|---|
| 0x404C60 | SavePath(slot) | Slot 0 → `Human.000` / `Martian.000` in the game folder. Otherwise `SaveGame\Human.%03d`. |
| 0x404CD0 | SaveName(slot) | If the file exists: open, read the header name into byte_586EB8, return it; else NULL. |
| 0x4042D0 | **SaveGame(slot)** | Message 0x80B4, handled immediately and not deferred ("Saving game to slot %d"). Opens for write with Save File ID, then **dword_4D2548 (WWarMapWorld) vtbl+0x0C(file)** serialises the campaign. Sent from the war map 0x41CDF9/0x41CE35. |
| 0x404660 | **NewCampaign(slot, name)** | Opens `Human.000`, sets the new name, **copies its body** into `SaveGame\Human.00slot` (0x44EBB0). The command then continues with 0x8133(0x8037): disc check → INTRO → 0x8A. |

### 2.5 Damage Layer (battle map ground damage)

Class "Damage Layer", vtable 0x4A92E8 (it derives from a named base 0x435640). Fields: +0x10 width, +0x14
height, +0x18 compressed size, +0x1C compressor (0x46-byte object, ctor 0x415470), +0x20 unpacked bytes while
editing, +0x24 packed buffer.

| Addr | Name | What it does |
|---|---|---|
| 0x404EA0 | ctor | Called from the battle map 0x4395D9. |
| 0x404F50 | Create(w, h) | Allocates w×h zero bytes, packs them (0x415520), frees the unpacked copy. Called at 0x439883. |
| 0x405020 | Decay(?, amount) | Unpacks (0x4155B0). With the **shym** cheat it zeroes everything once. Every byte −= `amount / [BattleMap] Damage reduction divisor (500)`, clamped at 0. Called at 0x439898. |
| 0x4050B0 | Repack | 0x415520. Called at 0x439FE2. |
| 0x405100 | Save | Tag `'DMGL'` (0x4C474D44), w, h, packed size, data. |
| 0x4051B0 | Load | The reverse; 0x415780 unpacks. |

What a damage value does (scorched ground drawing) is outside this piece: the Renderer's damage maps 0x410230.

### 2.6 Splash screens, credits and unused picture sequence

All of these derive from the Fullscreen View (§2.7) and share the message handler pattern of 0x405DE0. An enable
message 0x80F2 arrives after N ticks. Before it, Escape (WM_KEYDOWN 0x1B) or a click (0x201/0x204) is only
remembered (+0xA0). After it, they forward the exit command at once.

| Addr | Name | Details |
|---|---|---|
| 0x405BF0 | LegalSplash ctor (0xA8, vtable 0x4A9390) | Backdrop **GType 0x36D `legal1.spr`**, group **0x2F5** (F5). Post(0x80F2, delay **90**). Post(0x8106 (change backdrop), w = **0x36E `legal2.SPR`**, delay **180**). Post(exit = 0x8133, l = 0x8036, delay **300**). So legal1 shows for 6 s, legal2 for 4 s, and either can be skipped after 3 s. Exit: disc check → RAGELOGO → TITLE → main menu. |
| 0x405D00 | LogoSplash ctor (vtable 0x4A93D0) | **GType 0x15 `GTLOGO.SPR`**, group 0x2F4. Skippable after 1 tick, exits after 120. Reached only by 0x8035, which nothing sends. |
| 0x405DE0 | Splash::OnMessage | as above |
| 0x405CF0 / 0x405DD0 | dtors | |
| 0x4052B0 | Credits ctor (0xB4, vtable 0x4A92F8) | GType 0x344, group 0x2F3. +0x98 exit cmd, +0xA4 position = −60.0, +0xAC end offset 960 (480 with englishrose), +0xB0 music handle. Post(0x80F2, delay 60). Focus. Post(0x113 timer 0x812E). Fade all sounds over 60 ticks (0x464AB0). Play **sound 0x191** (0x464EE0(id, x, y, 1.0f, 0)). |
| 0x4053E0 | Credits dtor | Fades the music out over 60 ticks. |
| 0x405470 | Credits::Setup (on 0x8052) | Desktop 0x811D. Display mode 0x42C890(640, 480, 16). Renderer (0x400 objects, tiles 16×8). Speed = (imageH − 640) × flt_4A9334 (Human) / flt_4A9338 (Martian). Loads the palette group. |
| 0x4055C0 | Credits::OnMessage | |
| 0x4056F0 | Credits::Step (timer 0x812E) | Position += speed. If below `imageH − end`, scroll (0x42F6F0). Otherwise wait for the music (0x464E20). englishrose → Post(exit, delay **300**); otherwise Send(exit). |
| 0x408E30 | PictureSequence ctor (0x9C, vtable 0x4A9690) | Backdrop GType `dword_4D25E8[i]` and group `dword_4B8C28[i]` (0x2F3, 0x2F4, 0x2F5…) for page i = dword_4D25FC (0–4). On page 0 plays **sound 0x18D** (dword_4D25E0, +0x24 = 30). Post(0x8091, l = 2). |
| 0x408F20 | PictureSequence::OnMessage | A key (not Escape) or a click → Send 0x8083 (next page; WGame rebuilds the screen). Escape → 0x8076. 0x8083 increments the page; after 4 → 0x8076. 0x8076: fade sound 0x18D over 30 ticks, page = 0, pass to base. byte_4D2540 is set on exit but never read. **dword_4D25E8 is never written (all zero) and nothing outside sends 0x8083, so this looks like dead code (a demo or tutorial leftover).** WGame's 0x8076 rebases game time to frame−5 and starts a new campaign with slot 0. |

### 2.7 Fullscreen View (menu backdrop base class) and transitions

Class "Fullscreen View", vtable 0x4A9450, 0x98 bytes. ctor **0x4061B0**(owner, gtype, group, flags). When gtype
is 0 it uses **0x36F** and group **0x2F6** (Human) or **0x2F7** (Martian). It is a 0x200000-style window
covering the whole screen (0x42F520). Fields: +0x7C Renderer, +0x88 backdrop sprite object (0x40EC50, +0x1E = 1),
+0x8C palette group, +0x90 overlay sprite (black), +0x94 "no transitions" byte (always 0 here).

| Addr | Name | What it does |
|---|---|---|
| 0x4062E0 | OnShow | Desktop 0x8052 then 0x42F610. |
| 0x406310 | Setup (0x8052) | Unless +0x94: Send(**0x811D**, 1) to start the fade-in. Mode 640×480×16 (0x42C890). Renderer(0x100 objects, tiles 128×128) at +0x7C. Loads the palette group (0x42C990). |
| 0x406410 / 0x406470 | Hide / dtor | Release the renderer, overlay and backdrop. |
| 0x406530 | Draw | Renderer begin. Draws the backdrop. If there is an overlay: sets it to cover the client rect (+0x1E = 0x7D01) and draws it. Renderer end and present. |
| **0x4065E0** | OnMessage | **WM_COMMAND (0x111) from any child**: Post(0x811B, same w/l, delay **5**) and start the **fade-out** Post(0x811C, stage 1). **0x811B**: forward the WM_COMMAND to the parent (0x42F650), which is how every menu click reaches WGame 5 ticks late. **0x811C** stage 1: overlay = `bigBlack.SPR` (GType **0x5C**) at 50% (flags bit0 | bit2), then stage 2 a tick later; stage 2: opaque. **0x811D** (fade-in) stage 1: overlay opaque, then stage 2: 50%, then stage 3: overlay removed. **0x8106**: change the backdrop to GType w (0x4067C0). Everything else goes to 0x42F650. |
| 0x4067C0 | SetBackdrop(gtype) | |
| 0x406860 | CreateBlackOverlay | GType 0x5C, flags |= 4 | 1. |

So every front-end screen change has a short black fade: 50% black, then black, then the command five ticks after
the click, then the new screen fades in black → 50% → clear.

### 2.8 Menus (WFullScreenMenu and the individual screens)

WFullScreenMenu ctor **0x40A060**(owner, id, name, prevCmd) uses vtable 0x4A9890 and the font/style table
dword_4E9658. Fields: +0x7C previous/back command, +0x80 glow level (starts at [Colours] "Menu Glow First" = 4),
+0x84 glow direction, +0x88 glowing control, +0x8C, +0x94 style, +0x44 = 0x6662. It Posts the glow timer
(0x113, 0x8129, delay 3).

| Addr | Name | What it does |
|---|---|---|
| 0x40A1B0 | Menu::OnMessage | **Escape** → Send 0x8123. **WM_COMMAND**: clears the glow control and focus (0x435600 when +0x7C is set). For 0x8123 (back): Send(+0x7C) when it is not 0x8123. Otherwise forward to the owner (`[esi+0x3C]` vtbl+0x1C) after 0x42F2A0. **0x803B** → 0x42DB00(1). **Timer 0x8129** → re-post (delay 2) and run **Glow** 0x40ADC0. |
| **0x40ADC0** | Menu::Glow | Every 2 ticks the focused control (0x42EC20) gets message 0x8128 with shade level +0x80. The level ping-pongs between **"Menu Glow First" (4, dword_4B93F4)** and **"Menu Glow Last" (9, dword_4B93F8)**. When focus moves, the old control is reset to **"Menu Items" (4, dword_4B93F0)**. vtbl+0x1C and +0x20 are both called (set the level, then redraw). |
| 0x40A000 / 0x40A020 / 0x40A030 | dtor / dtor / handler that swallows 0x8120 | 0x40A030 is used by checkbox menus. |
| 0x40A7D0 | AddHeading(stringId) | Font 0x6D. Control 0x4301C0. |
| 0x40AA10 | AddButton(cmd, text) | Control 0x430AF0. |
| 0x40A9B0 / 0x40A9E0 | AddButton(cmd, stringId) / focused variant | |
| 0x40AB00 | AddBottomButton | Also centres the stack (0x42EEC0). |
| 0x40A8C0 | AddEditField | Control 0x430FC0. Glow level from dword_4B93F0. |
| 0x40A320 | AddCheckbox(cmd 0x8120, label, **tooltip**) | Control 0x430500. Tooltip strings such as 0x453 "Show vehicle and street lights". |
| 0x40A480 | AddEventCheckbox | Two-column variant for Critical Events. |
| 0x40A630 | AddRadio(groupCmd, cmd, label, tooltip) | Control 0x430E10. |
| 0x40AC80 | AddSlider | "Scrollbar" control 0x431EF0. |

Individual screens (layouts already in FRONTEND.md). Extra details found here:

| Addr | Screen | Notes |
|---|---|---|
| 0x405ED0 | Main Menu 0x90 | Restarts sound 0x18C with a 60-tick fade-in. Network version → 0xA0 / 0xA4 buttons; otherwise 0x6F (if 0x404C60 slot exists) and 0x8138. |
| 0x406130 | Game Options handler | Restarts 0x18C only if it has stopped (0x464E20). |
| 0x4057F0 / 0x405A70 / 0x4059A0 | Sound Control ctor / handler / dtor | Timer **0x812B** each tick refreshes "%s - %d%%" labels for 0x43E Master, 0x43F Ambient and 0x440 Speech, **and applies Master live** (0x4644A0). The dtor applies Ambient to sound types 0–4 and Speech to type 5 (0x464490), plus CD. The CD checkbox applies 2 ticks after the change (0x812D → 0x412690). |
| 0x4068F0 / 0x406A10 / 0x406B30 | Single Player / Network Host / Waiting | |
| 0x406C10, 0x406E20, 0x406F00, 0x406DA0, 0x406DC0 | Service Provider | 0x406E20 is the DirectPlayEnumerate callback: up to 10 entries, GUID pointers at +0x98[i], names as std::string at +0xC0 + 16·i, count +0x160, next command +0x164. Picking i → DeePlay create with that SP (0x44ABB0), then Send(next). |
| 0x407110, 0x407200, 0x407260, 0x4073D0 | Game Session | Timer **0x812A every 30 ticks** re-enumerates sessions (0x44A400 / 0x4672E0) and rebuilds (0x407260). Picking one → 0x44A8D0 (join) then 0xA5. |
| 0x407990, 0x4081D0, 0x4088B0, 0x408170 | CD Player | Done in `src/frontend/CdPlayer.*`. Handler extras: **MM_MCINOTIFY (0x3B9)** "MCI_NOTIFY_SUCCESSFUL" → next track (Send 0x111). Status timer 0x812D. Transport 0x80D3 has **7 cases**: 0 prev, 1 play, 2 stop, 3 next, 5 pause, and **4 / 6 seek ±0x500 with a repeat count in the high word ("Repeat %d")**, which probably come from holding next/prev. 0x80A3 refreshes the track buttons and text (0x4B8B38 table). |
| 0x4089D0, 0x408AA0, 0x408AB0 | Insert CD | Timer 0x8134 every 15 ticks rechecks. |
| **0x408B70** | CorrectCdInserted | Uses the `CD Path` registry value. If the side's `Human.cd` / `Martian.cd` is in the drive root, it reopens CD audio (0x412B50, 0x412960) and returns true. If **the other side's disc is in, it ejects it** (0x413F70: MCI_SET + MCI_SET_DOOR_OPEN) and returns false. |
| 0x408CF0 | Game Options 0x8E | |
| 0x4090A0 / 0x4091E0 / 0x4092D0 / 0x409400 | Save / Replace? / Edit Campaign / edit-changed (0x813A copies ≤36 chars into byte_4D1448) | |
| 0x409460 / 0x409560 | Quit / Load (slot buttons 0x80B3 + slot) | |
| 0x4096D0 / 0x409AD0 / 0x409A90 | Display Settings ctor / handler / dtor | Resolutions: cmd 0x8115 = 512×384, 0x810B = 640×480, 0x810C = 800×600, 0x8108 = 1024×768, 0x8109 = 1152×864, 0x810A = 1280×1024, written to dword_4EA370/374. Only modes in the table unk_4B8C88..4B8CA0 (12-byte entries, available flag at +0xC) are offered. BPP radio group 0x8110: 0x810D = 16-bit (byte_4D2630 = 0, dword_4BAAC4 = 16), 0x810E = 8-bit. The scroll-speed slider writes dword_4BA058 = (pos+1)·10 in the dtor. The checkboxes have tooltips 0x453 / 0x455 / 0x456… |
| 0x409DE0 | Critical event options 0x8117 | |

### 2.9 Frame drawing glue

| Addr | Name | What it does |
|---|---|---|
| 0x40AEB0 | InvalidateAll | `dword_4DA650[buf] = −1` (full redraw). Used by the war map, the desktop, widgets and the battle menu. |
| 0x40AED0 | AddDirtyRect(rect) | Appends to `unk_4D2648[buf][≤0x400]`. On overflow it switches to −1 (full redraw). Two buffers alternate through dword_4DA658. |
| 0x40AF20 | DrawFrame | Swaps the dirty buffers. For each renderer queued this frame (dword_4D2638[], count dword_4DA648, queued by 0x40E0D0): renderer 0 first gets vtbl+0x18(dword_4DAF74) (the cursor?). Then vtbl+0x2C, +0x20, +0x24, +0x28, then release. Finally flip (0x44BF90). |

The engine redraws everything each frame, so it only needs the draw **order**.

### 2.10 Time of day and lighting

The time-of-day table (64 bytes per entry, at 0x4DA828) is filled by the OBJ.ojd loader (0x437750+0x12D). Fields:
+0 int sun angle (degrees), +4 char file letter (B, L, M, N, R; L and N become X and Y in Martian games), and floats
at +8, +0xC, +0x10 and +0x14.

| Addr | Name | What it does |
|---|---|---|
| 0x40B030 | SetTimeOfDay(float idx) | Called by WLandscape 0x47AFA3. The index can be forced by [Debug] Mat + [Game] Time Of Day. It stores dword_4DA820 and computes the sun vector from sin/cos(angle·π/180) with constants −0.5, 0.2, 64, 65536, 1/65536. Results go to flt_4DA760–4DA778, flt_4DAE28 and flt_4DAEBC–4DAEC4 (fixed-point copies). |
| 0x40B570 | LoadLandIntensity | Picks landscape class dword_4DAED0 from dword_4F7430 (≤3 → 0, 4–9 → 1, ≥10 → 2; dword_4F7430 is set by the battle map at 0x43983E; only class 0 files exist in DAT). Loads **`dat\LANDR%d%c.int`** (256 bytes) → byte_4DA660 and finds the ambient threshold (flt_4DA764). Then WLandscape 0x47BD80. Called by battle-map set-up 0x439812. |
| 0x40CB10 | IntensityLookup(f) | `byte_4DA660[clamp(f·k)]`. Used by 0x4A28C0… |
| 0x40CA90 | DirectionalLight(nx, ny, nz, scale, bias) | sqrt(normal·sun)·scale + bias. Used by WLandscape 0x47BE81 and RTypeISO / WOFLib 0x49DFCB… |
| 0x40C930 | PointLight(dx, dy, dz, …, r0, r1) | Distance falloff with a cone/normal term. Used by ISOLib 0x411240… and WOFLib 0x49EEA6 / 0x4A0310. |

### 2.11 Shade tables (LoadShadeTables 0x40B6C0) and system colours

`LoadShadeTables(group)` frees all 33 slots (dword_4DA780[0..32]; level counts in dword_4DAE38[]) and the LANDR
table. The extension comes from the colour depth: 8 → `SHL`, 15 → `SHM`, 16 → `SHH` (off_4B91B4 / B8 / BC); any
other depth gives "Invalid colour depth". {S} = H or M. {T} = the time-of-day letter for group 0x2F0, otherwise
"W". Slot assignments, corrected for the compiler's interleaving: each `mov dword_4DA7xx, esi` belongs to the
*previous* file.

| Group | Files → slots |
|---|---|
| 0x2ED hw | {S}W{S}W→6, {S}W{S}I→5, {S}WGI→9 (as in ShadeGroups.h) |
| 0x2EE hb | {S}B{S}B→8, {S}B{S}I→5, {S}BGI→9, {S}MINIT→21 (this load shares the bm tail at 0x40C436). These are `HBHB` / `HBHI` / `HBGI` / `HMINIT` in Dat.wow. |
| 0x2EF hr | {S}R{S}R→7, {S}R{S}I→5, {S}RGI→9 (the last is the shared tail store, presumed) (`HRHR` / `HRHI` / `HRGI`) |
| **0x2F0 bm** (battle map) | `BMHB{T}`→1, `BMMB{T}`→4, `BMR{d}NA{T}`→10, `BMMV{T}`→3, `BMHV{T}`→2, `BM{S}I`→5, `BMGI`→9, `BMEX`→11, `LANDR{d}{T}`→20, `{S}MINIB`→21. {T} ∈ B, L, M, N, R, X, Y and d = landscape class. |
| 0x2F1–0x2F8 | F1 (12/5/9), F2 (13/5), F3 (14/5), F4 (15/5), F5 (16/5), F6 (17/5/9), F7 (18/5/9), SE (19/5/9) |
| 0x2F9 cd | cdsepia→22, cd1…cd10 → 23–32, {S}R{S}I→5, {S}RGI→9 |

Anything else gives "LoadShadeTables: Invalid palette ID (%d)". The loader then calls **0x40B120**, which builds the
**system colours** dword_4DAF7C–4DAFC8 (red, green, blue, black, white, greys 0x30/0x20/0x10, side colours…)
through **0x40B3B0** `MakeColour(r6, g6, b6)`: nearest palette entry in 8-bit, packed 555/565 duplicated in both
halves of the dword in 15/16-bit. It also sets up PolyLib (0x40FE80, 0x40FEA0, 0x410020).
`0x40B500 ShadeLevel(slot, level) = (level << dword_4DAEC8) + base[slot] + 1`. `0x40B520 LoadShadeSlot(slot, name)`
is the single-file helper.

**ShadeGroups.h says hb, hr and bm "build names from unit data". They don't: hb and hr double the side letter, and
bm uses the time-of-day letter and the landscape class.**

### 2.12 Mouse cursor object and Renderer

| Addr | Name | What it does |
|---|---|---|
| 0x40CB60 | Cursor ctor | Sprite object (GType 1), vtable 0x4A9900, singleton dword_4DAED4 (made by DeeDraw 0x44ADF5). Its image rect is clamped to 64×64. |
| 0x40CD30 | Cursor::Set(n) | Table 0x4B9200: {GType, dx, dy} per cursor, 12 bytes each (in FRONTEND.md). Sets dirty flag +0x48. Callers: 0x40E69C…, 0x44C722. |
| 0x40CDC0 | Cursor::Animate | Each tick advances the frame (0x40EF20(0x80, 1)). If the frame changed it sets dirty. **Animated cursors** such as the wait cursor (hwaitcur / mwaitcur) cycle frames. |
| 0x40CC70 | Cursor::Draw(surface) | Clipped through the asm blitter (0x44F890 / 0x44F967 / 0x44FE7F), shade level from 0x40B500. |
| 0x40CC30 / 0x40CC60 / 0x40CD10 / 0x40CD20 | frame getter / dtor / MarkDirty / IsDirty | |
| 0x40CF00 | Renderer ctor(maxObj, rect, tileW, tileH) | "Renderer", vtable 0x4A9920, 0xF4 bytes. +0x18 origin, +0xE4/+0xE8 size, +0x38 back buffer (a DDraw surface if Use DDraw surfaces, else memory), **4 damage maps** at +0x70–+0x7C (0x410230: tile dirty maps), object list (0x44ED50) with capacity maxObj·10 (·50 more with Edit functionality), +0x6C 12 bytes per tile. |

---

## 3. Data structures and globals

| Global | Meaning |
|---|---|
| dword_4D2538 / dword_4D130C | WGame instance |
| dword_4D254C | Win95DeskTop (0x8C); +0x34/+0x38 client w/h |
| dword_4D2544 / dword_5891D8 | WText / SoundFXSystem |
| dword_4D2548 | current WWarMapWorld (campaign document; vtbl+0x0C = save) |
| dword_4E9B70 | WWarMapWorld `this` (set at 0x417DB1) |
| dword_4E9B74 / dword_4E9B78 | local player (0x438B20, 0x43C bytes) / opponent (AI 0x442890, 0x548 bytes, or remote 0x445EA0) |
| dword_4D2550 | current map command target (war map 0x418CEB, battle map 0x4397C8) |
| dword_4D1490 / dword_4D1398 | deferred command (wParam / lParam), run on 0x95 |
| dword_4D255C / dword_4D2560 | command to send when the current movie ends (0x8056) |
| dword_4D2574 | menu "Start New Campaign" came from (0x90 / 0xA0 / 0x9F) |
| dword_4D2578 / byte_4D1448[37] | new-campaign slot / name |
| byte_4D256C | network load or game (also disables cheats) |
| byte_4D253C / byte_4D2530 / byte_4D2534 | quitting / shutting the world down / tick re-entrancy guard |
| byte_4EA3E0 | app active |
| dword_4D2558 / dword_4D257C | game frame counter / render frame counter |
| dword_4D2570 / dword_4D14A4 | game-time base / UI-time base |
| dword_4D14A8[1024], dword_4D2580 | random table and index |
| unk_4D24C0 / unk_4DAF28 | game / render message queues (ctor 0x434E50, pop 0x434FA0) |
| dword_4EA370 / dword_4EA374 / dword_4BAAC4 | in-game resolution and BPP (Display Settings) |
| dword_4EA3D8 = 640, cy = 480, dword_4BAAC8 = 16 | front-end / movie display mode (set at 0x42C580) |
| byte_4D2630 | 8-bit chosen |
| dword_4BA058 | scroll speed |
| byte_4B881C | CD enabled before pause |
| dword_4DA780[33] / dword_4DAE38[33] | shade-table slots / level bases |
| byte_4DA660[256] | LANDR intensity table |
| dword_4DA820 | current time-of-day index |
| dword_4DAF7C–4DAFC8 | system colours |
| dword_4D2638[] / dword_4DA648 | renderers to draw this frame |
| unk_4D2648 / dword_4DA650 / dword_4DA658 | dirty-rect double buffer |

Structures: WGame 0x48; Fullscreen View 0x98 (§2.7); WFullScreenMenu (§2.8); Credits 0xB4; Splash 0xA8; Damage
Layer (§2.5); Renderer 0xF4 (§2.12); save file (0x44E7D0, header name at 0x0C, 36 bytes).

---

## 4. Messages, commands, strings, tuning

### 4.1 WGame::OnMessage (0x402B80)

- **WM_COMMAND (0x111)**. The ids below are not acted on at once: the handler stores w and l (dword_4D1490 /
  dword_4D1398) and **Posts 0x95**. The 0x95 handler then runs them, so each runs one queue pass later, after the
  sender has finished. Deferred ids: 0x6F, 0x70, 0x74, 0x79, 0x8A, 0x8E, 0x90, 0x9F–0xA2, 0xA4–0xA6, 0xAB, 0xAC,
  0x3ED, 0x3EF, 0x8035–0x8038, 0x803E, 0x8076, 0x8083, 0x80B3, 0x80B6, 0x80C9, 0x80CA, 0x80FE, 0x8117, 0x8133,
  0x8138, 0x8139, 0x8140, 0x8142. Ids 0x71–0x73 and 0xA3 return 0.
- **0x80B4** runs immediately: SaveGame(lParam).
- **0x8056** (movie finished): Send(dword_4D255C), or dword_4D2560 when wParam != 0. The two are always equal.

| Cmd | Action |
|---|---|
| 0x70 | **Quit**: 0x42DB00(0), desktop 0x8042, fade all sounds over **15** ticks, quitting flag, PostQuitMessage, WM 0x400 |
| 0x74 | Legal splash (0x4039D0) |
| 0x79 | Shut down network and world, then Send 0x90 (abort to main menu; sent from 0x43E4EA and 0x45A7D3) |
| 0x8A | StartCampaign(dword_4D2578, 0) |
| 0xA5 / 0xAB | StartCampaign(0, network = 1). 0xAB comes from DeePlay 0x44A37F (the other player joined). |
| 0x8076 | game time base = frame − 5, StartCampaign(0, 0) |
| 0x80B3 | StartCampaign(lParam, byte_4D256C) |
| 0x8036 / 0x8038 / 0x8037 | Movie RAGELOGO → 0x8038, TITLE → 0x90, INTRO → 0x8A. Before each: clear views, 0x44E420, fade sounds 0, 0x42DB00(0), mode 640×480×16, desktop 0x8042, 0x42CCF0(name). |
| **0x80FE** | **Game over** (sent by the war map 0x41F492 / 0x41F4F1 with lParam from 0x4212A0). Shuts the world down, then lParam 0x80FA / 0x80FC → **VICTORY**, 0x80FB / 0x80FD → **DEFEAT**, then credits (0x80CA) → 0x80C9 → main menu. |
| 0x80C9 | clear views, Send 0x90 |
| 0x8139 | NewCampaign(slot, name), then Send(0x8133, l = 0x8037) |
| others | open the screens listed in §2.4 |

### 4.2 Other message ids seen in this piece

0x8052 view setup; 0x80F2 enable skip; 0x8106 change backdrop; 0x811B delayed command; 0x811C / 0x811D fade
out / in; 0x8120 checkbox changed; 0x8123 back / Escape; 0x8128 set glow level; 0x8129 glow timer; 0x812A
session poll; 0x812B sound labels; 0x812D CD apply / CD status; 0x812E credits step; 0x8134 Insert CD poll;
0x813A edit text changed; 0x80A3 CD Player refresh; 0x80D3 CD transport; 0x803F CD track; 0x8042 desktop
(pre-movie / quit); 0x803B menu (cursor on?); 0x800F to map; 0x806C / 0x806D cheats; 0x3B9 MM_MCINOTIFY.

### 4.3 Strings and GTypes

Strings: "The War Of The Worlds" (class, mutex), "DirectX 5 or later required" / "Cannot proceed", "SaveGame",
"%s.%03d", "SaveGame\%s.%03d", "Human" / "Martian", "Human.cd" / "Martian.cd", "Text.OJD", "TITLE", "INTRO",
"RAGELOGO", "DEFEAT", "VICTORY", "Damage Layer", "Fullscreen View", "Main Menu", "Renderer", "LoadShadeTables…",
"dat\LANDR%d%s.int" and the shade-table names above.

TEXT ids: 0x43E–0x440 volume labels, 0x447 / 0x448 colour depth, 0x449 Resolution, 0x44D–0x451 option labels,
0x453–0x456 option tooltips, 0x4B9–0x4BD CD tooltips, 0x531 insert CD, 0x52E campaign name.

GTypes: 0x15 GTLOGO, 0x36D legal1, 0x36E legal2, 0x36F humanbd / martbd, 0x344 CREDITS, 0x5C bigBlack, 0xAF
CD_SEP1.

Sounds: 0x18C menu loop, 0x18D picture-sequence sound, 0x191 credits music.

### 4.4 Tuning (all in the table in §2.1)

Game Frequency 30; Damage reduction divisor 500; Menu Glow First 4, Last 9, Menu Items 4; splash timings
90 / 180 / 300 and 1 / 120 ticks; transition delay 5 ticks; credits 60-tick lock-out and 300-tick egg hold; quit
fade 15 ticks; menu sound fade-in 60; session poll 30; Insert CD poll 15; glow step every 2 ticks.

---

## 5. Cross-references to other pieces

- Settings objects 0x411540, 0x411660, 0x411810, 0x411850, 0x411890, 0x411EE0, 0x412160, 0x4166B0, 0x416770.
- CD audio 0x412660–0x414150.
- Text 0x414410 / 0x4146F0. Files 0x414DE0 / 0x415200. Memory 0x414780 / 0x415410. Compression 0x415470–0x415780.
- War map 0x417CD0 (ctor), 0x41F492 (game over), 0x41CDF9 (save).
- Desktop 0x42C580–0x42DDB0 (0x42C890 set mode, 0x42C990 palette group, 0x42CCF0 play movie, 0x42DB00 cursor? on/off, 0x42D2D0 / 0x42D610 playback).
- Widgets 0x42E0E0–0x431EF0. Fog 0x434744. Queues and commands 0x434E50–0x4356B0. OBJ.ojd 0x437750 (fills the TOD table).
- Battle-map set-up 0x4395D9–0x439FE2 (Damage Layer, LANDR). Player, AI and remote 0x438B20 / 0x442890 / 0x445EA0. Cheats 0x43B15C.
- DeePlay 0x449F40–0x44AB40. DeeDraw 0x44AD20 / 0x44BF90 / 0x44C640 / 0x44C700. FMV 0x44E420–0x44E5B0. Save file 0x44E7D0–0x44ED20. Asm blitter 0x44F890 / 0x44F967 / 0x44FE7F.
- Research 0x458460 (icomeback). Sound 0x462EF0–0x464EE0. WLandscape 0x47AFA3 / 0x47BD80. Lights users 0x411240, 0x49DFCB, 0x49EEA6, 0x4A0310, 0x4A28C0. Random-table users 0x48898E…, 0x4953C0.

---

## 6. Missing in WoWEngine / candidate tasks (prioritised)

**High (visible front-end and flow fidelity)**

1. **Screen transitions (fade to black).** The engine switches screens instantly. Rebuild the Fullscreen View
   transitions (0x4065E0, 0x406860, 0x406310): on any button command, 50% `bigBlack.SPR` (GType 0x5C), then
   opaque, and the command arrives **5 ticks later** (0x811B). New screens fade in opaque → 50% → clear
   (0x811D, one tick per stage).
2. **Legal splash screens at start-up.** The engine starts at the disc check and RAGELOGO. The original posts 0x74
   (0x402A30) → 0x405BF0: `legal1.spr`, then `legal2.SPR` at tick 180, palette group 0x2F5, skippable from
   tick 90, auto-exit at 300 → 0x8133(0x8036). Also the WGame start palette group 0x2F1 and the **"Skip"**
   setting, which jumps straight to 0x8138.
3. **Menu glow animation** (0x40A060 timer 0x8129, 0x40ADC0). The focused item's text shade ping-pongs 4..9 every
   2 ticks; the unfocused level comes from "Menu Items". FRONTEND lists this as not done.
4. **Game-over flow** (0x80FE in 0x402B80). Shut down the war map, play VICTORY or DEFEAT by lParam
   (0x80FA / 0x80FC win, 0x80FB / 0x80FD lose), then credits with exit 0x80C9 → main menu (0x4041B0). Today the
   engine's credits always return to their "previous" menu.
5. **New campaign set-up** (0x404660): copy the body of `Human.000` / `Martian.000` into `SaveGame\<side>.00N`
   with the typed name, then 0x8133(0x8037). The engine logs it and starts from Human.000 without writing the slot.
6. **Saving** (0x80B4 → 0x4042D0): write `SaveGame\<side>.%03d` with version [Debug] "Save File ID" (5) through
   the world's serialiser (vtbl+0x0C). This needs the war-map save writer; the format is in SaveGame.h /
   WARMAP.md.
7. **Deferred command semantics.** WGame runs most commands one queue pass later (0x95). It matters for ordering
   when a command is sent while a screen is closing; worth mirroring in `FrontEnd::Execute`.

**Medium**

8. **Option tooltips** (0x40A320 / 0x40A630 take a tooltip string id, such as 0x453–0x456), gated by [Options]
   "Show simple tooltips" (default 1). The hover delay uses UI time (0x4038F0, used at 0x433909).
9. **Sound Control live preview** (0x405A70): the labels update every tick and Master applies immediately; Ambient
   (types 0–4) and Speech (5) apply on leaving; the CD checkbox applies 2 ticks after a change (0x812D). Check this
   against the engine's SoundControl.
10. **The full cheat set** (0x401000). The engine only has englishrose. Add youlikeit, shym (clear damage),
    datsun (fog of war off, byte_4BAD38 read at 0x434744), curtains (Mat debug and Time Of Day override),
    icomeback (0x458460), punyhumans (0x806C) and atchooo (0x806D). Typing is ignored in network games. The
    cheats are needed once the war map, battle map and research exist.
11. **Pause and resume on focus loss** (0x404BC0 / 0x404C10): pause CD music when the window is deactivated,
    restore it on activation, and force a redraw.
12. **Language detection** by `English.CD` / `French.CD` / `German.CD` / `Italian.CD` / `Spanish.CD` in the game
    folder (WinMain 0x4019F0). Check how the engine picks the TEXT language.
13. **Animated mouse cursor** (0x40CDC0): advance the cursor sprite's frame every tick (the wait cursor and
    others). The engine always draws frame 0.
14. **CD Player transport extras** (0x4081D0): auto-advance to the next track on MCI notify, and seeking ±0x500
    with the repeat count (cases 4 / 6, "Repeat %d"). Check what control sends cases 4 and 6 (holding prev/next?).
15. **Display Settings resolution list** (0x409AD0): 512×384 … 1280×1024 commands and the mode table at 0x4B8C88.
    In the original the front end and movies always run at 640×480×16 (0x42C890) and only the maps use the chosen
    mode.

**For the battle map (later)**

16. **Shade groups hb / hr / bm** (0x40B6C0 cases 0x2EE–0x2F0) with the slot list in §2.11, the time-of-day
    letter and the X/Y swap for Martians. Fix the incorrect comment in `core/ShadeGroups.h`.
17. **Time of day and lighting**: the TOD table from OBJ.ojd, SetTimeOfDay (0x40B030), `LANDR%d%c.int`
    (0x40B570), and the light functions 0x40CA90 (directional), 0x40C930 (point) and 0x40CB10 (intensity lookup).
18. **Damage Layer** (0x404EA0–0x4051B0): w×h bytes, decay by amount/500 per update, the 'DMGL' save chunk, and the
    shym reset.
19. **Random table** (0x4025F0): 1024 `rand()` values with an inclusive `% (n+1)`. Used by vehicles, ISOLib and
    puppets. Use one shared generator with the same range semantics.
20. **System colours** (0x40B120 / 0x40B3B0) for debug lines, the mini-map and side colours, per colour depth.
21. **Network menus** (0x406C10–0x4073D0, 0xA5 / 0xAB / 0x79) once networking exists. Note that the engine has no
    0x79 "abort to main menu" handling.

**Not needed / low**: the single-instance mutex, the DirectX version check, the threads, timer and sleeper
(replaced by the 30 Hz loop), the Autoexec registry clean-up, memory pools and statistics, dirty rects and the
Renderer's damage maps, the GT logo splash (0x8035, unreachable), the unused picture sequence (0x408E30, dead), and
ejecting the wrong disc (the engine's disc check always passes).
