# Part 05: 0x42DA10–0x437380 (window system, UI controls, fog of war, message queues, sector link tables)

Survey of `pieces/part05.txt` (194 functions). Addresses are vanilla `WoW_patched.exe`. "Guess" marks anything
not proven from the code; "unclear" means I could not tell.

## 1. Overview

The piece is almost entirely Rage's **home-grown window/GUI toolkit** (a Win32-like retained-mode window tree
with its own message dispatch), plus a few small engine pieces that the compiler placed after it:

| Range | Contents |
|---|---|
| 0x42DA10–0x42DDF0 | Tail of Win95DeskTop: cursor position scaling, colour-depth helpers, the root desktop's message handler, pause/resume |
| 0x42DE00–0x42F6F0 | **WWindow** base class: tree (children, overlay children), z-depth, coordinates, show/hide, destroy, hit-testing, invalidation, capture/focus/hover/modal globals, mouse dispatcher; **WView** (0x42F520) |
| 0x42F730–0x42F9A7 | **WDialog** (menu/dialog panel: Esc, Tab order, open/close notifications) |
| 0x42F9B0–0x430E90 | Push buttons (WGraphicPushButton family), menu items (plate+glass+text), checkboxes, radio buttons |
| 0x430FC0–0x431DD0 | **WEditField** (text entry with caret) |
| 0x431E30–0x43251B | **WScrollbar** (sliders: options sliders, war-map speed slider) |
| 0x432520–0x432C56 | **WInfoBox** (modal "Press button to continue" message box for critical events) |
| 0x432C60–0x4333AB | **WPopupMenu** (right-click command menu, icon buttons RBH*.spr) |
| 0x4333B0–0x433660 | View/radio buttons, overlay windows |
| 0x4336D0–0x433B55 | **WDesktop** root window, **tool tips** (delays from `[Screen]`) |
| 0x433B60–0x4342C0 | **WGraphicControl / WControl** bases (frame-state logic), toggle buttons, popup hot-buttons with sounds |
| 0x4344C0–0x434E49 | **WFogOfWar** (battle map fog, `[BattleMap]` tuning) |
| 0x434E50–0x435620 | Thread-safe **delayed message queues** (game queue 0x4D24C0, display queue 0x4DAF28) |
| 0x435310–0x435450 | **WWorld** base ("World tasks", current sub-world) |
| 0x4354C0–0x4356B0 | WObject / WTask base-class helpers (refcount, name, active flag, magic 'WOBJ') |
| 0x4356C0–0x43737F | **Sector link data**: land-link test, link-kind table builder (0x435710), battle-map entry points per link (0x436050, a 91-case switch) |

No hand-written assembly in this piece: everything has normal MSVC prologues, SEH frames and `__thiscall`
conventions. The large functions 0x435710 and 0x436050 are compiler-generated switch tables of constants, not
asm. (The range 0x437342–0x43737F at the end of 0x436050 is the switch's index-byte table, which IDA shows as
`push cs`/`or` instructions; it is data.)

## 2. Subsystems

### 2.1 Win95DeskTop tail / platform glue

| Addr | Name (proposed) | What it does |
|---|---|---|
| 0x42DA10 | `GetCursorPosLogical` | `GetCursorPos`; when `byte dword_4EA364 == 0` also maps through 0x42DA50. Callers: battle map 0x43ADA0, 0x484DF0, 0x4A0630, 0x4A14C0. |
| 0x42DA50 | `ScreenToLogical(POINT*)` | If `dword_4EA364 == 0` (guess: windowed mode): `ScreenToClient`, scale by logical size (`dword_4EA3A0` × `dword_4EA348`, set by 0x42C9B0) over the client rect; a point outside is replaced by the screen centre. Always returns 1. |
| 0x42DB00 | `SetGameCursorVisible(bool)` | Posts 0x8084 (param = bool) to the desktop through the display queue (0x4355D0), flushes (0x44AD20) and returns the old `byte_586CA0` (DeeDraw cursor-visible flag, guess). 13 callers (WGame handler 0x402B80, war map 0x418980/0x4198C0/0x41C5E0, research 0x458E50/0x45E9E0...). |
| 0x42DB30 | `GetPixelFormatCached` | Caches DeeDraw 0x44C640 (`dword_586DF0`) in `dword_4EA368`. |
| 0x42DB50 | `GetColourDepthClass` | 8 bpp → 1, 15/16 bpp → 2, else 0. Used by screen settings 0x40CF00. |
| 0x42DB90 | `Win95DeskTop::WndProc` (vtable 0x4A9F90 slot 7) | Ctrl+E (WM_KEYDOWN/WM_CHAR 'E' with VK_CONTROL) → WM_COMMAND 0x8049 (no handler found anywhere in this exe: dead debug key, guess). 0x204 (right button down) stores the tick in `dword_4EA404` (used by 0x42D610 to send 0x805B "right button held"). 0x3B9 MM_MCINOTIFY → broadcast to children (CD audio). 0x803B: hide cursor, `0x42C9B0(w,h,bpp,flag)` (change display mode), broadcast 0x803B. 0x803D → `LoadShadeTables(param)` 0x40B6C0. 0x8041 → 0x44DCA0 (start movie thread). 0x8042 → 0x44BD90 (DeeDraw restore surfaces, guess). 0x8084 → 0x44CFA0 (cursor visible). Everything else → WDesktop handler 0x433AB0. |
| 0x42DDB0 | `WGame_SetRunning(bool)` | Edge-triggered on `byte_4EA3E0`: "WGame::Resume()" → 0x404C10, "WGame::Pause()" → 0x404BC0. Caller 0x402A30 (app activation). |
| 0x42DDF0 | static init | Zeroes the global mouse point `pt`. |
| 0x42E750 | `ErrorMessageBox(text, caption, type)` | Flips DeeDraw to GDI (0x44BEC0) then `MessageBoxA(hWnd, …, type | MB_SYSTEMMODAL)`. Called by DeeDraw/FMV error paths 0x44AF40…0x44E4B0. |

### 2.2 WWindow base class (vtable 0x4A9FD0)

Constructor 0x42DE00 (`WWindow(id, RECT*, flags, ?, parent)`), destructor 0x42DFE0. Derives from WTask
(0x4354C0) → WObject (0x435640). Every UI element in the game (menus, war-map panels, battle-map HUD, research
screens) is one of these.

Vtable layout (15 slots, shared by every subclass):

| Slot | Off | Base impl | Meaning |
|---|---|---|---|
| 0 | +0x00 | 0x42DFC0 | scalar deleting destructor |
| 1 | +0x04 | 0x4356A0 | returns magic 0x4A424F57 ('WOBJ') |
| 2,3 | +0x08/0C | 0x4356B0 | return true (IsValid, guess) |
| 4 | +0x10 | 0x435590 | Tick: sends WM_TIMER 0x113 to itself |
| 5 | +0x14 | 0x435560 | Activate (task +0x18 = 1) |
| 6 | +0x18 | 0x435570 | Deactivate |
| 7 | +0x1C | 0x42E290 | **WndProc(msg, wParam, lParam)** |
| 8 | +0x20 | 0x42E0E0 | Broadcast message to children |
| 9 | +0x24 | 0x42E150 | Draw(displayList, x, y) |
| 10 | +0x28 | 0x42E220 | Draw children only |
| 11 | +0x2C | 0x42E450 | ScreenToClient(POINT*) |
| 12 | +0x30 | 0x42E570 | ClientRectToParent(RECT*) |
| 13 | +0x34 | 0x42E4E0 | ClientToScreen(POINT*) |
| 14 | +0x38 | 0x42EC70 | Invalidate (add rect to dirty list 0x40AED0; root with flag 0x200000 → full redraw 0x40AEB0) |

Functions:

| Addr | Name | What it does |
|---|---|---|
| 0x42DE00 | ctor | Fills the fields below; magic guards 0x87654321/0x98765432/0xA9876543 at +0x1C/+0x20/+0x24; z = 0x4650, or parent z + 0x14 for an overlay; adds itself to the parent (or the desktop 0x4336D0 if none); flag 0x100000 creates a black background object (GType 0x5C `bigBlack.SPR`). |
| 0x42DFE0 | dtor | Hides (0x42F310(0)), deletes children of both lists, unlinks from parent, clears every global that points at it (focus, capture, modal, hover, root). |
| 0x42E290 | base WndProc | WM_SETTEXT 0xC stores +0xC; WM_GETTEXT 0xD copies it; WM_SETCURSOR 0x20 → cursor 0; WM_COMMAND 0x111 forwarded to parent; 0x800C (keyboard navigation?) → 0; 0x80CF (wants tool tip?) → 1. |
| 0x42E0E0 | Broadcast | Sends msg to every visible overlay child, and to every normal child (optionally slot 8 recursion). |
| 0x42E150 | Draw | Positions/draws the background object, then children (overlays at absolute pos, normal children offset by scroll). |
| 0x42E220 | DrawChildren | as above without background. |
| 0x42E330 | HitTest(window) | Point `pt` (global): recursive search of visible normal children, then overlay children; returns deepest window containing it, or 0. |
| 0x42E450 / 0x42E4E0 / 0x42E570 | coordinate conversion | Overlay windows (flag bit 30) use +0x2C/+0x30 relative to parent and recurse; normal ones use +0x50/+0x54 absolute minus scroll +0x58/+0x5C. |
| 0x42E600 | GetRect | {x, y, x+w, y+h}. Caller 0x484A30. |
| 0x42E630 | GetChildrenBounds | Union of normal children rects, or the screen rect `dword_4E9658..64` if none. Used by 0x40AB00 (menu centring). |
| 0x42E700 | SetScroll(x,y) | Stores +0x58/+0x5C, returns old. |
| 0x42E730 | BringToFront | Remove + re-add to parent. |
| 0x42E800 | AddChild | Overlay (flag 0x40000000) → list +0x68/+0x6C; normal → list +0x60/+0x64, new child z = old first child z + 2. |
| 0x42E880 | RemoveChild | Unlinks from either list. |
| 0x42ED40 | InsertBefore(sibling) | Moves window in z order next to another (caller research 0x45CF10). |
| 0x42EDA0 | SetSize(w,h) | |
| 0x42EDC0 | Move(x,y) | Relative move; propagates absolute position to children (0x42EE60); re-hit-tests hover. |
| 0x42EE60 / 0x42EEC0 | OffsetChildren | recursive absolute offset. |
| 0x42EF60 | SetRect(RECT*) | |
| 0x42F020 | Align(flags) | In parent: 0x10 centre-x, 2 left, 8 right; 0x20 centre-y, 1 top, 4 bottom. Used by menu builders 0x40A320…0x40AC80. |
| 0x42F130 | ClampInParent(mx, my) | Keeps window ≥ margin inside parent; returns bits 2 (left), 8 (right), 1 (top), 4 (bottom) that were clamped. |
| 0x42F2A0 | Destroy | Hide, WM_DESTROY (2), destroy children recursively, delete self. |
| 0x42F310 | Show(mode) | 0 = hide (clear visible bit 0x10000000, invalidate, drop focus/capture/modal/hover/root), 5 = show (0x42F3D0(0x10000000)); recursive. |
| 0x42F3D0 / 0x42F480 | SetFlags / ClearFlags | Handles visible (invalidate) and 0x100000 (create/delete background). |
| 0x42F4E0 / 0x42F500 / 0x42F510 | sound position | +0x48/+0x4C, initialised to the listener centre (`dword_4E9250`, `dy`), read by 0x446690. Guess: where the window's sounds are placed. |
| 0x42E780 | SetCursor(id) | `dword_4EA438` (the cursor table index, see FRONTEND.md). 21 callers. |
| 0x42E790 / 0x42E7E0 | SetCursorQuery(x,y) / Requery | Stores `dword_4EA43C/440` and resends WM_SETCURSOR 0x20 to the hover window. |
| 0x42E9B0 / 0x42E9F0 / 0x42EA00 | SetCapture(childrenToo) / GetCapture / ReleaseCapture | `dword_4EA428`, `byte_4EA430`; old capture gets 0x215 WM_CAPTURECHANGED. |
| 0x42E9E0 | SetModal | `dword_4EA42C`; returns old. |
| 0x42EA30 | **DispatchMouse(msg, wParam, x, y)** | Target = capture window (or a child under the mouse if capture flag set); else, with a modal window, right-button messages 0x204/0x205 and 0x805B go to it; else hit-test from the desktop. Hover change: old gets 0x8006 (mouse leave), new gets WM_SETCURSOR. The point is converted to client coords and packed into lParam. |
| 0x42EBE0 / 0x42EC20 / 0x42EC30 / 0x42EC40 | SetFocus / GetFocus / HasFocus / SendToFocus | `dword_4EA434`; WM_KILLFOCUS 8 / WM_SETFOCUS 7; keyboard messages are sent to the focus. 48 callers of SetFocus (every menu builder). |
| 0x42ED20 | SendSetText-like | sends 0xC (WM_SETTEXT) — used by option menus 0x405A70, 0x4096D0. |

### 2.3 WView (vtable 0x4AA010) and WDialog (vtable 0x4AA050)

| Addr | Name | What it does |
|---|---|---|
| 0x42F520 | `WView` ctor (id, rect, flags, ?, owner, parent) | +0x7C camera/scroll object, +0x80 byte, +0x84 owner (refcounted) that receives its WM_COMMANDs. Used by Fullscreen View 0x4061B0, war map views (mini-map 0x422C70, mini-map frame 0x423420, calendar 0x426C10, 0x41C010), desktop 0x433700, battle map 0x484A30, 0x4868B0. |
| 0x42F590 | dtor | |
| 0x42F610 / 0x42F630 | Activate / Deactivate | show(5) + task start; task stop + hide. |
| 0x42F650 | WndProc | WM_COMMAND first offered to owner +0x84, then to the base. |
| 0x42F6C0 | GetScroll | from camera +0x7C (0x40E610: camera +0xCC/+0xD0). |
| 0x42F6F0 | SetScroll(x,y) | **Clamps through the camera (0x40E5A0): 0 ≤ x ≤ world w (+0xDC) − view w (+0xE4) − 1, same for y (+0xE0/+0xE8)**, then 0x42E700. Called by war-map scrolling 0x41C1F0/0x41C5E0 and battle map 0x484D00/0x484FB0. |
| 0x42F730 | `WDialog` ctor | WWindow + notifies parent 0x80C5 (dialog opened). Used by WFullScreenMenu 0x40A060, InfoBox, PopupMenu, 0x45F140, 0x49BAA0, 0x4A3E60. |
| 0x42F7C0 | dtor | Focus back to parent, notifies parent 0x80C4 (dialog closed). |
| 0x42F830 | Activate | task start + take focus. |
| 0x42F850 | WndProc | Esc key → WM_COMMAND 0x8123. Command 0x8123 → release capture, hide, **destroy itself**. Command 0x8024 → release capture, hide. 0x8007 = focus next child that has flags 0x10000 (tab stop) + visible, wrapping; 0x8008 = previous (walks +0x70 from the last child). 0x800C → 1 (children use keyboard navigation). |

### 2.4 Controls

Class tree (vtables): WWindow → WControl (0x4AA3D0, ctor 0x433E70) → WGraphicControl (0x4AA390, 0x433B60) →
WGraphicPushButton (0x4AA090) → {glass button 0x4AA110, toggle 0x4AA410 → checkbox 0x4AA150, hot button
0x4AA450, radio 0x4AA190, view button 0x4AA2D0}; WControl → WMenuItem (0x4AA0D0) → WEditField (0x4AA1D0);
WGraphicControl → WScrollbar (0x4AA210).

| Addr | Class / name | What it does |
|---|---|---|
| 0x433E70 | `WControl` ctor | Asks parent 0x800C; +0x7C = 1 when the parent does keyboard navigation (menus) – then **hovering takes the focus** and "highlighted" means "has focus", otherwise highlighted means "is the hover window". |
| 0x433F10 | dtor | |
| 0x433F20 | `WControl::WndProc` | Esc → WM_COMMAND 0x8123 ("Escape from WControl"); Tab → 0x8007, Shift+Tab → 0x8008 (to itself, forwarded to parent); 0x8007/0x8008 forwarded to parent; 0x807C (tool-tip text query) forwarded to parent. |
| 0x433B60 | `WGraphicControl` ctor (id, gtype, label, flags, parent, x, y) | +0x80 sprite (0x40ECF0), size from sprite, +0x84 pressed state. |
| 0x433C80 | dtor | |
| 0x433D00 | Draw | sprite at z+2 with frame from 0x433DB0. |
| 0x433D80 | WndProc | WM_SETCURSOR → cursor 0; else 0x433F20. |
| 0x433DB0 | **FrameForState** | By the sprite's frame count: 1 → 0. 2 → highlighted?1:0. 3 → focused ? (pressed?2:1) : 0. ≥4 → not pressed: highlighted?1:0; pressed: highlighted?2:3. (Answers FRONTEND.md's open "which frame is which" question.) |
| 0x433E50 | SetPressed(state) | +0x84. |
| 0x42F9B0 / 0x42FA20 / 0x42FA70 / 0x42FB40 / 0x42FC20 / 0x42FD00 | `WGraphicPushButton` ctors | Flags 0x10010000 (tab stop + visible). Variants: plain sprite (CD Player 0x407990; research/build lists 0x45BD70, 0x45E670, 0x460F10, 0x461500, 0x462E50), with a text label (+0x8C text object 0x40F6D0, centred 0x40F9F0(6)), with a command parameter +0x88. 0x42FB40/0x42FD00 skip the label when flag 0x8000000 is set (the label is then the tool tip, +0xC). |
| 0x42FDF0 | dtor | |
| 0x42FE70 | Draw | Base sprite then label centred at z+4. |
| 0x42FEF0 | `WGraphicPushButton::WndProc` | Enter/Space → WM_COMMAND(id, +0x88) immediately. Left/Up → 0x8008, Right/Down → 0x8007. Left **or right** button down (0x201/0x204) → pressed + capture. Mouse move (0x200) inside with a button held → pressed; outside while captured → unpressed. Button up (0x202/0x205) inside → "WGraphicPushButton::SendCommand" WM_COMMAND(id, +0x88); release capture. WM_SETCURSOR with keyboard nav → take focus. 0x807C → tool tip (+0xC) when flag 0x8000000. 0x8128 → set label shade level (0x40EC30) when wParam≠0 or the level is 0. |
| 0x430A30 / 0x430AF0 | glass push button ctors (vtable 0x4AA110) | Adds +0x90 glass sprite (50% blend: object flags 4|1). 0x430AF0 builds the menus' buttons (0x40AA10, 0x40AB00), CD Player, 0x45EEA0. |
| 0x430BC0 | dtor | |
| 0x430C40 | Draw | glass with the same state frame (0x433DB0), then base. |
| 0x434050 | `WToggleButton` ctor (vtable 0x4AA410) | +0x90 = bool*; initial pressed = *bool. |
| 0x434110 | dtor | |
| 0x434120 | WndProc | Enter/Space or click toggles +0x84 and sends WM_COMMAND(id, state); a WM_COMMAND from outside writes `*bool`. 0x80D0 → tool-tip anchor (right edge, y − 4). |
| 0x430CD0 | `WCheckBox` ctor (vtable 0x4AA150) | toggle + glass +0x94; label font 0x71. Built by 0x40A320/0x40A480. |
| 0x430D90 / 0x430DA0 | dtor / Draw | |
| 0x430E10 | `WRadioButton` ctor (vtable 0x4AA190) | glass push button. Built by 0x40A630. |
| 0x430E80 / 0x430E90 | dtor / WndProc | Enter/Space/click → pressed + WM_COMMAND; 0x8034 sets/clears pressed by group (wParam = selected id, guess). |
| 0x4333B0 | `WViewButton` ctor (vtable 0x4AA2D0) | War-map mini-map panel buttons (0x423420). |
| 0x433470 / 0x433480 | dtor / WndProc | Like the radio button: click/Enter → pressed + WM_COMMAND; 0x8034 → pressed iff it names this button; 0x807C/0x80D0 tool tip. |
| 0x434300 / 0x434340 | `WHotButton` ctors (vtable 0x4AA450) | Popup-menu items; label font 0x71. |
| 0x434380 / 0x434390 | dtor / WndProc | WM_SETCURSOR (hover) → **sound 0x194**, parent 0x80F1(id). Button up 0x202/0x205 → **sound 0x195**, then press handling. 0x80CF → no tool tip. |
| 0x4335F0 | `WOverlayWindow` ctor (vtable 0x4AA310, flags 0x50000000) | Non-clipped child used for war-map counters (0x42B2F0, 0x426140, 0x426840), research 0x45EC00, puppets 0x496D60. |
| 0x433650 / 0x433660 | dtor / WndProc | Forwards WM_COMMAND to parent. |

**Menu items (plate + glass + text), vtable 0x4AA0D0**

| Addr | Name | What it does |
|---|---|---|
| 0x4301C0 | ctor (plate, glass, text, font, parent, x, y) | +0x80 glass (blend), +0x84 plate (size), +0x88 text object centred. Used for menu headings 0x40A7D0, CD Player 0x407990, edit field. |
| 0x4303A0 | ctor without glass | CD Player, 0x49BAA0 (war/battle map menus). |
| 0x430500 | text-only ctor | Option-row labels 0x40A320/0x40A480/0x40A630/0x40AC80, research 0x45D3F0/0x45DD50. |
| 0x430360 | SetPos wrapper | |
| 0x4306D0 | Draw | glass z, plate z+2, text z+10 at 0x4307D0's offset. |
| 0x4307D0 | TextOffset | From the text object's alignment (+0x44): 1 left (indent 0x18,3 when there is a plate), 2 centre-x, 4 centre-y, 8 right; default centre both. |
| 0x4308A0 | SetTextAlign | 0x40F9F0 on the text. |
| 0x4308C0 | WndProc | WM_SETTEXT updates the text; mouse messages are forwarded to the parent in parent coords; 0x8128 shade level. |

**WEditField (vtable 0x4AA1D0)**

| Addr | Name | What it does |
|---|---|---|
| 0x430FC0 | ctor (plate, glass, text, font, maxLen, …) | +0x8C plate, +0x90 glass, +0x94 caret (GType 0x52 `caret8.spr`), +0x98/+0x9C caret pixel x/y, +0xA0 caret index, +0xA4 max length; flag 0x10000. Used by Edit Campaign 0x40A8C0. |
| 0x4312D0 | Draw | Menu item + caret (only with focus) animated via 0x40EF20(0x33,1), z+0xC. |
| 0x4313F0 | CaretFromX | Click positions the caret between characters. |
| 0x431490 | CaretPixel | Recomputes +0x98. |
| 0x431500 | WndProc | WM_SETCURSOR → cursor 0x11. Click → focus + caret. WM_CHAR: accepts `isalnum` (C locale: ASCII only) and ``space ! " # $ & ' ( ) * . : ; < = > ? ^ _``; rejected or field full → **sound 0x192 (error beep)**. WM_KEYDOWN handled by 0x431730. |
| 0x431730 | KeyDown | Up → 0x8008, Down/Enter → 0x8007, Delete, Backspace, Home, End, Left/Right (Ctrl = by word, using `isspace`). |
| 0x431C30 | InsertChar(c, pos) | Truncates to max length, updates text, sends parent **0x813A** (text changed, lParam = text). |
| 0x431D50 | SetText | Same notification. |
| 0x431DD0 | FrameForState | edit-field variant of 0x433DB0. |

**WScrollbar (vtable 0x4AA210)** — options sliders and the war-map speed slider.

| Addr | Name | What it does |
|---|---|---|
| 0x431E30 | ctor (gtype, x, y, max, parent, marginLo, marginHi) | label "Scrollbar", flags 0x10010000; +0x88 horizontal = sprite w > h; +0x8C position; +0x90 max; +0x94/+0x98 margins; +0x9C int* (none). Calendar 0x426C10: GType 0x97, Human (0,0x1F) max 10 margins **18/31**; Martian (0x30,0x16) margins **8/11**. |
| 0x431EF0 | ctor with int* and background | +0x9C = int* (read initially), +0xA0 optional background sprite (2 frames). Options rows 0x40AC80 pass margins **21/21** (0x15). |
| 0x432000 | dtor | |
| 0x432080 | Draw | Frame = pos × (frames/2 − 1) / max, + frames/2 when highlighted; background frame = highlighted. |
| 0x4321B0 | WndProc | Left/Right arrows ±1 (clamped 0..max); Up/Down → focus prev/next. Mouse down 0x201 → capture; click and drag (0x200 with MK_LBUTTON): `v = clamp(coord − marginLo, 0, len − marginLo − marginHi − 1); pos = (max+1)·v / (len − marginLo − marginHi)` (no rounding). Every change writes `*int` and sends the parent WM_HSCROLL 0x114 (vertical: WM_VSCROLL 0x115) with wParam = pos<<16 | SB_THUMBPOSITION(4), lParam = this. Button up → release capture, parent gets WM_NOTIFY 0x4E (wParam 0x202). |
| 0x4324D0 | SetPos(pos) | Same notification. Caller calendar 0x4278A0. |

### 2.5 WInfoBox (vtable 0x4AA250) — critical-event dialog

| Addr | Name | What it does |
|---|---|---|
| 0x432520 | ctor (text, cmd, text2, cancellable) | WDialog id 0x801B "InfoBox", z 0x65FE, sized to the screen; `dword_4EA44C`++ (open count). Backdrop GType 0x68 (`hu_event.SPR` / `ma_event.SPR`) centred horizontally, 4 px above the bottom; Martian games add GType 0x69 (`ma_scrn.SPR`) at +0xB, −0x5E. Text in font 0x73 word-wrapped to 320 px (0x4161E0), shade `word_4B9428` (21), placed 0x2B (Human) or 0x17 (Martian) below the backdrop top; optional second text, shade `word_4B942C` (22), 16 px lower. Button: WGraphicPushButton GType 0x6A (`hu_buton`/`ma_buton`), font 0x73, caption 0x480 "Press button to continue ESC to cancel" when cancellable, else 0x47F "Press button to continue"; position backdrop + (0x52,0x76) Human / (0x87,0x28) Martian (from the `and 0CBh`/`and 4Eh` selects on IsHuman 0x404BA0). Takes capture and becomes modal; the button takes focus. Callers: war map critical events 0x41E4E0, battle map 0x43A980. |
| 0x432A80 | Draw | backdrop z, overlay z−1, texts z+1. |
| 0x432B30 | WndProc | Enter/Space → WM_COMMAND(cmd); Esc only when cancellable. Command 0x8122 → close, tell parent 0x8122; 0x8123 when cancellable → forward + tell parent 0x8122; other commands → forward to parent then destroy. |

### 2.6 WPopupMenu (vtable 0x4AA290) — right-click command menus

| Addr | Name | What it does |
|---|---|---|
| 0x432C60 | ctor | WDialog, z 0x61B2; 10 columns × 8 rows of item pointers at +0x7C (row stride 0x28). Built by "Warmap menu" 0x41D120 and "Battlemap menu" 0x485DE0. |
| 0x432D70 / 0x432E90 | AddItem(cmd, icon gtype, string id) | Creates a WHotButton (0x434300 / 0x434340); string 0x475 "View map" is replaced by the current sector's name (`dword_4E9B70+0x5C → +0x48 + 2`). Places it in the first free grid cell of its column, resizes the menu. Items seen: 0x476 Cancel (icon 0x1C `RBHcncl`), 0x47A Return to Warmap (0x2C `RBHrturn`), 0x470 Build (0x20 `RBHbuild`), 0x496 Resume building tasks (0x23 `RBHgo`), 0x477 Cancel Move (0x350 `RBHcnclM`), 0x478 Information (0x2A `RBHinfo`); callers 0x429A70, 0x43ADA0, 0x440300, 0x448500, 0x47E1E0. |
| 0x4331C0 | FindCell | |
| 0x4332C0 | MeasureGrid | widest row, total height. |
| 0x433220 | LayoutGrid | left-to-right, or right-to-left when flipped (+0x1BC). |
| 0x432FB0 | ShowAt(x,y) | Centres on the point, clamps 3 px inside the screen; if clamped on the right, flips (+0x1BC) and relayouts; shows, captures, **sound 0x193**. |
| 0x433330 | Highlight(cmd) | Shows only the hovered item's column expanded (sub-items hidden unless their id matches; guess). |
| 0x433090 / 0x4330B0 | Draw / WndProc | Esc → **sound 0x196**, destroy. Click outside → destroy. WM_COMMAND → forward to parent, destroy. 0x80F1 (item hover) → 0x433330. |

### 2.7 WDesktop and tool tips

| Addr | Name | What it does |
|---|---|---|
| 0x4336D0 | `GetDesktop` | Returns `dword_4EA550`; clears `byte_4BACC0`. 15 callers. |
| 0x4336E0 | static init | desktop rect `dword_4EA538` = 0,0,2000,2000. |
| 0x433700 | `WDesktop` ctor (vtable 0x4AA350) | WView, flags 0x10200000 (visible + root); +0x88 cursor id. Called by Win95DeskTop 0x42C6C0. |
| 0x433760 | dtor | |
| 0x433770 / 0x4337B0 / 0x4337F0 / 0x433830 | ini statics | `[Screen]` "Quick Tool Tip Delay" = 15, "Tool Tip Delay" = 70, "Slow Tool Tip Delay" = 150 (ticks), "Tool Tip Enable" = 1. |
| 0x433870 | `WDesktop::Draw` + **tool-tip manager** | Each frame: if enabled and there is a hover window that answers 0x80CF: the time since the mouse last moved (0x4038F0 = tick `dword_4D2558` − `dword_4D14A4`) picks level 1 (> 15), 2 (> 70) or 3 (> 150); it asks 0x807C with that level (falling back to lower levels) for a string; a non-empty answer creates a tool-tip object (0x49D470, 0x2AC bytes) at the point where hovering started (`dword_4EA548/54C`) and installs it with 0x40E470. A mouse button (GetAsyncKeyState 1/2) or hover change removes it. If the tool tip is showing and the window answers again before 70 ticks, it stays. |
| 0x433AB0 | `WDesktop::WndProc` | WM_SETCURSOR → cursor +0x88; keys 0x100–0x102 → focus window (0x42EC40); mouse 0x200–0x205, 0x800A (per-frame mouse position, sent by 0x42D640) and 0x805B (right button held, 0x42D610) → 0x42EA30. |

### 2.8 Fog of war (battle map) — WFogOfWar (vtable 0x4AA4A0)

| Addr | Name | What it does |
|---|---|---|
| 0x4344C0…0x434680 | ini statics, section `[BattleMap]` | "EnableFogOfWar" = 1; "Fog darkness" = 0.7; "Fog return rate" = 5; "Fog update freq" = 15; "Fog smooth index" = 128; "Fog vert dropoff" = 3; "Fog damage delta" = 0; "Fog vert damage radius" = 16. |
| 0x4346C0 | ctor(w, h) | WObject "fog"; +0x84 w, +0x88 h; enabled (+0x8C) = ini && `byte_4BAD38` (cleared by a cheat in 0x401000). +0x90 darkness, +0x94 return rate, +0x98 update freq, +0x9C smooth index, +0xA0 vert dropoff, +0xA4 damage delta, +0xA8 vert damage radius. Two w×h byte maps from the memory manager: "FogMap" (+0x10, data +0x18) and "OldFog" (+0x14, data +0x1C); filled with 0xFF (all visible) when fog is off. 19 circular stamps for radii 5, 10 … 95 (+0x24.., sizes +0x71..). Caller battle-map setup 0x439770. |
| 0x434910 | dtor | "WFogOfWar::~WFogOfWar". |
| 0x4349A0 | BuildStamp(buf, r) | value = 255 inside r − dropoff, linear fall to 0 at r (255·(r−d)/dropoff), 0 outside. |
| 0x434A80 | Reveal(x, y, range) | range capped at 100, ×0.2 → stamp index 1–19; max-blends the stamp into FogMap. Callers vehicles 0x48C570, puppets 0x4964B0. |
| 0x434BD0 | Flush(rect, …) | For every cell whose FogMap differs from OldFog (beyond smooth-index rules) projects the cell through the landscape (`dword_59D940`: grid +0x24/+0x28, scale +0x30, vertex heights +0x64) to a screen rect ± vert damage radius, invalidates it (0x40AED0) and copies new → old. Caller 0x49F4E0 (renderer). |
| 0x434DF0 | Decay | Every non-zero cell −= return rate (≥0). Callers 0x439770, 0x43A980. |

### 2.9 Message queues, WTask, WWorld, WObject

| Addr | Name | What it does |
|---|---|---|
| 0x434E50 / 0x434EA0 | CriticalSection wrapper ctor/dtor | |
| 0x434EC0 / 0x434F00 | `MessageQueue` ctor/dtor | +0x8 critical section, +0x20 head, +0x24 tail. Instances `unk_4D24C0` (game queue, built by 0x402500) and `unk_4DAF28` (display/render queue, 0x40CEC0). |
| 0x435050 | Post(target, msg, wParam, lParam, delayTicks) | Node 0x18 bytes: +0 next, +4 target, +8 due tick (`dword_4D2558` + delay), +0xC msg, +0x10 wParam, +0x14 lParam. |
| 0x434FA0 | Pop(out, target) | First node due (and for target, if given); returns bool. Callers: game thread 0x403680, render 0x403920. |
| 0x435100 | RemoveFor(target) | (0 = clear all). |
| 0x435200 | RemoveFor(target, msg) | |
| 0x4355A0 | `PostGame` | 47 callers (all menus, WGame…). |
| 0x4355D0 | `PostDisplay` | 13 callers (desktop commands 0x8084, 0x803B…). |
| 0x435600 | `FlushTarget` | Both queues. |
| 0x435620 | `RemoveGame(target,msg)` | e.g. stopping a self-reposting timer like 0x812E. |
| 0x4354C0 | `WTask` ctor | +0x18 active flag (0x435560 start / 0x435570 stop); base 0x439300. 21 callers (war map routes 0x4240D0, pods, events…). |
| 0x4354B0 | returns 0 | |
| 0x435310 | `WWorld` ctor(owner, name) | Base of WGame (0x402630), WWarMapWorld (0x417CD0), 0x4460E0 (battle map world, guess) and the research world 0x457FA0. +0x1C owner, +0x20 task list "World tasks" (0x43FF80), +0x40 current sub-world (refcounted), +0x44 paused. |
| 0x4353A0 | dtor | |
| 0x435430 | Tick | If not paused, run the task list. |
| 0x435450 | `SetCurrentWorld(world, param)` | old → vfunc +0x18 (leave), new → vfunc +0x14(param) (enter). Called 29× by WGame (0x402880…0x403F00) to swap front end / war map / battle map; (0,0) drops it. |
| 0x435640 / 0x435690 | `WObject` ctor(name)/dtor (vtable 0x4AA508) | +4 refcount (word), +0xC name (default "Default"). 22 callers. |
| 0x4356A0 / 0x4356B0 | 'WOBJ' / true | |

### 2.10 Sector link data (war map ↔ battle map)

| Addr | Name | What it does |
|---|---|---|
| 0x4356C0 | `LandLinked(from, to)` | `to` in neighbour list `off_4BD260[from]` and `byte_4EAA18[from*32+to] & 7`. Callers AI 0x441AD0/0x441B80/0x441C50. |
| 0x435710 | `BuildLinkKinds` | 32×32 table at 0x4EAA18 (already documented in WARMAP.md; ported as `warmap::RouteLinks`). Caller 0x402630. |
| 0x436050 | `BattleEntryPoint(from, to, moveType, &x, &y)` | If `to` is a neighbour of `from`: a switch on `from*100 + to` (91 cases) gives a land point (x,y) and a sea point (x2,y2) on `from`'s battle map; `moveType` 5 or 6 picks the sea point (guess: boats/hover craft), else the land point; non-neighbours give 0,0. Examples: 1→2 land (1300,50) sea (20,1130); 1→6 and 1→8 sea only (100,2100), (5935,1230). Values up to ~6000 are battle-map world units. Callers: 0x421FE0 (war map, battle set-up), 0x43D150 (sector object: where arriving units are placed, converted with landscape `dword_59D940`). |

## 3. Data structures & globals

**WWindow** (0x7C bytes; offsets are the same for all controls)

| Off | Meaning |
|---|---|
| +0x00 | vtable |
| +0x04 | refcount (word) |
| +0x0C | text / name (WM_SETTEXT; tool-tip text for buttons with flag 0x8000000) |
| +0x18 | task active |
| +0x1C/20/24 | magic 0x87654321, 0x98765432, 0xA9876543 |
| +0x28 | id (command id, low 16 bits) |
| +0x2C/+0x30 | x, y relative to parent |
| +0x34/+0x38 | w, h |
| +0x3C | parent |
| +0x40 | flags: 0x10000 tab stop, 0x100000 black background, 0x200000 root (scroll root `dword_4EA448`), 0x8000000 label is a tool tip, 0x10000000 visible, 0x40000000 overlay (absolute, own list) |
| +0x44 | z depth (word): 0x4650 default, overlay = parent+0x14, normal = previous first child + 2; InfoBox 0x65FE, popup 0x61B2 |
| +0x48/+0x4C | sound position |
| +0x50/+0x54 | absolute screen position |
| +0x58/+0x5C | scroll offset |
| +0x60/+0x64 | first/last normal child |
| +0x68/+0x6C | first/last overlay child |
| +0x70/+0x74 | previous/next sibling |
| +0x78 | background object (GType 0x5C) |

Controls: +0x7C keyboard-nav flag (WControl) / camera (WView); +0x80 sprite; +0x84 pressed/checked; +0x88
command lParam (push button) or text (menu item); +0x8C label; +0x90 glass or bool*; +0x94 checkbox glass.
Edit field +0x8C…+0xA4, scrollbar +0x88…+0xA0, fog +0x84…+0xA8, InfoBox +0x7C…+0x98 (see §2).

**Globals**

| Addr | Meaning |
|---|---|
| 0x4EA428 / 0x4EA430 | mouse capture window / capture-includes-children |
| 0x4EA42C | modal window |
| 0x4EA434 | keyboard focus |
| 0x4EA438 | current cursor id |
| 0x4EA43C / 0x4EA440 | last WM_SETCURSOR point |
| 0x4EA444 | hover window |
| 0x4EA448 | scroll root window |
| 0x4EA44C | InfoBox counter |
| 0x4EA450 | sprintf buffer for "View map" sector name |
| 0x4EA538 | desktop rect (0,0,2000,2000) |
| 0x4EA548 / 0x4EA54C | tool-tip anchor |
| 0x4EA550 | desktop |
| 0x4EA554 | window the tool tip belongs to |
| 0x4BACC4/C8/CC/D0 | tool-tip delays 15/70/150, enable 1 |
| 0x4BAD38 | fog allowed (cheat) |
| 0x4EA558…0x4EA614 | fog ini objects |
| 0x4EA364 | windowed flag (guess) for cursor scaling |
| 0x4EA3A0 / 0x4EA348 | logical screen w/h |
| 0x4EA368 | cached pixel format |
| 0x4EA3E0 | WGame running |
| 0x4EA404 | tick of the last right-button press |
| 0x4D2558 | game tick counter (read for queue due times and tool tips) |
| 0x4D14A4 | tick the mouse last moved |
| 0x4D24C0 / 0x4DAF28 | game / display message queues |
| 0x4BD260 | per-sector neighbour lists (−1 terminated) |
| 0x4EAA18 | link-kind table 32×32 |
| 0x4E9250 / `dy` | listener (screen centre) position passed to every UI sound |
| 0x4E9658 | screen rect |

## 4. Messages, commands, strings, tuning

**Window messages used by the toolkit**

| Id | Meaning |
|---|---|
| 2, 7, 8, 0xC, 0xD | WM_DESTROY, SETFOCUS, KILLFOCUS, SETTEXT, GETTEXT |
| 0x20 | WM_SETCURSOR (hover) |
| 0x4E | WM_NOTIFY from a scrollbar on release |
| 0x100/0x102 | key down / char |
| 0x111 | WM_COMMAND (id low word, param) |
| 0x113 | WM_TIMER (task tick) |
| 0x114/0x115 | slider moved (pos<<16 \| 4) |
| 0x200–0x205, 0x207/0x208 | mouse (both buttons press buttons) |
| 0x215 | capture lost |
| 0x3B9 | MM_MCINOTIFY |
| 0x8006 | mouse leave |
| 0x8007 / 0x8008 | focus next / previous |
| 0x800A | per-frame mouse position |
| 0x800C | "do you use keyboard navigation?" |
| 0x8024 | close dialog (hide only) |
| 0x8034 | radio/view-button group select |
| 0x803B | change display mode |
| 0x803D | load shade-table group |
| 0x8041 | play movie |
| 0x8042 | restore surfaces (guess) |
| 0x8049 | Ctrl+E (no handler) |
| 0x805B | right button held (wParam = ticks held) |
| 0x807C | tool-tip text query (wParam level 1–3, lParam char**) |
| 0x80C4 / 0x80C5 | dialog closed / opened (to parent) |
| 0x80C6 | menu-item text flag (+0x3C) |
| 0x80CF | wants a tool tip? |
| 0x80D0 | tool-tip anchor point |
| 0x80F1 | popup item hovered |
| 0x8084 | game cursor visible |
| 0x801B | InfoBox window id |
| 0x8122 | InfoBox closed |
| 0x8123 | back / Escape |
| 0x8128 | set label shade level |
| 0x813A | edit text changed |

**Sounds:** 0x192 edit-field reject beep; 0x193 popup opens; 0x194 popup item hover; 0x195 popup item
click; 0x196 popup cancelled (all at volume 1.0 through 0x464EE0).

**Strings (TEXT.ojd):** 0x475 View map, 0x476 Cancel, 0x477 Cancel Move, 0x478 Information, 0x47A Return to
Warmap, 0x470 Build, 0x496 Resume building tasks, 0x47F "Press button to continue", 0x480 "Press button to
continue ESC to cancel".

**GTypes:** 0x52 caret8.spr; 0x5C bigBlack.SPR; 0x68 hu_event/ma_event; 0x69 ma_scrn (Martian only); 0x6A
hu_buton/ma_buton; 0x1C/0x20/0x23/0x2A/0x2C/0x350 RBH* popup icons; 0x97 speed slider; fonts 0x71
(e-f-4-w / m-f-4-w), 0x73 (e-f-12-w / m-f-14-w).

**Ini tuning:** `[Screen]` Quick Tool Tip Delay 15, Tool Tip Delay 70, Slow Tool Tip Delay 150, Tool Tip Enable 1.
`[BattleMap]` EnableFogOfWar 1, Fog darkness 0.7, Fog return rate 5, Fog update freq 15, Fog smooth index 128, Fog
vert dropoff 3, Fog damage delta 0, Fog vert damage radius 16.

**Debug strings:** "WGame::Resume()", "WGame::Pause()", "Mouse move outside window %d, %d",
"WGraphicPushButton::SendCommand( %d, %d )", "WScrollbar::WM_MOUSEMOVE", "Escape from WControl", "World tasks",
"WFogOfWar::…".

## 5. Cross-references to other pieces

- **Win95DeskTop** (0x42C460–0x42D9xx, previous piece): builds the desktop (0x42C6C0 → 0x433700), changes display
  mode (0x42C9B0), sends 0x800A/0x805B (0x42D610/0x42D640), recording/playback.
- **WGame** 0x402630/0x402B80: creates queues (0x402500 → 0x434EC0), WWorld (0x435310), swaps worlds (0x435450),
  posts every menu command through 0x4355A0, flushes with 0x435600; game/render threads pop queues (0x403680,
  0x403920 → 0x434FA0).
- **Menus** 0x4052B0–0x40AC80 (FRONTEND.md): every builder uses WDialog (0x42F730), the control ctors, Align
  (0x42F020), SetFocus (0x42EBE0), 0x8128.
- **War map**: mini-map views (0x422C70/0x423420 → WView, view buttons 0x4333B0), calendar (0x426C10 → speed
  scrollbar 0x431E30, 0x4324D0), critical-event InfoBox (0x41E4E0 → 0x432520), right-click menu (0x41D120,
  0x429A70 → popup), scrolling (0x41C1F0/0x41C5E0 → 0x42F6F0), counters (0x42B2F0 → 0x4335F0), route arrival
  battle entry (0x421FE0 → 0x436050).
- **Battle map**: 0x484A30 (WView), 0x485DE0 (popup), 0x43A980 (InfoBox, fog decay), 0x439770 (fog ctor),
  vehicles 0x48C570 / puppets 0x4964B0 (fog reveal), renderer 0x49F4E0 (fog flush), 0x43D150 (entry point).
- **Research / build lists** 0x45BD70–0x462EB0: push buttons, overlays, z-order.
- **Tool-tip object** 0x49D470 (not in this piece) draws the tip.
- **AI** 0x441AD0… uses 0x4356C0.
- **Sound** 0x464EE0; **DeeDraw** 0x44AD20/0x44BD90/0x44CFA0/0x44DCA0; **text objects** 0x40F6D0/0x40F9F0/
  0x40FA40/0x40FBA0/0x40FBF0; **sprite objects** 0x40ECF0/0x40EC50/0x40F000; dirty rects 0x40AED0/0x40AEB0.
- Next piece starts at 0x437380 (sector waypoint, already used by WARMAP.md).

## 6. Missing in WoWEngine / candidate tasks (prioritised)

WoWEngine (`src/frontend/Screen.*`, `Menu.*`, `src/warmap/WarMap.*`) has a flat list-of-buttons model, not
the window tree. That is fine as long as behaviour matches; the items below are behaviour differences or gaps.

**High (affects existing screens)**

1. **Button frame states** (0x433DB0): use the exact table for 2/3/≥4-frame sprites, with "highlighted" =
   focus in menus (keyboard-nav parents) and = hover elsewhere. In particular the war-map view buttons (4
   frames, hover-based): the selected button shows **frame 3** when not under the mouse and 2 under it (the
   engine always draws 2). Update the FRONTEND.md and WARMAP.md "not confirmed" notes.
2. **Slider maths** (0x4321B0): `pos = (max+1)·clamp(x − lo, 0, len−lo−hi−1) / (len−lo−hi)`, no rounding.
   Options sliders lo = hi = **21** (engine: `KnobInset = 22` plus rounding, `Screen.cpp:12,130`). War-map speed
   slider: Human margins **18/31**, Martian **8/11** (engine uses the full sprite width, `WarMap.cpp:448`).
   Only acts on left button down/drag; a change sends 0x114 → calendar 0x427932.
3. **Edit field** (0x431500/0x431730/0x431C30): caret (GType 0x52, animated, only with focus), click-to-position,
   Left/Right (Ctrl = word), Home/End/Delete, insert at caret, character filter (ASCII alnum + the listed
   punctuation; Latin-1 accents are rejected), reject beep sound 0x192, Up → previous control, Down/Enter → next.
   Engine: append/backspace only, accepts Latin-1.
4. **Keyboard focus details** (0x42F850, 0x433F20, 0x42FEF0): Shift+Tab → previous (missing), focus wraps;
   Enter/Space on a push button fires immediately; right mouse button also presses push buttons; moving out of
   a held button un-presses it and back in re-presses it.
5. **War-map scroll clamp** (0x42F6F0 → 0x40E5A0): clamp the view origin to `[0, world − view − 1]`.

**Medium (needed for the war map)**

6. **InfoBox** for critical events (0x432520, 0x432B30): layout and assets per side, strings 0x47F/0x480, modal,
   Esc only when cancellable, notify 0x8122. Feeds the critical-event dialog queue in 0x41E4E0 (WARMAP.md "not
   done").
7. **Right-click popup menu** (0x432C60–0x4333AB, 0x434300/0x434390): grid of RBH* icon buttons, "View map" →
   sector name, clamp/flip at the screen edge, sounds 0x193–0x196, closes on outside click/Esc. War map 0x41D120
   (Cancel…), sector menu 0x429A70 (Build, Resume building tasks, Cancel Move), object 0x440300 (Information).
8. **Tool tips** (0x433870, delays 15/70/150 ticks, `[Screen]` keys; 0x807C levels; tip object 0x49D470): needed for
   the CD Player transport buttons (strings 0x4B9–0x4BD, FRONTEND "Tooltips" to-do) and the war-map panel buttons
   (0x47B–0x47E), replacing the engine's provisional bottom-left text strip.
9. **Delayed message queue semantics** (0x435050/0x434FA0): commands posted with a tick delay and due-time ordering
   per target; removal by (target,msg). Useful when porting WGame's self-reposting timers (0x812A, 0x812E,
   0x8134) exactly.
10. **WWorld switching** (0x435450): enter/leave hooks and pause flag for front end ↔ war map ↔ battle map.

**Low / later (battle map)**

11. **Fog of war** (0x4346C0–0x434DF0): two byte maps, 19 stamps, reveal/decay/flush, `[BattleMap]` tuning,
    cheat flag 0x4BAD38.
12. **Battle-map entry points** (0x436050): extract the 91-entry table (land and sea point per directed link) into
    `WarMapData.h` for placing reinforcements when a battle starts.
13. **0x805B / 0x800A** mouse messages (right-button-held ticks, per-frame position) for battle-map right-drag and
    edge scrolling.
14. Display-mode change via 0x803B and cursor visibility 0x8084 (engine draws at 640×480 only; FRONTEND "Display
    Settings").
15. Minor: label shade message 0x8128 (menu text colours), Ctrl+E (no handler; ignore), MessageBox on fatal
    DeeDraw errors (0x42E750).
