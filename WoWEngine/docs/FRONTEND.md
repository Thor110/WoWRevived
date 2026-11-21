# Front end

How the original game builds its menus, worked out from the vanilla exe. The engine copies this structure in `src/frontend`.

## Files and resources

- **File loading** (FILE_Load 0x415200, lookup 0x415030). This is rebuilt in `core/Vfs`.
  - Names starting with `d` are looked up in `DAT\Dat.wow`, and names starting with `m` in `MAPS\MAPS.WoW`. The lookup ignores case and skips the `dat\` or `maps\` prefix.
  - A loose file in `DAT\` with the same name replaces the archive copy (0x414DE0 flags those entries when Dat.wow is opened).
  - Everything else is read as a loose file.
- **Graphics table** (OBJ.ojd, loader 0x437750). This is rebuilt in `core/ObjOjd`.
  - Sprites, fonts and palettes are referred to by GType id. Each table entry has a class, a palette slot, a file name, a hotspot and a "load early" flag.
  - Entries can apply to both sides or to only one side.
- **Strings** (TEXT.ojd, loader 0x414410). This is rebuilt in `core/TextOjd`.
  - The game keeps only the strings for the side being played.
  - `\n` and `\t` are escapes.
  - Characters are converted to the fonts' code page 437 layout with the table at 0x4B9A30 (`core/Charset.h`).
- **Fonts** (measuring 0x40FBF0, drawing 0x40F770). This is rebuilt in `core/Font`.
  - The glyph table starts at offset 0x0A and holds (width, atlas x) pairs. WoWViewer reads it from 0x08, which pairs each glyph's width with the previous glyph's x.
  - The word at 0x08 is a fixed cell width, used by monospaced text objects.
  - Glyph = character − 32. Advance = width + the font's spacing (offset 6).
  - Text objects hold up to 16 lines.
  - Flag bits: 2 centres horizontally, 4 centres vertically, 8 right-aligns.
- **Colours** (LoadShadeTables 0x40B6C0, level lookup 0x40B500). This is rebuilt in `core/ShadeTable` and `core/ShadeGroups.h`.
  - The "palette" number on every OBJ.ojd entry is a shade-table slot, not a PAL file.
  - Loading a palette group (GTypes 0x2ED–0x2F9: hw hb hr bm f1–f7 se cd) fills the slots. For example, group F6 puts `F6F6.SHH` in slot 17, `F6HI.SHH` / `F6MI.SHH` in slot 5 and `F6GI.SHH` in slot 9.
  - A `.SHH` file is a level count byte, then that many levels of 256 RGB565 colours.
  - Objects pick a level: sprites default to 0, and menu text uses the `[Colours]` settings "Menu Heading" (7) and "Menu Items" (4).

- **Sprites** (frame selection 0x40EE10, rows 0x451235). This is rebuilt in `core/Sprite`.
  - Header: `u16 width, u16 height, u16 frames`, then `u32` frame offsets from the start of the file.
  - Each frame: `u32` row offsets from the start of the frame, then RLE rows of (colour, count) pairs, where colour 0 is transparent.
  - WoWViewer reads the offsets as a "carry" byte plus 16 bits, 2 bytes out of step. That agrees for single frames, but not for every animated sprite.
  - Objects pick their frame with +0x38 (8.8 fixed point).
- **Blending** (blitter modes, table at 0x4C0B5F). Objects with flag bit 0 set are drawn at 50%: in 16-bit, each channel is halved and added. In 8-bit, the game used the PAL blend table instead, which gives the checkered look.
- **Text line spacing.** Lines are the font height plus a spacing value apart. Menu text objects (0x40F6D0) use 2. Free-standing text (0x40F620) uses `[Tweak]` "Text Line Spacing", default 4.

## Which side is being played

The game checks for `Human.cd` in the game folder (0x402630) to pick the side. WoWEngine takes `--human` or `--martian` from WoWLauncher instead, and plays the Humans when given neither; the install doesn't copy `Human.cd` or `MARTIAN.cd`. The CD Player plays the side's own disc's music: `Music-Human` in a Human game, `Music-Martian` in a Martian one. Saves are `SaveGame\Human.001`–`.005` or `SaveGame\Martian.001`–`.005` (0x404C60).

## Menus

Every full-screen menu is a WFullScreenMenu (0x40A060). Behind it, a "Fullscreen View" (0x4061B0) draws the backdrop, GType 0x36F (`humanbd.spr`, or `martbd.spr` in Martian games). It uses palette group 0x2F6 (F6) in Human games and 0x2F7 (F7) in Martian games.

Items stack from y = 160, each taking its plate's height, in this order:
1. The heading (0x40A7D0), with text in font 0x6D.
2. The buttons (0x40AA10), with text in font 0x73.
3. The bottom button (0x40AB00). Adding it centres the whole stack on the screen.

Each item has a plate, drawn solid, and a glass panel, drawn at 50%. Both have three frames: normal, highlighted and pressed. The plate's first frame is empty and its others are the outline, so the glass makes the dark panel and the outline shows the selection:

| Menu style | Heading | Button | Bottom button |
|---|---|---|---|
| 0 | 0x3E / 0x3F | 0x38 / 0x39 | 0x38 / 0x3A |
| 1 | 0x3E / 0x3F | 0x43 / 0x44 | 0x38 / 0x3A |
| 2 | 0x40 / 0x41 | 0x3B / 0x3C | 0x3B / 0x3D |

Buttons send WM_COMMAND with their command id to the menu's owner. Escape sends 0x8123 (back), then the previous command, which the menu hands its Fullscreen View, so it fades out as a click does (0x40A1B0 -> 0x4065E0). WGame's handler (0x402B80) opens the next menu. A screen's own command (the credits' exit, 0x405689) fades the same way, and a right click skips the Splash and the credits as a left one does (0x405E6E, 0x405672).

Word wrap (0x416250) sums each character's width (glyph + spacing, 0 below 0x20) and breaks at a character over ' ' once the sum reaches the width (or after a '\n'): the line ends before the word, less the spaces before it, and the next starts at the word with the sum at 0. Runs of spaces inside a line are kept.

Palette groups free all 33 shade slots before loading theirs (0x40B6C0); the CD Player's (0x2F9) also loads the research screen's interface and fonts, `{S}R{S}I` into slot 5 and `{S}RGI` into 9.

### Menus rebuilt so far

| Menu | Id | Built at | Contents (command → string id) |
|---|---|---|---|
| Main Menu | 0x90 | 0x405ED0 | Heading 0x3FB. With the network version enabled: 0xA0 → 0x3FE (Single Player Campaign), then 0xA4 → 0x3FF (Network Campaign, Human) or 0x400 (Join Network Campaign, Martian). Otherwise: 0x6F → 0x406 (only when a save exists), then 0x8138 → 0x40C. Then 0x8E → 0x401, 0x803E → 0x402, and the bottom button 0x80B6 → 0x403. Also starts sound 0x18C. |
| Game Options | 0x8E | 0x408CF0 | Heading 0x401. 0x3EF → 0x410, 0x3ED → 0x411, 0x8117 → 0x412, 0x80CA → 0x534. Bottom button: back to the previous menu → 0x530 (from the main menu) or 0x40F. |
| Quit Game | 0x70 | 0x409460 | Heading 0x403 or 0x404. 0x70 → 0x445 (quits). Bottom button: back to the previous menu → 0x530 or 0x446. From the war map's menu ("Save & Exit Game") it's drawn over the map as that menu is (backdrop 0, the war map's palette group); from the main menu on the menu backdrop. |
| Display Settings | 0x3EF | 0x4096D0 | Style 1. Heading 0x410. Resolution slider ("%s: %d * %d", `[Screen]` Size). 8-bit and 16-bit Colour radios (`[Screen]` BPP). Checkboxes for Lights, Smoke and Particles, Gouraud Shading and Vehicle shadows (`[Options]` "Show lights", "Show Smoke & particles", "Show gouraud shading", "Show vehicle shadows", all on by default). Scroll Speed slider (`[Screen]` "Scroll Speed", default 40, shown as (speed − 10) / 10). |
| Sound Control | 0x3ED | 0x4057F0 | Heading 0x411. Master, Ambient and Speech sliders, 0–100% in tens (`[Sound\Volume]` Master / Ambient / Speech, 0–1). In-game CD music checkbox (`[Sound\Volume]` CD). |
| Critical event options | 0x8117 | 0x409DE0 | Style 2. Two columns of five checkboxes (0x40A480), labels 37 pixels in. `[Event]` "Construction Completed", "Unit Manufactured", "Command Centre Built" (off by default), "Constructor Idle", "Facility Idle" (on), "Unit Arrived At Destination" (off), "Travel Interrupted", "Research Completed" (on), "Resource Warning" (off), "Resource Alert" (on). |
| Load Game | 0x6F | 0x409560 | Style 2. Heading 0x406 ("Resume Campaign"), or 0x434 when there are no saves. One button per existing save, command 0x80B3 + slot, captioned with the save's name (36 bytes of Latin-1 at 0x0C, read by 0x404CD0). |
| Save Game | 0x6E | 0x4090A0 | Opened from the in-game menu. Style 2. Heading 0x52D. Five slot buttons, command 0x8142 + slot, captioned with the save's name or 0x408 "Empty". |
| CD Player | 0x803E | 0x407990 | Its own screen, with backdrop 0xAF and palette group 0x2F9. The layout and commands are documented in `src/frontend/CdPlayer.h`. Music plays from `Music-Human\NN TrackNN.wav` or `Music-Martian\NN TrackNN.wav`, tracks 2 to 5. Its handler also takes transports 4 and 6 (0x4082B2 / 0x4082CF: back or on 5 seconds, 0x413740 / 0x413990, kept to the track; `Music::Seek`), though no button sends them. |
| Credits | 0x80CA | 0x4052B0 | Rebuilt in `src/frontend/Credits.h`, which documents how it works. CREDITS.SPR (0x344) scrolls up in palette group F3 at (height − 640) / 3150 pixels a tick in Human games, or / 4200 in Martian ones, to fit each side's credits music (sound 0x191). Escape or a click leaves after the first 60 ticks. Typing "englishrose" (0x401000) makes it scroll on to a hidden picture of the team at the bottom of the image. |
| Briefing / Debriefing | 0x8019 / 0x97 | 0x499930 / 0x49A7B0 | Rebuilt in `src/frontend/Briefing.h`, which documents them; the war map's battle flow (0x8072 → 0x8133 → 0x8019 → 0x71 / 0x98 → 0x97 → 0x99 / 0x8017) is in WARMAP.md "Battles". Palette group hb (0x2EE), backdrop 0x91. |

| Single Player Options | 0xA0 | 0x4068F0 | The network version's single player menu. Heading 0x40B. 0x8133 → 0x40C "Start New Campaign" with parameter 0x8037 (focused): straight to the disc check and the intro, without picking a slot. 0x6F → 0x406, 0x8E → 0x401. Bottom button: previous menu. |
| Select Campaign to Replace | 0x6E | 0x4090A0 | The save menu above, opened by 0x8138 "Start New Campaign" with the menu it came from (dword_4D2574: the main menu, 0xA0 or 0x9F) as its previous menu. |
| Replace Existing Campaign? | 0x70 | 0x4091E0 | Built with the Quit Game class. Heading 0x532. 0x8140 → 0x469 "Proceed". Bottom button (focused): back to 0x8138 → 0x446 "Cancel". |
| Edit Campaign | 0x8139 | 0x4092D0 | Style 2. Heading 0x52E. A text field (0x40A8C0, focused) holding the campaign's name, up to 36 characters; every change is copied out (message 0x813A, 0x409400). 0x8139 → 0x52F "Begin Campaign". Bottom button: back to 0x8138 → 0x40F. |
| Insert CD | 0x8133 | 0x4089D0 | Heading 0x531. 0x8024 → 0x469 "Proceed". Bottom button (focused): 0x90 → 0x530. Proceed checks the disc again and sends the menu's parameter; while the disc is missing it rechecks every 15 ticks (timer 0x8134). Escape is ignored (0x408AB0). |
| Select Service Provider | 0xA4 | 0x406C10 | Style 2. Heading 0x430. A button per DirectPlay service provider (DirectPlayEnumerateA, up to 10), the first and the last one used focused; picking one goes on to 0x9F (Human) or 0xA6 (Martian). Then a 16 pixel gap (0x4B8DFC − 0x4B8DF8) and the bottom button. |
| Network Host | 0x9F | 0x406A10 | Heading 0x3FF. 0xA1 → 0x40D "Start New Network Game" (focused), 0xA2 → 0x40E "Load Network Campaign" (the load menu with byte_4D256C set), 0x8E → 0x401. Bottom button: back to 0xA4. |
| Waiting for Player | 0xAC | 0x406B30 | Opened by 0xA1. Heading 0x433. Bottom button (focused): back to 0x9F. |
| Game session | 0xA4 | 0x407110 | Opened by 0xA6 (Martian games join). Style 2. Rebuilt every 30 ticks (timer 0x812A, 0x4073D0 / 0x407260): heading 0x431 "Select Network Campaign" over a button per session found (up to 10), or 0x432 "Waiting for Network Campaign..." while there are none, then the gap and the bottom button. Picking a session sends 0xA5. |

### Starting a new campaign

WGame's handler (0x402B80) doesn't act on a command straight away: it stores it and posts 0x95, which runs it. The new campaign goes:

1. 0x8138 (0x404230): Select Campaign to Replace, the save menu with five slot buttons.
2. 0x8142 + slot (0x4043D0): the slot is kept (dword_4D2578). If it holds a save, its name is copied to the name buffer (byte_4D1448) and Replace Existing Campaign? asks first; otherwise the name is cleared and 0x8140 is sent.
3. 0x8140 (0x4045C0): Edit Campaign, with the name buffer in the text field.
4. 0x8139 (0x404660): the new campaign is set up in the slot, then 0x8133 with parameter 0x8037.
5. 0x8133 (0x403FA0): if the right disc is in (0x408B70 looks for Human.cd or Martian.cd in the CD drive's root) the parameter is sent at once; otherwise the Insert CD menu waits for it.
6. 0x8037: the INTRO movie, then 0x8A, which starts the war map.

The other movies go the same way: 0x8036 plays RAGELOGO then 0x8038, which plays TITLE then opens the main menu. Victory and defeat play VICTORY or DEFEAT, then the credits.

### Movies

Start-up (0x4039D0) sends 0x8133 with 0x8036: the disc check, then RAGELOGO, TITLE and the main menu. Each movie is
0x42CCF0(name), which moves the game to 0x8041 and remembers the command for after it (0x4D255C); 0x44DE20 builds
`FMV\<name>.smk`, stops every sound (0x465B20) and plays it (0x44F2C0, SMACKW32.DLL), centred on the 640 x 480
screen, until its last frame or Escape (0x44E3E0, VK_ESCAPE). Every movie on the discs is SMK2, 640 x 180,
Y-interlaced (shown 360 high on every other line), 25 frames a second, with 16-bit stereo sound at 22050 Hz; the
Martian disc's INTRO, VICTORY and DEFEAT differ from the Human disc's.

WoWEngine's `MovieScreen` (0x8041) plays `FMV-Human\` or `FMV-Martian\<name>.mp4` (`FMV\` for TITLE and RAGELOGO,
the folders WoWRevived's Smackw32 wrapper uses) through Media Foundation on Windows, otherwise the `.smk` there or
in `FMV`, with its own Smacker decoder (`core/Smacker`, checked with `wowdump --smk`). An `.mp4` larger than the
screen is scaled down to fit.

### In the engine

- The disc check always passes, because the engine runs from the copy WoWLauncher made of both discs, music included. So the Insert CD menu goes straight on; `--open 0x8133` still shows it.
- Typing in the Edit Campaign field comes from SDL's text input. The field (WEditField 0x430FC0) works as the
  game's: a caret (caret8.spr, GType 0x52, a frame every 5 draws or so: 0x33 / 256 of a frame each) shown while it
  has the focus, starting at 0. WM_CHAR (0x431500) takes only ASCII letters and digits and
  `space ! " # $ & ' ( ) * . : ; < = > ? ^ _`, inserted at the caret; the text is then cut to the field's length,
  and a character at or past the length is refused with sound 0x192 (0x431C30). A click left of the text does
  nothing, otherwise the caret goes after the first character whose right edge is past the click, never before
  the first and, past the end, before the last (0x4313F0). Keys (0x431730): Up the control before, Down and Enter
  the one after, Delete the character after the caret, Backspace the one before, Home, End, Left and Right a
  character or with Ctrl a word.
- Setting up the campaign (0x404660) writes the side's start file to the slot under the new name; loading a save (0x80B3) and starting the war map (0x8A) play the slot's file, and leaving the war map saves it (see WARMAP.md, Saving). Network games (0xA5, 0xAB) show a "not rebuilt yet" placeholder.
- "Skip" (`[Main] Skip=1`, byte_4D1361): WGame::Start (0x402A30) goes straight to Start New Campaign instead of the legal screens and the movies.
- There's no network play yet, so Select Service Provider lists no providers and the session menu always says it's waiting. The Network Host and Waiting for Player menus can be opened with `--open 0x9F` and `--open 0xA1`.

## Sound

- **The sound table** (SFX.ojd, loader 0x463D10) and the three sound libraries are read in `core/SoundTable`; the format is described there. Sounds are played by 0x464EE0 (id, x, y, volume, ...), which makes a 0x54 byte sound object (0x4648C0). A looping sound (opcode 7) plays until it's stopped; another lasts as long as its sample.
- **Volumes.** Each sound's type picks one of the `[Sound\Volume]` settings (0x4CA858 on): 0 Ambient, 1 Engine, 2 Explosions, 3 Weapons, 4 Special, 5 Speech, all under Master.
- **The menus.** The main menu (0x405ED0) starts sound 0x18C, the looping "option" music, afresh each time and fades it in over 60 ticks (0x464E60). Game Options (0x406130) starts it again only if it has stopped. Opening the CD Player (0x407A21) or the credits (0x40535B) fades every sound out over 60 ticks (0x464AB0, "Fading out all current sounds"); the credits then play 0x191, the side's epilogue music (VOX `hepilog` or `mepilog`), and don't leave the bottom of the scroll until it ends.
- **Credits timing.** The scroll steps on message 0x812E, which re-posts itself for the next tick, so it moves every other game tick (about 15 steps a second). That's what makes each side's scroll last as long as its music: 2988 steps for 206.8 seconds of `hepilog`, 3984 for 272.5 seconds of `mepilog`.
- **Control sounds.** The front-end push buttons, checkboxes and sliders play nothing themselves; the click sounds (0x192-0x196) belong to the in-game controls: 0x192 a text field refusing a character, 0x193 / 0x196 a popup menu opening / closed with Escape, 0x194 / 0x195 a popup item (WHotButton, 0x434390) hovered / clicked.
- **The CD Player** has no destructor of its own, so in the original the disc kept playing after leaving it. The engine stops the music when you leave; `[WoW Engine] "CD Player Music Continues" = 1` keeps the original behaviour.

## The mouse cursor

The game draws its own cursor (DeeDraw::DrawFlipCursor, 0x44C700). Controls answer WM_SETCURSOR with a number (0x42E780 sets it), and the table at 0x4B9200 (read by 0x40CD30) gives each number's GType and an offset from the mouse position:

| Cursor | GType | Offset | | Cursor | GType | Offset |
|---|---|---|---|---|---|---|
| 0 | 0x1 normal | 0, 0 | | 0x9-0xB | 0x1 | -8, -8 |
| 1 | 0x2 select | -16, -16 | | 0xC | 0xB "don't" | 0, 0 |
| 2 | 0x3 select + | -16, -16 | | 0xD | 0xC wait | -16, -16 |
| 3 | 0x4 select - | -16, -16 | | 0xE | 0xD add | 0, 0 |
| 4 | 0x9 move | -16, -16 | | 0xF | 0xE subtract | 0, 0 |
| 5 | 0xA attack | -16, -16 | | 0x10 | 0xF promote | 0, 0 |
| 6-8 | 0x1 | -8, -8 | | 0x11 | 0x10 text | 0, 0 |

The menus use cursor 0; the text field (0x431500) asks for 0x11. Each side has its own sprites (`hnrmcurs.spr` and `normcurs.spr`...). The engine hides the system cursor and draws this one; `[WoW Engine] "System Cursor" = 1` shows the system's instead.

## Still to do

- **Shade-table groups.** The hw, hb, hr and bm groups build their shade-table names from unit data. They're needed for the war map and battle map, not the menus.
- **Button frames.** It isn't checked yet which states use which frame (WGraphicPushButton, 0x42FC20). The engine uses 0 = normal, 1 = highlighted, 2 = pressed. The glow animation ("Menu Glow First" / "Menu Glow Last") isn't done.
- **Option controls.** The slider uses sprites 0x345/0x346 (0x347/0x348 in style 2), with 22 frames: 11 knob positions, then the same 11 highlighted. Checkboxes and radio buttons use 0x4D/0x4E (0x4B/0x4C in style 2), with frames for cross, cross highlighted, tick highlighted and tick. Not yet checked against the game:
  - how a slider reacts to clicks and drags (0x431EF0);
  - the event menu's check boxes, which sit about 14 pixels left of the original's while the labels line up.
- **Display Settings.** The engine still draws at 640 × 480 and in true colour. The resolution and colour depth are saved for the launcher, but the engine doesn't use them yet.
- **Loading and saving** work for the war map (see WARMAP.md); battles, research and the economy aren't saved yet beyond what the save already held.
- **Sound for the credits.** The credits music (sound 0x191) isn't played yet. The game also waits for it to finish before leaving the credits.
- **Cheat codes** (0x401000, `FrontEnd::OnKey` / `CheatTyped`; ignored in a network game, byte_4D256C). With none
  under way (dword_4D1130 0) a letter starts one by its first letter: y `youlikeit`, e `englishrose`, s `shym`, d
  `datsun`, c `curtains`, i `icomeback`, p `punyhumans`, a `atchooo`. Each code's handler (0x4011F0, 0x401280,
  0x401320, 0x4013A0, 0x401440, 0x4014F0; `shym` and `datsun` inline) takes the next letter when it's right and does
  the cheat at the last; any wrong letter ends it, and that letter isn't tried as a first one. Every cheat sets
  byte_4D1138. The effects:
  - `youlikeit`: byte_58913C, every product of a sector's three supply ratios taken as 1 (0x41AA16 the Construction
    view's figure, 0x45F82F / 0x4606D0 the factories' speed, 0x429C00 the hourly repair, not in);
  - `englishrose`: byte_4D25A4, the credits' hidden end;
  - `shym`: byte_4D25A0, the next battle's Damage Layer cleared first (0x405039, then the flag's cleared);
  - `datsun`: byte_4BAD38 0, no fog of war (0x434744);
  - `curtains`: byte_4D1279, the `[Debug] "Mat"` flag (what it does isn't traced);
  - `icomeback`: the local player's research all done (0x458460 on dword_4E9B88);
  - `punyhumans` / `atchooo`, with a war map: 0x806C / 0x806D to the sector (0x43B15C), the other side's (0x806C) or
    the player's own (0x806D) units, constructions and resources each taking every battle object out (0x421F50 ->
    0x47D4E0, vt+0x78).

  ResetCheats (0x4011D0, as a war map is made or loaded) clears them all, byte_4BAD38 back to 1.
- **Tooltips.** Done for the front end's controls (the CD Player's transport buttons 0x4BC / 0x4B9 / 0x4BA / 0x4BD /
  0x4BB, Display Settings' colour radios with their labels and the options with 0x453–0x456) and the war map's
  panel buttons, Research button and calendar ("Advance time", only at the third level). Still to do: the war
  map's counters and facilities (0x428D70, 0x42A4B0, 0x426430 / 0x426AE0) and the owner position query (0x80D0).

## Tool tips

The desktop's manager (0x433870) runs every update, gated by `[Screen] "Tool Tip Enable"` (1). If a tip is up for the
window under the mouse and the mouse has been still for less than `"Tool Tip Delay"` (70) it stays, until a mouse
button goes down. Otherwise a button held takes it away; a new window under the mouse marks where the hovering
started; and the time the mouse has been still picks a detail level: 1 past `"Quick Tool Tip Delay"` (15), 2 past 70,
3 past `"Slow Tool Tip Delay"` (150). The window is asked for that level's text (0x807C), then the levels below, and
any text makes a tip (0x49D470): BoxedText (0x49D260 / 0x49D300) in font 0x6F (tooltip.fnt), filled grey
(dword_4DAF90, 6-bit 48, 48, 48) in Human games and black (dword_4DAF88) in Martian ones, outlined in black for the
Humans and the side colour (12, 42, 4) for the Martians, the text 1 in. The window can say where it goes (0x80D0);
otherwise its top left is 32 right of where the hovering started and `[Screen] "Tool Tip Offset"` (16) less 32 below
it. It's kept 4 in from the right edge, its middle at least 4 down, and grown by 2 on the left and top and 1 on the
right and bottom.
- **Sounds.** Menu sounds (0x18C on the main menu, plus button clicks) aren't played yet.
- **Mouse cursor.** The game's own cursor (GTypes 0x22/0x23 and so on) isn't drawn yet.
