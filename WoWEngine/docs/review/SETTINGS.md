# The settings audit (update 244)

The survey's list of every registry / INI setting in the exe (docs/survey/part14.md section 4: 227 rows in 22
sections) against the engine's source: each key looked for by name. 175 are read. The other 52 are below, each
with why it isn't; the "never read" ones were checked in the listing (only their constructor and the data
definition refer to them). `InsulatedHighEnergyResistantArmourMultiplier` (cut short in the survey's table) is
read, by its full name (core/Research.cpp).

So every setting the original reads that changes how the game plays is read by the engine; what's left is the
platform, the network game, the developers' switches and settings the game itself ignores.

## Platform (the engine has its own window, timing and memory; nothing a player sees)

| Section | Keys | Why |
|---|---|---|
| (root) | Thread Enable / Timer Enable / Sleeper Enable | the game loop's Windows threading and timers |
| (root) | Use INI | registry or WOW.INI; the engine reads WoWRevived.ini |
| (root) | Full Screen | the engine's own windowed / full-screen option (MODERN.md) |
| Screen | Use DDraw surfaces, BLIT instead of FLIP, Damage map VRAM, Use Reaper, Support screen BPP | DirectDraw |
| Screen | Top left, Support screen size, AllowResize, FixedAspect | the window (the engine's resolution options) |
| System | Game max time (ms), Game min time (ms) | frame time bounds; the engine runs a fixed 30 updates a second (Game Frequency, which it reads) |
| System | Game Memory, Renderer Memory, Game Memory Handles, Renderer Memory Handles | the game's memory pools |
| Debug | Show Mem Dump On Alloc Failure | memory |
| Sound | Unload Limit | looked at in update 181: nothing a player hears changes (PLAN 4.3). Search Libraries First is read since update 247 (loose sound files) |

## Written, not read

| Section | Keys | Why |
|---|---|---|
| Stats | IRQ mem used, Non IRQ mem used, Routes left, WM Groups, Holding Groups, BM Groups, Scenery objects left, Puppets objects left | counters the game writes for its developers |

## Network (with the network game, PLAN A9)

| Section | Keys | Why |
|---|---|---|
| Network | I Am Human, Messages Guaranteed, Session Name |  |

## Developer switches (debug drawing and logging)

| Section | Keys | Why |
|---|---|---|
| Debug | Enemy Visible | dword_54604C: the computer player's war map moves snap its counters to the sector waypoint, so its moves can be watched (part07); not a player's setting |
| Debug | Show Accelerations, Show Radii, Show Arrival, Show Orders | debug drawing on the battle map |
| Debug | Log Debug Output, Ignore fresher files | debug logging; the file loader's date check |

## Made and never read by the game (no reader of the value in the listing)

| Section | Keys | Why |
|---|---|---|
| Tweak | AI Oracle (0x442E51), AI Hours Per Turn (0x442E91), IMap Iteration Count (0x439741) | the int / bool setting objects keep their value at +0x14 (as startTurnRate's object 0x59E1A8 at dword_59E1BC); nothing reads it |
| Vehicles | stopTurnRate (0x48FBB1) | the same: no reader of dword_59E18C |
| Fun | Veh Speed (0x4916B1), Veh Rot Speed (0x4916F1) | the same |
| Debug | Arrow scenery load fix (0x47CD01) | the same |
| Colours | Credits Heading (17), Credits Detail (6) | the bound variables (unk_4B943C / unk_4B9440) are only ever handed to the setting |
| Options | Show simple tooltips | the bound variable (unk_4B8818) is only ever handed to the setting |
