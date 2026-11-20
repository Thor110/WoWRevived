# The unit table

Every object type, from vehicles to buildings to scenery, has a 0xB0-byte entry in the unit table at 0x51A0F0,
filled from OBJ.ojd by the loader at 0x437750 (`ObjOjd::LoadUnits`, `ObjOjd::LoadBattleTables`; `wowdump --units`
and `--tables` print them). OBJ.ojd is a stream of records: 0xFF, a u16 line number, a s16 type, then its payload.

## Before the first record

The table is cleared (0xAD6C dwords), then every entry (0x4377A3) gets: faction (+0x0C) 2, battle units (+0x58) 1,
scale (+0xA8) 1.0, +0x98 0x250, +0xA0 0, +0x40 0x10, armour (+0x4A) 100, +0x43 1, +0x46 0, +0x50 0, +0xA1 0xC0 and
+0x92 90. The loader keeps a running scale (1.0 at first), the current graphics id (record 0), the class for new
types (record 17) and the current type (record 15).

## Records

| Rec | Payload | Effect | Engine |
|---|---|---|---|
| 0 | s16 | the current graphics id (for records 6, 10, 11 and the graphics table) | |
| 6 | f32 a, f32 b, u16 c, u16 d | 0x4F0530 + id x 12: {c, d, a, b} (read by the battle map, 0x4802E0) | `BattleTables::six` |
| 10 | 17 x s16 | 0x4F6E38 + id x 0x44, as ints | `ten` |
| 11 | s16, f32 | 0x4F04C0[id], 0x4F0510[id] | `eleven` |
| 12 | 8 x u16 | 0x4F2F10 + type x 16 (read by 0x494CB0) | `twelve` |
| 13 | f32, 3 x s16 | 0x4EC598 + type x 16: {f, ints} (read by 0x48DDC0) | `thirteen` |
| 15 | s16 type | the current type; one whose +0x00 is still 0 gets +0x00 = itself (its base type), +0x10 = the record 17 class and +0x90 = 20 | `baseType`, `category` |
| 16 | u16 len, data | +0x30-0x33 = the four bytes before the data's last (a NUL): the end of its WSelectableObject name ("Post", "ence"...) | `code` |
| 17 | s16 | the class the next new types get: 1 vehicle, 2 building, 3 resource, 4 and up scenery | |
| 18 | s16 template | the whole 0xB0 bytes copied from the template, base type included, then the parts (+0x38, count +0x42) and builders (+0x78-0x80) cleared; the emitters (+0x3C) stay shared | |
| 19 | s16 a, s16 b | +0x88 a (the type it gives way to, 0x45F900), +0x8C b | `upgrade` |
| 20 | s16 | +0x88, +0x8C = 0 | `upgrade` |
| 21 | s16 | +0x14 its name (TEXT.ojd) | `nameText` |
| 22 | s16 | +0x18 its information page (0x4403A0) | `infoPage` |
| 23 | s16 | +0x2C its war map counter frame | `counterFrame` |
| 24 | s16 | +0x28 | `field28` |
| 25 | f32 | +0xA8 and the running scale | `scale` |
| 26 | s16 | +0x0C faction: 0 Human, 1 Martian, 2 neither | `faction` |
| 27 | 4 x s16 | a box for the next part | |
| 28 | s16 a, u16 b, c, d, f32 f | a "Part____" (0x1C bytes) put in front of +0x38: {next, a, f x scale, b, c, d, the box or (-1, 0, 0, 0)}, +0x42 one more | `parts` |
| 29 | s16 a, u16 b, c, 4 x f32 | an "Emitter" (0x1C bytes) put in front of +0x3C | `emitters` |
| 30 | u16 hp, u16 b, u16 c | +0x44 and +0x46 = hp x scale (truncated, a word), +0x40 = b x scale, +0x41 = c x scale (bytes) | `hitPoints`, `size40`, `size41` |
| 31 | u16 | +0x46 = it x scale | `currentHitPoints` |
| 32 | u16 | +0x48 = it x [Tweak] "VisRadMul" (1.5) | `Vision()` |
| 33 | s16 | +0x50 | `field50` |
| 34 | s16 | +0x4A armour = it x the armour modifier for the type's faction so far | `Armour()` |
| 35 | f32 | +0x4A x it | `Armour()` |
| 36 | u16 | +0x4E move class | `moveClass` |
| 37 | 3 x u16 | +0x52, +0x54 war map speed, +0x56 | |
| 38 | 2 x f32 | +0x54 and +0x56 x them | |
| 39 | s16 days | +0x08 research hours = days x 24 x the research modifier | RESEARCH.md |
| 40-42 | s16 | +0x1C, +0x20, +0x24: research name, "researching" and "researched" texts | |
| 43 | f32 | +0x5C manufacture days x the manufacture modifier | `BuildTable()` |
| 44 | f32 | +0x5C x it (not in OBJ.ojd) | `buildMul` |
| 45 / 46 | 3 x f32 / f32 | +0x60-0x68 upkeep, scaled | `upkeep` |
| 47 / 48 | 3 x f32 / f32 | +0x6C-0x74 work use, scaled | `activity` |
| 49-51 | 1-3 x s16 | +0x78-0x80 its builders | `builders` |
| 52 | u16 | +0x90 how many a sector may hold (0x43C7F0) | `maxPerSector` |
| 53 | u16 | +0x92 | `field92` |
| 54 | s16 | +0x94 the building it needs in the sector (0x442420) | `prerequisite` |
| 55 | u16 | +0x58 battle units | `battleUnits` |
| 56 | u8 | +0x43 | `field43` |
| 57 | u16 | +0x4C (not in OBJ.ojd) | `field4C` |
| 58 / 59 | s16 | +0xA2 / +0xA4 | `fieldA2`, `fieldA4` |
| 60 | s16 | +0x98 | `field98` |
| 61 | u16 | +0xA0 = it - 1 (a byte): its selection voice class (0x439000) | `fieldA0` |
| 62 | u8 | +0xA1 | `fieldA1` |
| 63 / 64 | s16 page, 3 x s16 | research page info (0x5888C0) | RESEARCH.md |

Records 1-5, 7, 9 and 14 belong to the graphics table, the lighting and the route arrows; 8 to the sectors.

## Modifiers

Research (record 39), manufacture (43) and armour (34) take a modifier by the type's faction as it stands when the
record comes, and the side being played (byte_4B84C4): faction 0 takes "HumanFriend ..." in a Human game and
"HumanEnemy ..." in a Martian one; faction 1 "MartianEnemy ..." and "MartianFriend ..."; faction 2 1.0 for
research and manufacture, `[Tweak] "Scenery Armour Modifier"` (1.2) for armour. The armour modifiers default to 1.8
(0x437450-0x437510), the manufacture ones to 1.0 / 0.85, the research ones to 0.75. Values are truncated as the
game does it, on the x87's wider product: a command building's 10000 armour comes out at 17999, as its saved
battle data has it.

Fields still unnamed are kept by their offset (`field28`, `field92`...); the battle map will say what they are.
