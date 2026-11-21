# Parity review: front end and platform

Scope: `src/frontend/*`, `src/platform/*`, `src/main.cpp` loop timing, `core/Font`, `core/TextOjd`, the cursor. Each item
was checked in the IDA listing (addresses quoted) and in the engine source (file:line, relative to `WoWEngine/src`).
Items from the earlier coverage review that have since been fixed are not repeated: the 0x2F9 shade slots, slots
cleared on a group change, credits hold length, credits and Escape fades, right click on the splash and the credits,
the cheat state machine, ResetCheats, and the Damage Layer hand-off.

## Visible bugs

### 1. Menus open with the wrong control focused (Quit, Load, Sound, Display, Event menus). Severity: high
- Original: these builders call 0x42EBE0 (focus) on the control that 0x40AB00 (bottom button) returns:
  - Sound Control 0x40594E
  - Display Settings 0x409A40
  - Critical events 0x409FE4
  - Load Game 0x409676
  - Quit Game 0x40950F
  - Replace Campaign 0x40927E and Insert CD 0x408A61 (the engine already does these two)

  So the glow and the keyboard start on "Return to Main Menu / Previous Menu / Cancel". In the Quit Game menu, the
  default is not "Quit".
- Engine: `Screen::Place` (frontend/Screen.cpp:22) focuses the first usable row, and these builders never call
  `FocusLast()`:
  - `QuitGame` FrontEnd.cpp:260
  - `LoadGame` :193
  - `SoundControl` :272
  - `DisplaySettings` :298
  - `CriticalEvents` :346

  The highlight starts on the first slider, checkbox or save. In Quit Game it starts on "Quit", so Enter or Space
  exits the game at once.
- Fix: call `m->FocusLast()` right after `AddBottomButton` in those five builders.
- Also checked and correct: the main menu (0x405FD3 / 0x406059 / 0x40608E: Single Player, else Resume, else New),
  Game Options (0x408D75, Display Settings), Save (0x40915D, slot 1), Edit Campaign (0x40936F, the field), Single
  Player (0x40696F) and Network Host (0x406A90).

### 2. Wide (style 2) menus sit 10 pixels right of the original. Severity: medium
- Original: Load (0x4095B1), Save (0x4090FB), Edit Campaign (0x409342) and Service Provider (0x406CA1) stack their rows
  from x = 0. Adding the bottom button (0x40AB00, 0x40ABDA-0x40AC3F) moves every child by
  `dx = (menuW - (r - l)) / 2 - l` over the union of the children's rects (0x42E630, applied by 0x42EEC0). HUGEBUT and
  HUGEtit are 540 wide, so every row ends up at x = 50.
- Critical events (0x409E1B-0x409E51): the heading goes at `W/2 - 0x104` = 60, and the check columns at
  `W/2 - controlWidth` and `W/2` (0x40A54A-0x40A580). They are then recentred by the same dx, so the menu shifts
  left by 5-10 pixels, depending on the checkbox control's width.
- Engine: `Menu::Stack` puts style 2 rows at `320 - 260 = 60` (frontend/Menu.cpp:18). `Menu::Centre` skips the
  horizontal centring for style 2 (Menu.cpp:171, "the wide menus are placed already").
- Fix: stack style 2 rows at x = 0, like the other styles, and centre horizontally for every style. For the event
  menu, place the heading at 60 and the columns at 320 - W and 320, then let `Centre` move all of them. W is the
  checkbox control's width (0x430CD0 -> 0x434050 -> 0x42FA70, probably the 270-pixel chkbox2 plate). That would also
  settle the "check boxes about 14 px off" note in docs/FRONTEND.md.

### 3. The research screen fades in and out; the original cuts. Severity: medium
- Original: the research view (0x45E670) sets the Fullscreen View's byte `[+0x94] = 1` (0x45E6B8). That is the only
  writer of this flag in the whole binary. With it set:
  - 0x406310 skips the 0x811D fade-in (0x40632E);
  - 0x4065E0 skips the 0x811B/0x811C fade-out and passes WM_COMMAND straight on (0x4065FB).

  The research screen's handler hands its commands to 0x4065E0 (0x45EB38), so closing it (0xBD) takes effect at once.
- Engine: `FrontEnd::Fades` (FrontEnd.cpp:958) is true for `ResearchScreen::Id`. Opening it fades in from black, and
  the exit button or Esc (0xBD) fades to black and waits 5 ticks.
- Fix: return false from `Fades` for `ResearchScreen::Id`. Better, give `Screen` a `noFade` flag for the view's +0x94
  (see Architecture 1).

### 4. Display Settings doesn't fade in. Severity: low
- Original: 0x409713 (`mov dword ptr [esi+94h], 1`) is the **menu's** style field. The WFullScreenMenu's +0x94 is the
  style, as `dword ptr [edi+94h], 2` in Load and Save shows. It is not the Fullscreen View's no-fade byte, which only
  0x45E6B8 sets (bug 3). Display Settings fades in like every other menu.
- Engine: FrontEnd.cpp:149-150 skips the fade-in for `CmdDisplaySettings`, on the strength of that misreading.
- Fix: drop the `CmdDisplaySettings` exception and fix the comment.

### 5. The CD Player's time panel is drawn translucent; the original draws it solid. Severity: medium
- Original: the status control (0x4303A0, built at 0x407DCF with GType 0xAA cdtmalph.spr) sets only bit 4 on its
  sprite (`or [eax+14h], di` with di = 4, 0x430454-0x43045D). Bit 0, the 50% blend, is set only on glass panels
  (0x430AD9, 0x430D7B, 0x431FEA...).
- Engine: `CdPlayer::Draw` draws it with `Canvas::Blend::Translucent` (frontend/CdPlayer.cpp:150).
- Fix: draw it opaque.

### 6. CD Player: the initial focus, and the focus when Play and Stop swap. Severity: low
- Original:
  - the constructor focuses the Play button (0x407ED4 `call sub_42EBE0` on +0xAC);
  - on 0x80A3 (0x408533-0x4085E4), if Play or Stop had the focus, the one now shown takes it.
- Engine: the first track button has the focus (Screen.cpp:22). When Play or Stop hides, `selected` stays on the
  hidden control: no glow, and the next key jumps to the first usable control (CdPlayer.cpp:83-87).
- Fix: `Focus(playButton)` after the layout. In `Refresh`, move `selected` to the button now visible when it was on
  the other one.

### 7. Right click doesn't toggle radio buttons (or, probably, checkboxes). Severity: low
- Original: the radio handler 0x430E90 treats 0x204 / 0x205 as 0x201 / 0x202 (0x430F07-0x430F0D). The push button
  base 0x42FEF0 does the same (jump table 0x4301A0). Checkboxes (vtable 0x4AA150) inherit that base, but this is
  unverified for them.
- Engine: `Screen::OnRightButton` (Screen.cpp:270) handles only Push and Graphic.
- Fix: let a right press and release on a Check or Radio `Activate` it as the left button does.

### 8. The click fade starts one tick early. Severity: low
- Original (0x4065E0): a WM_COMMAND posts 0x811B with delay 5 and 0x811C(1) with delay 1. Then:
  - 0x811C(1) creates bigBlack (GType 0x5C) at 50% (0x406860, flags 4|1) and posts 0x811C(2) with delay 1;
  - 0x811C(2) clears bit 0, so it goes opaque.

  So: 50% one tick after the click, black on the tick after that, the command at +5.
- Engine: `Dispatch` sets `overlay = 1` in the same frame as the click (FrontEnd.cpp:972), then black on the next
  update.
- Fix: start `overlay` at 0, show 1 when `delayTicks == 1` and 2 from `delayTicks == 2`.

### 9. Multi-line text centred on a point can be one pixel off. Severity: low
- Original:
  - 0x40FBF0 builds the object's box from the **widest** line: for flag 2, `l = -(widest/2)`, using sar;
  - 0x40F770 then centres each line inside that box, at `l + (widest - lineW)/2`.
- Engine: `Canvas::DrawString` centres each line on `box.x + (box.w - lineW)/2` (frontend/Canvas.cpp, the
  `TextCentreX` branch). With a zero-width box, that is `x - lineW/2` per line. Lines whose width parity differs from
  the widest line's land one pixel off. This affects the Briefing heading and the CD Player credits text.
- Fix: when the box is a point (w == 0), use the widest line's half-width as the anchor, as 0x40FBF0 does.

## Behaviour divergences

### 1. The game ticks every 33 ms, not 33.33. Severity: low
- Original: the multimedia timer period, and the thread's Sleep, are `1000 / [..] "Game Frequency"` in integer
  division (0x401CAA-0x401CBD, 0x40202C-0x402040, 0x4020AA-0x4020B7). With the default of 30 (dword_4B84C8), that is
  33 ms, about 30.3 ticks a second.
- Engine: `TickNs = 1e9 / 30` (main.cpp:45), exactly 30 a second, and "Game Frequency" isn't read at all.
- Fix: `TickNs = (1000 / gameFrequency) * 1'000'000`, read from the ini. The 1% difference reaches every tick-based
  timer: the credits, fades and the war-map clock.

### 2. Tool tips: a held button hides the tip only for windows that answer 0x80CF. Severity: low
- Original (0x433870): the window under the mouse is asked 0x80CF first (0x4338E5), and its answer gates the
  GetAsyncKeyState(1/2) checks (0x433916, 0x433943). A window that answers 0 keeps its tip while a button is held.
  Also, once the still time passes "Tool Tip Delay", the tip is rebuilt on every update (0x43390E falls through).
- Engine: `UpdateTooltip` drops the tip whenever a button is held (FrontEnd.cpp, `held`).
- Fix: add a `Screen::TipHidesOnPress()` hook for the 0x80CF answer. What 0x80CF means per control is unverified.

### 3. Sound fades: a fade-out replaces the fade-in factor; the engine takes the smaller. Severity: low
- Original: the per-tick update 0x464CB0 writes `+0x4C`:
  - as the fade-in ratio while `now <= +0x28` (0x464D96-0x464DC3);
  - then, if `now >= +0x2C`, overwrites it with `(end - now) / (end - start)` (0x464DC6-0x464DF1).
- Engine: `v.fade = std::min(v.fade, 1 - out)` (platform/Sound.cpp:301).
- Fix: assign the fade-out ratio rather than take the minimum. This only matters when the two overlap: the menu sound
  faded out while it's still fading in, which happens when leaving the main menu within 2 seconds.

### 4. "Finished" for a sound is time-based. Severity: low
- Original: 0x464E20 is `now >= +0x30`, where `+0x30` = `samples * GameFrequency / (rate * 4) + 3` (0x464999-0x4649AA).
  The credits (0x40574C) and the menu-sound restart (0x405F1F, 0x40613A) use it.
- Engine: `Sound::Active` is "the voice is still in the list", ended by the mixer at the sample's end.
- Effect: equivalent within about 3 ticks.

### 5. Movies play at full volume. Severity: low
- Original: SMACKW32 is driven without SmackVolumePan. Only SmackSoundUseDirectSound, SmackOpen, SmackDoFrame and the
  like are imported. So movie sound ignores [Sound\Volume] Master.
- Engine: `MovieScreen` scales the movie sound by `sound.Master()` (frontend/MovieScreen.cpp:17, FrontEnd.cpp `PlayMovie`).
- Fix: pass 1.0, or keep it as a documented modern choice.

### 6. CD Player: the status time and the track when re-entering. Severity: low
- Original:
  - 0x4088B0 prints `E8 + clock delta`, capped at the track length, while playing; otherwise the last read position E8
    (MCI TMSF). It refreshes every "Game Frequency" ticks (0x812D, 0x408048 / 0x40822D).
  - The constructor reads the CD's current track (0x413D30 -> +0xDC) and only falls back to 2.
- Engine: shows 0:00 whenever stopped and not paused (CdPlayer.cpp:155). It always starts on track 2
  (CdPlayer.h:47), so with "CD Player Music Continues" the picture and title don't match the track playing.
- Fix: keep the music's track in `Music`, and read it in the constructor.

### 7. The fade-out blocks all input. Severity: low
- Original: during the 5-tick wait the view still takes messages. A second command posts another 0x811B, and the
  first 0x811B's queue flush (0x40665C -> 0x435600) drops it.
- Engine: `if (delayed) return;` (FrontEnd.cpp:967).
- Effect: equivalent; no fix needed.

### 8. Debriefing's 0x8017 timer starts at construction. Severity: low
- Original: 0x49B830 (the draw) posts 0x8017 with "Debriefing Delay" only on a draw after the first (+0x120 and
  +0x80 set).
- Engine: counts from construction (frontend/Briefing.cpp `Debriefing::Update`).
- Effect: one frame at most.

## Architecture misalignments

### 1. The Fullscreen View and the menu are merged into one `Screen`
- Original: the view (0x4061B0: backdrop, palette group, no-fade byte +0x94, bigBlack overlay +0x90) is a separate
  object from the WFullScreenMenu (0x40A060: style +0x94, glow +0x80/+0x84, previous command +0x7C). The menu hands
  its commands to the view (0x40A245).
- Engine: `FrontEnd::Fades` hard-codes which screens fade, by id. That led to bug 3 and the misread +0x94 in bug 4.
- Fix: give `Screen` the view's own fields (`noFade`; overlay state per screen), set at construction as 0x45E6B8
  does, and drop the id list.

### 2. Controls are one flat `Button` struct
- Original: a checkbox or radio is a control with its own text (0x434050 -> 0x42FA70, font 0x71), plus a separate
  label child (0x430500 at 0x40A422 / 0x40A5xx) placed with layout flags (0x42F020(0x30)) and coloured
  "Menu Static Text". The CD status is a separate static (0x4303A0).
- Engine: label, glow level and hit area all live on the one `Button`.
- Consequences:
  - the glow (0x40ADC0 sends 0x8128 to the focused control only) lights the label in the engine, which is
    unverified against the original;
  - the per-control sprite flags (bit 0 translucent, bit 2) aren't represented, so bug 5 slipped through.
- Fix: carry explicit per-sprite blend flags on `Button` (plate, glass), set from the factory each control uses. If
  the label/glow question matters, model the label as its own non-focusable row.

### 3. The cursor has no object of its own
- Original: the cursor (0x40CB60, dword_4DAED4) is an RType with a 64 x 64 clipped box, set by 0x40CD30. It is
  animated every tick by 0x40CDC0 (`0x40EF20(0x80, 1)`).
- Engine: `DrawCursor` draws frame 0 statically (FrontEnd.cpp:1344).
- Effect: only the wait cursor (hwaitcur / mwaitcur, 8 frames) animates, and nothing shows it yet (0x40E680 is
  missing). See Missing.

## Provisional items resolved

- **"Display Settings has no fade-in (+0x94, 0x409713)"** (FrontEnd.cpp:149): wrong. 0x409713 is the menu style. The
  only view with no fade is the research screen (0x45E6B8). See bugs 3 and 4.
- **Menu glow timer**: 0x40A0F6 first posts 0x8129 with delay 3, then every 2 (0x40A2F1). The step logic matches
  0x40AE15-0x40AE90. The control the glow leaves is reset to "Menu Items" (dword_4B93F0, 0x40ADE1), as the engine does.
- **Slider input (docs/FRONTEND.md "not yet checked", 0x431EF0)**: WScrollbar's handler is 0x4321B0:
  - Left/Right step the value by 1 within 0..max (0x43224C / 0x4322AF);
  - Up/Down go to the control before or after (0x8008 / 0x8007);
  - a click captures the mouse (0x42E9B0) and sets the value;
  - a drag updates only with the left button held (0x43240F);
  - the position is `(max + 1) * clamp(x - m1, 0, len - 1) / len`, with `len = width - m1 - m2` (0x432378-0x4323A3).

  The engine's `SetSliderFromX` and `OnKey` match.
- **Push button keys** (0x42FEF0):
  - Enter and Space send the command on key-down (jump table 0x430184: 13, 32);
  - Left/Up -> 0x8008, Right/Down -> 0x8007;
  - Tab is handled by WControl 0x433F20 (Shift for 0x8008);
  - a mouse move with the button held over the control re-presses it, and moving off unpresses it (0x43002E /
    0x43004E).

  The engine matches, except it keeps drawing the pressed frame after the mouse leaves a held button
  (Screen.cpp `OnMouseMove`). That is a low-severity visual point.
- **Cursor hot spots**: 0x40CD30 overwrites the sprite box's left/top with the table offset (0x40CD58-0x40CD71), so the
  OBJ.ojd hotspot isn't applied. All cursor GTypes 1-4 and 9-0x10 have hotspot 0,0 on both sides (`wowdump --obj`),
  so the engine's extra `-hotX` is harmless.
- **War-map music track order**: dword_4B9DE0 starts at 1 and toggles with `(n+1) % 2` (0x41B0EE-0x41B103) over
  ArgList {2, 3}. Tracks of 10 seconds or less are skipped and retried in 10 s; a CD position that isn't zero means
  stop and retry in 10 s (0x41B0D5-0x41B173). The engine (FrontEnd.cpp `MusicStep`, FrontEnd.h:164) matches.
- **Text metrics**: line width is the sum of `glyph width + spacing` for every character, including the last
  (0x40FC41). Height is `lines * fontHeight + (lines - 1) * spacing` (0x40FCAA). Centring uses cdq/sar, which rounds
  toward zero. Monospace (+0x3D = 0) is never set by 0x40F620 / 0x40F6D0. The engine's `Font::Measure` and
  `DrawString` match, apart from bug 9.

## Missing

- **Cursor animation and the wait cursor** (0x40CDC0, 0x40E680 / 0x40E6E0). Low. Keep an 8.8 frame counter
  (+0x80 a tick) for the cursor sprite. Show cursor 0xD while saving, loading or doing the disc check.
- **"Game Frequency" / "Game max time" / "Game min time" / "Thread Enable" / "Timer Enable" / "Sleeper Enable"**
  (0x401770-0x401900). Low. Only Game Frequency affects play: see Behaviour 1.
- **CD Player:**
  - the 0x8040 command (0x408442 -> 0x4393D0) has no engine counterpart; its purpose is untraced and nothing in the
    CD Player sends it;
  - on MM_MCINOTIFY (0x3B9, 0x4084C1) with MCI_NOTIFY_SUCCESSFUL, the original sends WM_COMMAND with id 3 to itself
    (0x4084DB-0x4084F5). Id 3 matches none of the handler's cases (0x803F, 0x8040, 0x80D3), so it falls through to
    the menu base 0x40A1B0. What the disc does next is left to MCI (the play command's range, 0x414150, untraced).
    The engine instead goes on to the next track and stops after the last (CdPlayer.cpp `Update`). That behaviour
    is unverified against the original.
- **Engine inventions to keep in mind** (no original counterpart):
  - `NotRebuilt` placeholder menus (FrontEnd.cpp:542, 833, 954);
  - `BackTo` / `menuParents` loop-avoidance (FrontEnd.cpp:163);
  - the "not rebuilt returns to Options/Load/Save" previous menu rule (FrontEnd.cpp:153);
  - `[WoW Engine] "CD Player Music Continues"` and `"System Cursor"`;
  - stopping the CD music when leaving the CD Player.
