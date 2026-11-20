# WoWEngine task list

Built from a survey of the whole game executable (0x401000-0x4A8A92) in 14 pieces; the detailed reports, with every
function, structure, message and setting, are in [`docs/survey/`](survey/) (`partNN.md`, section 6 of each lists the
tasks for that piece with the addresses needed). The aim is a 1:1 match with the original first.

`[x]` done, `[~]` partly done, `[ ]` to do. The part number after each task points to the report.

## 1. Front end

- [x] Legal splash screens at start-up: legal1, legal2 at tick 180, skippable from tick 90, on at 300 (0x405BF0) — 01
- [x] Fade to black between views and the 5-tick command delay (0x4065E0, 0x811B/0x811C/0x811D) — 01
- [x] Focused menu item glow, 4..9 every 2 ticks (0x40ADC0) — 01
- [x] Button frames: 0 normal, 1 highlighted, 2 pressed and highlighted, 3 pressed (0x433DB0) — 05
- [x] Movies: RAGELOGO, TITLE, INTRO; Smacker and WoWRevived .mp4 (0x44DE20, 0x44F2C0) — 01, 07, 08
- [x] CD player: Pause shows Play, Play resumes (0x4081D0) — 01
- [ ] "Skip" setting (straight to Start New Campaign, 0x402A30) — 01
- [ ] Options sliders: (max+1)·(x − 21)/(length − 42), no rounding (0x431EF0) — 05
- [ ] Text field: caret, click to place, Home/End/Delete, Ctrl+arrows, the accepted characters, reject sound 0x192 (0x430FC0) — 05
- [ ] Shift+Tab back a control; the right mouse button presses buttons too — 05
- [ ] Tooltips: delays 15/70/150, `[Screen] Tool Tip Offset` 16, boxed text, CD player button tips 0x4B9-0x4BD, options tips 0x453-0x456 (0x433700, 0x49D470) — 05, 13
- [ ] InfoBox dialog (critical events) (0x432520) — 05, 03
- [ ] Right-click popup menu, sounds 0x193-0x196 (0x432C60) — 05
- [ ] Animated mouse cursors (wait cursor) and the busy icon while loading (0x40CDC0, 0x44E4B0) — 01, 08
- [ ] Sound Control live preview: Master at once, the others on leaving, CD 2 ticks after (0x405A70) — 01
- [ ] Display Settings: resolution list and colour depth as the original (0x409AD0) — 01
- [ ] New campaign in its save slot: copy Human.000 / Martian.000 with the name (0x404660) — 01
- [ ] Saving (0x80B4 → 0x4042D0) and loading through the war map's serialiser; Huffman encoder for saves — 01, 02, 03, 08
- [ ] Game over: VICTORY / DEFEAT movie by code 0x80FA-0x80FD, then credits, then the main menu (0x402B80 0x80FE) — 01, 03
- [ ] Commands one queue pass later (0x95) where the order matters — 01
- [ ] Cheats: youlikeit, shym, datsun, curtains, icomeback, punyhumans, atchooo (englishrose is in) (0x401000) — 01
- [ ] Pause on focus loss, Pause key, Alt+Enter (0x404BC0, 0x42CD40) — 01, 04
- [ ] CD player seek ±5 s (transport 4 / 6) (0x4081D0, 0x412660-) — 01, 02
- [ ] Briefing and Debriefing screens (0x499930, 0x49A7B0; `[Briefing]` / `[Debriefing] Delay` 30 / 300) — 13

## 2. Sound

- [x] Stereo samples (SFX.ojd record 15) — 09
- [x] Volume curve (128-step dB table, 0x465B20), 8 voices with per-type limits and stealing (0x463640), ducking 0.5 / 0.25 / 0.7, variations in turn (0x466BC0) — 09, 10
- [ ] Speech queue: opcode-6 sounds one at a time, no duplicates (0x4652B0) — 09
- [ ] Positional sound: listener at the view centre, inner/outer rects, pan 4·dx (0x464FF0) — needs the battle map — 09
- [ ] Virtual voices that resume at the right place when they get a voice back (0x4654E0) — 09
- [ ] `[Sound]` Minimum Audible Volume, Search Libraries First, Unload Limit; library-before-loose-file order — 09, 10
- [ ] Speech attached to text ids (0x414700) — 02
- [~] War map and battle map music (war map done; battle map to come): CD tracks 2/3 (war map) and 4/5 (battle), ambient 0xFD-0xFF without CD music (0x41B070, timer 0x813C) — 02, 03, 11

## 3. War map

- [x] Map, borders, ownership, panel, calendar, slider drag, routes with the game's A* rules, arrows (decoded curve, head, styles), arrival labels, message list, arrival / battle events — 03, 04, 13
- [ ] WMOB object model: one object per war map object, loaded from the save (0x428BB0, 0x42A190) — 04
- [ ] Hourly step: 0x802D fan-out and the economy phases (0x41A760, 0x423B00, `[Tweak] Resource Drop Off Rate` 3.0) — 03, 04
- [ ] Battle detection (0x41AB00): BattleInSector, UndefendedSectorLost, PodLanded; then the briefing hand-off — 03
- [ ] Critical-event dialogs and the one-at-a-time queue; all 20 event handlers; the `[Event]` toggles (0x41E4E0-0x41F4B0) — 03
- [ ] Martian pods: every 24 h, at most 8, sectors {2,4,6,7,8,9,10,11} (0x41A330) — 03
- [ ] Resource warnings / alerts (0x4198C0) — 03
- [ ] End of game: events 18/19, codes 0x80FA-0x80FD (0x41AED0) — 03
- [ ] Save and load the war map: WMAP, CALN/DtTm, MPAD, routes, WMOB, sectors (0x41AC40) — 03, 04, 07
- [~] Original keyboard (all but M and Backspace, which wait on the message history and the time-running flag): digits for speed, F1-F5 views, Space, Tab, Home, PgUp/PgDn, arrow steps, M, R, Esc, Backspace (0x41A450, 0x41C5E0) — 03
- [ ] Original mouse: rubber band, sector selection, quick right-click cancel, right-hold menu, hover route preview (0x41D300-0x41D590) — 03
- [ ] View shading by unit strength / resource value, enemy sectors in Unit view, pulsing battle sectors (0x4192F0, 0x41DE70) — 03
- [ ] Message list: scroll-up on expiry, history (M, sounds 0x193 / 0x196), re-show last 2 (0x420CD0) — 03
- [~] Calendar: month names from TEXT 0x521+, odometer wheels, a click moves time on an hour (done); the flashing while paused and the time-running flag for the opponent (to do) (0x427500, 0x427710, 0x4278A0) — 03, 04
- [x] Clock sounds 0x1A1-0x1A2 (0x41F860) — 03
- [ ] War map menu items: Load, Sound Control, Event Menu, Quit, Return to Main Menu, with pause and time scale (0x41CD50) — 03
- [ ] Counter decorations: power bar, production blink, idle and repair icons, counts (0x42B850) — 04
- [ ] Selection: Shift-click, box select, cursors; a counter click stops time (0x42BEB0) — 04
- [ ] Repair (MRV / workshops) and construction progress / completion, CommandCentreBuilt (0x4297C0, 0x42A4B0) — 04
- [ ] Context buttons: factory panel, Suspend/Resume, Retreat (0x429A70) — 04
- [~] Research button on the mini-map frame (GType 0x341) (0x423420): in, opening a placeholder until the research screen is rebuilt; its tooltip and flash to do — 04
- [ ] Intel ageing and war map fog: enemy counters for 336 h after contact (0x421630, 0x429B80) — 03, 04
- [ ] Travel: formation offsets, per-hour movement on segment lengths, arrow trim behind the leading units, TravelInterrupted, sea waypoints (0x425080, 0x42AFE0, 0x4373A0) — 04
- [ ] Counter cells shrink when a sector is full (WM clunting res 36 / adj 4) and slide over 5 ticks (0x4205F0) — 03
- [ ] A* details: newest-first ties, no re-sort on improvement, 1000-step limit (0x44EFA0) — 08
- [ ] Sector paths cache (WM_COMMAND 0xB7) for resources and the AI — 14
- [ ] Sector-map border pixels pick the lowest-numbered sector they touch (0x49C070) — 13

## 4. Research, production, AI

- [ ] Research data: the hard-coded trees (6 Human plans, 78 items + 8 triggers; 6 Martian, 84 + 6), states, obsolescence, triggers, the 17 `[Tweak]` multipliers, `[Research] Open Rate` (0x457F20-0x45BCB0) — 08, 09
- [ ] Research screen with its tree, detail panel and scientist speech (0x45E670, 0x45D3F0) — 09
- [ ] Build list / production: 32-item queue, time ×24 h, resource slowdown, repeats, events 1/2/9/10 (0x45F770-0x462EE0) — 09
- [ ] What can be built (command building rules) (0x45F970) — 09
- [ ] War map AI: the 10-state machine per game hour, 14 `[Tweak]` values, AI.ojd sites (0x442890, 0x443340-) — 06, 07
- [ ] Player command queue / order packets (lockstep) (0x438C90, 0x4460A0) — 06, 07

## 5. Battle map

- [ ] Fix `core/Models` terrain: height scale from CLS 0x2C, heights × √2 — 10
- [ ] Landscape: lighting, shadows, placement rules, scorch marks (0x47AF10-) — 10
- [ ] Renderers: the asm blitter modes, triangle spans, WOFLib, RTypeISO, terrain view (0x44F890-0x457F14, 0x49DA50) — 08, 13
- [ ] Objects: factory map, units (WPuppet), vehicles with steering (`[Vehicles]` 23 settings), tripods, fliers, boats, buildings, bridges, fences, mines, pods (0x446B30, 0x48A4C0-, 0x4956D0-) — 07, 12, 13
- [ ] Unit groups, orders, stances, specials (Black Dust ... Scan), order panel (0x480190, 0x47CCF0-) — 07, 10, 11
- [ ] Weapons and effects: shots, explosions, gas, area effects (0x4949A0, part 11) — 11, 12
- [ ] Battle map screen: keys, mouse, menus 0x80B8, minimap, fog of war (0x484A30, 0x4346C0) — 05, 11, 14
- [ ] Battle AI (0x445010) and deployment (0x421FE0) — 03, 07
- [ ] Enter / leave a sector, battle-end dialogs (0x439770, 0x439ED0) — 06

## 6. Data and platform

- [ ] The game's random table: 1024 `rand()` values, MSVC formula, `srand(1)`, inclusive `% (n+1)` (0x4025F0) — 01, 10, 14
- [ ] The ~229 registry/INI settings with their defaults (list in part 14) — 14
- [ ] Unit table: all 65 OBJ.ojd record types and the `[Tweak]` armour / research / manufacture modifiers (0x437750) — 06
- [ ] Object ids: 32768, lowest first then LIFO; the `VEHU` tag (0x4A....) — 14
- [ ] Shade groups hb / hr / bm and time-of-day lighting (fix the ShadeGroups.h comment) (0x40B6C0, 0x40B030) — 01
