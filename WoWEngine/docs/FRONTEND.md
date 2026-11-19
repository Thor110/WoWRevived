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

The game checks for `Human.cd` in the game folder (0x402630); the launcher renames it to pick the side. Saves are `SaveGame\Human.001`–`.005` or `SaveGame\Martian.001`–`.005` (0x404C60).

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

Buttons send WM_COMMAND with their command id to the menu's owner. Escape sends 0x8123 (back). WGame's handler (0x402B80) opens the next menu.

### Menus rebuilt so far

| Menu | Id | Built at | Contents (command → string id) |
|---|---|---|---|
| Main Menu | 0x90 | 0x405ED0 | Heading 0x3FB. With the network version enabled: 0xA0 → 0x3FE, then 0xA4 → 0x3FF (Human) or 0x400 (Martian). Otherwise: 0x6F → 0x406 (only when a save exists), then 0x8138 → 0x40C. Then 0x8E → 0x401, 0x803E → 0x402, and the bottom button 0x80B6 → 0x403. Also starts sound 0x18C. |
| Game Options | 0x8E | 0x408CF0 | Heading 0x401. 0x3EF → 0x410, 0x3ED → 0x411, 0x8117 → 0x412, 0x80CA → 0x534. Bottom button: back to the previous menu → 0x530 (from the main menu) or 0x40F. |
| Quit Game | 0x70 | 0x409460 | Heading 0x403 or 0x404. 0x70 → 0x445 (quits). Bottom button: back to the previous menu → 0x530 or 0x446. |
| CD Player | 0x803E | 0x407990 | Its own screen, with backdrop 0xAF and palette group 0x2F9. The layout and commands are documented in `src/frontend/CdPlayer.h`. Music plays from `Music-Human\NN TrackNN.wav` or `Music-Martian\NN TrackNN.wav`, tracks 2 to 5. |

### Menus still to rebuild (constructors)

| Menu | Id | Constructor |
|---|---|---|
| Single Player Game | 0xA0 | 0x4068F0 |
| Network Host | 0x9F | 0x406A10 |
| Waiting for Player | 0xAC | 0x406B30 |
| Main Menu, network variant | 0xA4 | 0x406C10 |
| Game session | 0xA4 | 0x407110 |
| Insert CD | 0x8133 | 0x4089D0 |
| Save Game | 0x6E | 0x408E30 |
| Load Game | 0x6F | 0x4052B0 |
| Edit Campaign | 0x8139 | 0x4092D0 |
| Display Settings | 0x3EF | 0x4096D0: checkboxes (0x40A320), spinners (0x40A630) and a slider (0x40AC80) |
| Sound Control | 0x3ED | 0x4057F0: heading 0x411; sliders (0x40AC80, step 10) for strings 0x43E–0x440, bound to the volumes; checkbox 0x441 (command 0x8120) |
| Critical event options ("Event Menu") | 0x8117 | 0x409DE0: event checkboxes (0x40A480) |
| Credits | 0x80CA | not traced yet |

## Still to do

- **Shade-table groups.** The hw, hb, hr and bm groups build their shade-table names from unit data. They're needed for the war map and battle map, not the menus.
- **Button frames.** It isn't checked yet which states use which frame (WGraphicPushButton, 0x42FC20). The engine uses 0 = normal, 1 = highlighted, 2 = pressed. The glow animation ("Menu Glow First" / "Menu Glow Last") isn't done.
- **Controls for the options menus.**
  - The scrollbar slider (0x431EF0) uses sprites 0x345/0x346, or 0x347/0x348 in style 2.
  - The checkbox (0x40A320) uses 0x4D/0x4E, or 0x4B/0x4C in style 2.
  - The spinner is 0x40A630.
  - These menus also need to save settings back to WoWRevived.ini.
- **Tooltips.** The CD Player's transport buttons carry tooltip strings (0x4B9–0x4BD) that aren't shown yet.
- **Sounds.** Menu sounds (0x18C on the main menu, plus button clicks) aren't played yet.
- **Mouse cursor.** The game's own cursor (GTypes 0x22/0x23 and so on) isn't drawn yet.
