#include "battlemap/BattleMap.h"
#include "frontend/PopupRows.h"
#include "platform/Sound.h"
#include "frontend/Canvas.h"
#include "frontend/Resources.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace wow
{
    namespace
    {
        // 0x43FF00 (as in Combat.cpp)
        int Octagonal(int x0, int y0, int x1, int y1)
        {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        }

        std::vector<uint8_t> Need(Resources& res, const std::string& path)
        {
            auto data = res.LoadFile(path);
            if (!data) throw std::runtime_error("battle map: can't load " + path);
            return std::move(*data);
        }
    }

    BattleMap::BattleMap(Resources& res, const Setup& s) : Screen(Id, res, 0, 0x2F0), setup(s)
    {
        // WLandscape::CreateTerrain (0x47B900): maps\<name>, its extension swapped for .atm
        std::string base = s.mapName;
        if (auto dot = base.rfind('.'); dot != std::string::npos) base.resize(dot);
        Terrain t = Terrain::Load(Need(res, "maps\\" + base + ".cls"), Need(res, "maps\\" + base + ".atm"));
        land = Landscape::Create(t);
        tiles = TerrainTiles::Load(t);
        // the sector's Damage Layer (0x439870): with none yet, one the landscape's size (+0x24 x +0x28, 0x404F50),
        // all 0; then (0x405020) unpacked and every byte less 1000 / [BattleMap] "Damage reduction divisor" (500:
        // 2), at least 0, as the scorch. byte_4D25A0, the shym cheat, clears it first, once.
        {
            std::vector<uint8_t> d = s.damage;
            if (s.clearDamage) std::fill(d.begin(), d.end(), uint8_t(0));
            if (s.damageWidth != land.Width() || s.damageHeight != land.Height() || d.size() != size_t(land.Width()) * size_t(land.Height()))
                d.assign(size_t(land.Width()) * size_t(land.Height()), 0);
            const int fade = s.damageDivisor ? 1000 / s.damageDivisor : 0;
            for (uint8_t& b : d) b = uint8_t(std::max(0, int(b) - fade));
            land.SetScorched(std::move(d));
        }

        // The hour's light (0x47AF10 -> 0x40B030) and the shade tables (0x40B570, 0x40B6C0): the first set's letter
        // names them, L and N becoming X and Y in a Martian game
        static const float noLight[4] = { 0.9f, 0.1f, 0.75f, 0.0f };
        int angle = 90;
        const float* set = noLight;
        char letter = 'M';
        if (s.dayTime)
        {
            angle = s.dayTime->angle;
            set = s.dayTime->sets[0].f;
            if (!s.dayTime->sets[0].name.empty()) letter = s.dayTime->sets[0].name[0];
        }
        if (!s.human && letter == 'L') letter = 'X';
        if (!s.human && letter == 'N') letter = 'Y';
        std::string tail = std::to_string(s.shadeClass) + letter;
        levels = Need(res, "dat\\LANDR" + tail + ".int");
        table = ShadeTable::LoadShh(Need(res, "dat\\LANDR" + tail + ".SHH"));
        sun = Landscape::SunFor(angle, set);
        sun.shade = Landscape::ShadeLevel(levels, set[2], 0.0f);
        land.LightTheMap(sun);
        dither = DitherTables::Build(Need(res, "dat\\dith.dat"));
        // the bm group's tables that go with the hour (0x40C08D): buildings, vehicles and nature by side
        const std::string hour(1, letter);
        const std::pair<int, std::string> slots[] = {
            { 1, "BMHB" + hour }, { 2, "BMHV" + hour }, { 3, "BMMV" + hour }, { 4, "BMMB" + hour },
            { 10, "BMR" + std::to_string(s.shadeClass) + "NA" + hour } };
        for (const auto& [slot, name] : slots)
            if (auto data = res.LoadFile("dat\\" + name + ".SHH")) slotTables[slot] = ShadeTable::LoadShh(*data);
        // slot 20 is the ground's own LANDR table (the bm group fills it too): the sprites drawn through it, the water's
        // twinkles (twink1-3.spr) among them, were skipped without it
        slotTables[20] = table;
        SetUpRouteFinder();     // 0x47AF10, with the landscape, before anything is placed (the bridges add nodes)
        // a vehicle's phase (+0x146) is drawn as the object is made (its constructor, 0x490943), which is on the war
        // map, long before the battle's scenery starts and its tree circles seed rand() again (0x48F42A): so the
        // draws are made here, ahead of the scenery, and don't come between the circles' seeds and the deployment's
        phases.clear();
        if (s.units)
            for (const BattleGroup& g : s.groups)
            {
                auto d = s.units->find(g.type);
                const bool vehicle = d != s.units->end() && d->second.category == 1;
                for (size_t i = 0; i < g.places.size(); i++) phases.push_back(vehicle ? int16_t(random::Rand()) : int16_t(0));
            }
        if (s.loadScenery) LoadScenery(sun, letter);
        // the units with saved places start, then the pod lands (0x439ADE-0x439B20: made, added and started before
        // the six deployments, 0x439B24, so the Martians deploy round it), in Deploy
        Deploy();
        SetUpReinforcements();
        // the fog (0x4346C0 at EnterBattleMap +0x1A8, its first decay at +0x625)
        fog.enabled = s.fogEnabled;
        fog.darkness = s.fogDarkness;
        fog.returnRate = s.fogReturnRate;
        fog.updateFreq = s.fogUpdateFreq;
        fog.smooth = std::max(1, s.fogSmooth);
        fog.dropoff = std::max(1, s.fogDropoff);
        fog.damageDelta = s.fogDamageDelta;
        fog.damageRadius = s.fogDamageRadius;
        FogStart();
        // the objects the deployment left where they were saved start now, in their order
        for (size_t i = 0; i < units.size(); i++) StartUnit(i);
        // each object's start (0x495740) ends, for the player's (+0x24 0), in vt+0x100: the fog lifted round it at
        // once, not at the first "Fog update freq" tick (which left the map dark for its first moments); and the
        // other side's hidden or shown by it
        if (fog.enabled)
        {
            FogRevealOwn();
            for (Unit& u : units)
                if (!u.players) u.fogged = !(FogLevel(u) > fog.darkness);
        }
        // the first decay last (0x439D95, after the deployment's reveals), so the revealed start at 255 less the
        // return rate
        FogDecay();
        SetUpGrounds();
        // the group's stance (+0x54, 0x4802E0) is its members' +0xD8 averaged (ftol(sum + 0.5) / n), but the game makes
        // the groups (0x4216E0, from 0x439A8D) before deployment starts the members (0x4222C0's vt+0x14 = 0x495740,
        // which sets +0xD8 from record 56), and its pool allocator (0x440740) reuses freed blocks uncleared, so what it
        // averages is whatever was left there. *Provisional*: Retaliate (1), the usual default.
        groupStance.assign(setup.groups.size(), 1);
        // the groups' targets, modes and engagement ranges (0x4802E0: +0x4E, from 100000 the least of each member's
        // sight (+0x48) and its vt+0xB0, its guns' longest record 12 +0 range, or 10000 with none (0x495850))
        groupTargets.assign(setup.groups.size(), GroupTarget{});
        groupMode.assign(setup.groups.size(), 0);
        groupRange.assign(setup.groups.size(), 100000);
        groupTicks.assign(setup.groups.size(), 0);
        groupInterval.assign(setup.groups.size(), 90);
        for (size_t g = 0; g < setup.groups.size(); g++) RecomputeGroup(int(g));
        groupHomeX.assign(setup.groups.size(), 0);
        groupHomeY.assign(setup.groups.size(), 0);
        groupBusy.assign(setup.groups.size(), false);
        groupHalted.assign(setup.groups.size(), false);
        groupOrdered.assign(setup.groups.size(), false);
        groupArrived.assign(setup.groups.size(), 0);
        groupSelfMove.assign(setup.groups.size(), false);
        AiStart();     // the battle begun, told to the computer player (0x8075 -> 0x445210)
        RekeyViews();

        // The start view (0x439B7E): on the player's HQ (the player's constructions list, HCON, searched for type
        // 278 in London or the Command Post 58 elsewhere in a Human game, 830 in sector 3 or 120 elsewhere in a Martian one), the
        // middle of its units' extent (0x42A2A0 / 0x42A340: (max - min) / 2 + min each way); else on the player's
        // deployment centre (dword_4F742C / 4F7434) unless both are 0; else on the map's middle; projected (with the
        // height there), less half the screen
        int cx = 0, cy = 0;
        const int hq = s.human ? deploy::HumanHq(s.sector) : deploy::MartianHq(s.sector);
        bool found = false;
        size_t at = 0;
        for (const BattleGroup& g : s.groups)
        {
            const size_t n = g.places.size();
            if (!found && g.list == 2 && g.type == hq && n)
            {
                int minX = 1000000, maxX = -1000000, minY = 1000000, maxY = -1000000;
                for (size_t i = at; i < at + n && i < units.size(); i++)
                {
                    if (float(minX) > units[i].x) minX = int(units[i].x);
                    if (units[i].x > float(maxX)) maxX = int(units[i].x);
                    if (float(minY) > units[i].y) minY = int(units[i].y);
                    if (units[i].y > float(maxY)) maxY = int(units[i].y);
                }
                cx = (maxX - minX) / 2 + minX;
                cy = (maxY - minY) / 2 + minY;
                found = true;
            }
            at += n;
        }
        if (!found)
        {
            cx = startX;
            cy = startY;
            if (cx + cy == 0)
            {
                cx = land.WorldWidth() / 2;
                cy = int((int64_t(land.Height()) * land.Cell()) >> 16) / 2;
            }
        }
        SetUpMinimap();
        ScrollTo(cx - 320, land.ProjectY(cy, land.HeightAt(cx, cy)) - 240);
    }

    // 0x4868B0: the picture (maps\hmin<sector>.spr, mmin in a Martian game) at the bottom right of the 640 x 480 view,
    // [Screen] "Scroll Border" (4) in; the view's rectangle its size x 640 / the world's width and x 480 / its height
    // (+0x1C, +0x20); drawn through the bm group's slot 21 ({S}MINIB) at level 0. The colours are system colours
    // (0x40B120 -> 0x40B3B0(palette, r, g, b), 6-bit each; the pushes are b, g, r): the rectangle (dword_4DAF9C)
    // 0, 0, 47 (Human) or 12, 42, 4 (Martian); a dot (0x480070) the side's vehicle colour (4DAFA0: 0, 63, 0 /
    // 63, 0, 0) for a category 1 object of the player's, its other colour (4DAFA4: 0, 63, 32 / 63, 32, 0) for the
    // player's others, and the other side's (4DAFA8: 63, 0, 0 / 0, 63, 0). (Update 143: the Human game's had red
    // and blue the wrong way round; the original's view rectangle is blue, seen in the owner's screenshot.)
    // In an 8-bit display these are the palette's nearest entries (0x40B42F), not the colours themselves.
    void BattleMap::SetUpMinimap()
    {
        char name[32];
        std::snprintf(name, sizeof(name), setup.human ? "maps\\hmin%02d.spr" : "maps\\mmin%02d.spr", setup.sector);
        miniPicture = res.GetFileImage(name);
        if (!miniPicture) return;
        miniW = miniPicture->width;
        miniH = miniPicture->height;
        const int border = 4;
        miniX = 640 - miniW - border;
        miniY = 480 - miniH - border;
        rectW = miniW * 640 / std::max(1, land.WorldWidth());
        rectH = miniH * 480 / std::max(1, land.WorldHeight());
        auto six = [](int r, int g, int b) {
            auto c = [](int v) { return uint32_t((v * 255 + 31) / 63); };
            return c(r) << 16 | c(g) << 8 | c(b);
        };
        if (setup.human)
        {
            rectColour = six(0, 0, 0x2F);
            boxInner = six(0, 0, 0x28);
            vehicleColour = six(0, 0x3F, 0);
            otherColour = six(0, 0x3F, 0x20);
            enemyColour = six(0x3F, 0, 0);
        }
        else
        {
            rectColour = six(0x0C, 0x2A, 4);
            boxInner = six(5, 0x27, 4);
            vehicleColour = six(0x3F, 0, 0);
            otherColour = six(0x3F, 0x20, 0);
            enemyColour = six(0, 0x3F, 0);
        }
    }

    // 0x486CB0 every 16 frames, and Tab showing it: the dots made again from the battle world's objects (0x8077: x x
    // the picture's width / the world's width, y x its height / the rows' extent; at most 1024). *Not yet*: the dots
    // of type 987 (0x487380) and types 694 / 695's own colours (0x48C870); every unit shows, fog or not.
    void BattleMap::MakeDots()
    {
        dots.clear();
        if (!miniPicture || !setup.units) return;
        const int W = std::max(1, land.WorldWidth());
        const int H = std::max(1, int((int64_t(land.Height()) * land.Cell()) >> 16));
        for (const Unit& u : units)
        {
            if (dots.size() >= 0x400) break;
            auto d = setup.units->find(u.type);
            const bool vehicle = d != setup.units->end() && d->second.category == 1;
            const uint32_t colour = !u.players ? enemyColour : vehicle ? vehicleColour : otherColour;
            const int x = int(int16_t(miniW * int(u.x) / W)), y = int(int16_t(miniH * int(u.y) / H));
            // 0x4A1D80 keeps the dot as x, y and a colour word, and builds the word as the 16-bit colour OR its
            // own low byte moved up (mov dh, al; or edx, eax). A colour with nothing in its low byte is unchanged
            // (the Martian game's red and orange); the Human game's green 0x07E0 becomes 0xE7E0, the yellow the
            // original's Human dots are (#E7FF00 in the owner's screenshots).
            auto stored = [](uint32_t rgb) {
                const uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
                uint32_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                c = (c | ((c & 0xFF) << 8)) & 0xFFFF;
                const uint32_t r5 = c >> 11, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
                return ((r5 << 3 | r5 >> 2) << 16) | ((g6 << 2 | g6 >> 4) << 8) | (b5 << 3 | b5 >> 2);
            };
            dots.push_back({ x & 0xFF, y & 0xFF, stored(colour) });
        }
    }

    // An object's screen point (+0x2C / +0x30, the view's coordinates; 0x47F480 -> 0x4406A0): x, and y projected at
    // its height (+0x80: the ground plus its lift) less the type's +0x41 byte, then less the scroll. The brackets,
    // the power bar, the click and the box all use it, so on a walker they sit up at its body, not its feet.
    bool BattleMap::UnitScreen(const Unit& u, int& sx, int& sy) const
    {
        const int ix = int(u.x), iy = int(u.y);
        int raise = 0;
        if (setup.units)
            if (auto t = setup.units->find(u.type); t != setup.units->end()) raise = t->second.size41;
        sx = ix - scrollX;
        sy = land.ProjectY(iy, land.HeightAt(ix, iy) + int(u.drive.lift)) - raise - scrollY;
        return true;
    }

    // The hotspot's half size: the type's +0x40 byte (0x48A660 makes it 2 x that square)
    int BattleMap::Selectable(const Unit& u) const
    {
        if (!setup.units) return 0;
        auto d = setup.units->find(u.type);
        return d == setup.units->end() ? 0 : d->second.size40;
    }

    // 0x485930 / 0x497770, the left button let go in cursor mode 0:
    //  - with a box (0x49C350): every object that counts (0x48A4C0 / 0x48A530: the player's, its screen point inside
    //    the box, a vehicle or with +0x3C set) is a hit; with at least one (world +0x114), the selection is cleared
    //    (packet 4) and each hit's group selected (0x8088 to +0x48)
    //  - else on an object's hotspot (0x497680): the player's, not selected yet, and with Ctrl only a vehicle: the
    //    selection is cleared and its group selected
    // *Provisional*: +0x3C isn't read (only vehicles count for the box); the hotspot is taken as centred on the screen
    // point, the frontmost (largest y) when they overlap.
    void BattleMap::ReleaseLeft(int x, int y)
    {
        if (!setup.units) return;
        auto vehicle = [&](const Unit& u) {
            auto d = setup.units->find(u.type);
            return d != setup.units->end() && d->second.category == 1;
        };
        if (boxActive)
        {
            boxActive = false;
            const int l = std::min(dragX0, dragX1), r = std::max(dragX0, dragX1);
            const int t = std::min(dragY0, dragY1), b = std::max(dragY0, dragY1);
            std::vector<int> hits;
            for (const Unit& u : units)
            {
                if (!u.players || !vehicle(u)) continue;
                int sx, sy;
                UnitScreen(u, sx, sy);
                if (sx >= l && sx < r && sy >= t && sy < b && std::find(hits.begin(), hits.end(), u.ref) == hits.end()) hits.push_back(u.ref);
            }
            if (hits.empty()) return;
            // each object's war map object selected (0x8088 -> 0x439000): their units one group
            SelectRefs(hits);
            for (size_t g = 0; g < selected.size(); g++) if (selected[g]) { SelectionSpeech(int(g)); break; }
            return;
        }
        const Unit* best = HotspotAt(x, y);
        // in mode 1 a reinforcement point's marker is an object too: an attack on it goes through it (0x481F70)
        if (!best && Mode() == 1)
            if (const int k = MarkerAt(x, y); k >= 0)
            {
                for (size_t g = 0; g < selected.size(); g++)
                    if (selected[g]) RetreatToMarker(int(g), size_t(k));
                return;
            }
        if (!best)
        {
            // cursor mode 1 on the ground (0x485A53): the point the mouse looks at (0x47B3F0), packet 0x0E to the
            // selection, or with Ctrl force fire there (packet 0x13); mode 1 from the popup's Force Fire (+0x1DC,
            // 0x481CA0) makes the 0x0E force fire too
            if (Mode() == 1)
            {
                const int wx = x + scrollX;
                const int wy = land.ScreenYToWorldY(wx, y + scrollY);
                const bool force = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
                // (0x485C5F-0x485CD5: sent with 0x47B3F0's y even when it found none, 0)
                if (force) ForceFire(wx, wy);
                else OrderMove(wx, wy);     // (a group in mode 1 fires there instead, 0x481CA0)
            }
            return;
        }
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        if (Mode() == 0)
        {
            // 0x497680: the player's (or the debug flag), and with Ctrl only a vehicle; then always packet 4 (the
            // selection cleared, 0x4976DA) and the object's war map object selected (0x8088), so clicking one of
            // several selected groups narrows the selection to it
            if (!best->players) return;
            if (ctrl && !vehicle(*best)) return;
            const int ref = best->ref;
            SelectRefs({ ref });
            for (size_t g = 0; g < selected.size(); g++) if (selected[g]) { SelectionSpeech(int(g)); break; }
            return;
        }
        // mode 1 (0x497794): with Ctrl force fire at the ground under it (packet 0x13); else the player's selects
        // its group and the other side's is attacked (packet 0x10)
        if (ctrl)
        {
            const int wx = x + scrollX;
            const int wy = land.ScreenYToWorldY(wx, y + scrollY);
            ForceFire(wx, wy);
        }
        else if (best->players)
        {
            const int ref = best->ref;
            SelectRefs({ ref });
            for (size_t g = 0; g < selected.size(); g++) if (selected[g]) { SelectionSpeech(int(g)); break; }
        }
        else AttackObject(best->id);
    }

    // The hotspot under a point: the frontmost (lowest on screen) of those whose square holds it
    const BattleMap::Unit* BattleMap::HotspotAt(int x, int y) const
    {
        const Unit* best = nullptr;
        int bestY = INT32_MIN;
        for (const Unit& u : units)
        {
            // the other side's in the fog have no hotspot (0x48B2D0: 0x4405C0(0) as it goes in, 0x48A660 as it
            // comes out), so they can't be hovered or clicked
            if (u.fogged) continue;
            const int half = Selectable(u);
            if (!half) continue;
            int sx, sy;
            UnitScreen(u, sx, sy);
            if (x >= sx - half && x < sx + half && y >= sy - half && y < sy + half && sy > bestY) { best = &u; bestY = sy; }
        }
        return best;
    }

    // The cursor (WM_SETCURSOR): over the minimap 0 (0x486E80); over a hotspot (0x497110) in mode 0 1 for the
    // player's (the other side's leave the view's), in mode 1 5 for the other side's and the player's with Ctrl, else
    // 1; elsewhere (0x485F60) mode 0 gives 4 with something selected else 0, mode 1 5 with Ctrl else 4. The table
    // (FrontEnd::DrawCursor) makes 1 hselcurs, 4 h-move and 5 h-attack.
    int BattleMap::CursorAt(int x, int y) const
    {
        if (!popup.empty()) return 0;
        if (InMinimap(x, y)) return 0;
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        const int mode = Mode();
        int cursor = mode == 1 ? (ctrl ? 5 : 4) : 0;
        if (const Unit* u = HotspotAt(x, y))
        {
            // mode 0 over the other side's (0x4971B7-0x4971C3): handled with no cursor set, so the last one stays
            if (mode == 0 && !u->players) return lastCursor;
            if (mode == 0) cursor = 1;
            if (mode == 1) cursor = (!u->players || ctrl) ? 5 : 1;
        }
        lastCursor = cursor;
        return cursor;
    }

    // Home (0x43A480): the middle of the selection's extent (its group's bounds, 0x4836A0: each half rounded to 0),
    // projected with the height of the vertex nearest it (0x47B120), less half the view
    void BattleMap::CentreOnSelection()
    {
        bool any = false;
        int minX = 0, maxX = 0, minY = 0, maxY = 0;
        for (const Unit& u : units)
        {
            if (!u.players || !selected[size_t(u.group)]) continue;
            const int x = int(u.x), y = int(u.y);
            if (!any) { minX = maxX = x; minY = maxY = y; any = true; }
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        if (!any) return;
        const int cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;
        // a Zoomer there at "Battlemap Zoomer Speed" (0x43A592)
        ZoomTo(cx - 320, land.ProjectY(cy, land.HeightAtNearest(cx, cy)) - 240);
    }

    // 0x446690: the target kept within the scroll (0 to the size less the view), the start the view's place now
    // (the scroller's +0x58 / +0x5C), the way there's heading and length (0x43FDA0), half of it (+0x2C), the speed
    // (+0x20), the step 0 (+0x24)
    void BattleMap::ZoomTo(int x, int y)
    {
        x = std::max(0, std::min(x, land.WorldWidth() - 640 - 1));
        y = std::max(0, std::min(y, land.WorldHeight() - 480 - 1));
        zoomer = Zoomer{};
        zoomer.active = true;
        zoomer.x = float(scrollX);
        zoomer.y = float(scrollY);
        const float fx = float(x) - zoomer.x, fy = float(y) - zoomer.y;
        const float d = float(std::sqrt(double(fx) * fx + double(fy) * fy));
        if (d > 0.0f) { zoomer.dx = fx / d; zoomer.dy = fy / d; }
        else { zoomer.dx = 1.0f; zoomer.dy = 0.0f; }
        zoomer.half = float(double(d) * 0.5);
        zoomer.speed = setup.zoomerSpeed;
    }

    // 0x446850, each update: the half less the step; past the middle (under 0) the step less the speed, else more;
    // the place moved on by the step along the heading (0x43FE90) and the view put there (0x7A, whole); at a step
    // of 0 or less it's done (0x4393D0)
    void BattleMap::ZoomTick()
    {
        if (!zoomer.active) return;
        zoomer.half -= zoomer.step;
        if (zoomer.half < 0.0f) zoomer.step -= zoomer.speed;
        else zoomer.step += zoomer.speed;
        zoomer.x += zoomer.dx * zoomer.step;
        zoomer.y += zoomer.dy * zoomer.step;
        ScrollTo(int(zoomer.x), int(zoomer.y));
        if (!(zoomer.step > 0.0f)) zoomer.active = false;
    }

    // 0x421D70 (',') / 0x421E50 ('.') over the player's units list (0x446370(1, 1): +0x4C): the object whose id is
    // the nearest below (',') or above ('.') the last one stepped to, else the highest (',') or lowest ('.')
    // *Provisional*: the war map object's index in the table stands for its id (+0x08).
    int BattleMap::StepObject(bool up)
    {
        int best = -1, bestId = 0, wrap = -1, wrapId = 0;
        for (size_t g = 0; g < setup.groups.size(); g++)
        {
            const BattleGroup& bg = setup.groups[g];
            if (!bg.Players() || bg.list / 2 != 0 || bg.ref < 0) continue;
            if (std::none_of(units.begin(), units.end(), [&](const Unit& u) { return u.ref == int(g); })) continue;
            const int id = bg.ref;
            if (up ? (id > lastStepped && (best < 0 || id < bestId)) : (id < lastStepped && (best < 0 || id > bestId))) { best = int(g); bestId = id; }
            if (wrap < 0 || (up ? id < wrapId : id > wrapId)) { wrap = int(g); wrapId = id; }
        }
        if (best < 0) { best = wrap; bestId = wrapId; }
        if (best >= 0) lastStepped = bestId;
        return best;
    }

    // The view on a war map object's units (0x43BE50 / 0x43BF30): the middle of their extent (0x42A2A0 /
    // 0x42A340), projected with the nearest vertex's height (0x47B120), less half the view; ',' puts it there at
    // once (0x7A), '.' glides there (a Zoomer at "Battlemap Zoomer Speed")
    void BattleMap::ViewOnGroup(int group, bool glide)
    {
        bool any = false;
        int minX = 0, maxX = 0, minY = 0, maxY = 0;
        for (const Unit& u : units)
        {
            if (u.ref != group) continue;    // the war map object's units
            const int x = int(u.x), y = int(u.y);
            if (!any) { minX = maxX = x; minY = maxY = y; any = true; }
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        if (!any) return;
        const int cx = (maxX - minX) / 2 + minX, cy = (maxY - minY) / 2 + minY;
        const int x = cx - 320, y = land.ProjectY(cy, land.HeightAtNearest(cx, cy)) - 240;
        if (glide) ZoomTo(x, y);
        else { zoomer.active = false; ScrollTo(x, y); }
    }

    // The selected units' brackets (0x4972F0 / 0x497000: GType 5, hselec.spr, frames 0-3 at the hotspot's corners 8 in)
    // and power bar (GType 7, hpower.spr, at the middle of its bottom 8 up; its frame the frame count x the damage,
    // 1 - health / max, at most the last: frame 0 green at full health, to red); then the drag box (0x49C3E0): two lines a side
    // (dword_4DAF9C outside, dword_4DAFB8 inside) 12 short of each corner where the side is 23 or longer, and GType 6
    // (ho-selec.spr) frames 0-3 at its corners 7 in
    void BattleMap::DrawSelection(Canvas& canvas) const
    {
        for (const Unit& u : units)
        {
            if (!u.players || !selected[size_t(u.group)]) continue;
            const int half = Selectable(u);
            int sx, sy;
            UnitScreen(u, sx, sy);
            const int l = sx - half, t = sy - half, r = sx + half, b = sy + half;
            canvas.DrawSprite(5, 0, l + 8, t + 8);
            canvas.DrawSprite(5, 1, r - 8, t + 8);
            canvas.DrawSprite(5, 2, l + 8, b - 8);
            canvas.DrawSprite(5, 3, r - 8, b - 8);
            if (const Sprite* bar = res.GetSprite(7))
            {
                // +0x98 = 1 - the health (vt+0xB4) / its type's (0x47DA20) (0x496DE3; 0x497290 as it's damaged):
                // frame ftol(the frame count x it), at most the last, at least 0, so full health is frame 0
                const int frames = bar->FrameCount();
                const int max = MaxHealth(u.type);
                const float damage = 1.0f - (max > 0 ? float(u.health) / float(max) : 1.0f);
                const int frame = std::clamp(int(float(frames) * damage), 0, std::max(0, frames - 1));
                canvas.DrawSprite(7, frame, (l + r) / 2, b - 8);
            }
        }
        if (!dragging || !boxActive) return;
        const int l = std::min(dragX0, dragX1), r = std::max(dragX0, dragX1);
        const int t = std::min(dragY0, dragY1), b = std::max(dragY0, dragY1);
        uint32_t* px = canvas.Pixels();
        const int cw = canvas.Width(), ch = canvas.Height();
        auto plot = [&](int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < cw && y < ch) px[size_t(y) * size_t(cw) + size_t(x)] = c; };
        auto hline = [&](int x0, int x1, int y, uint32_t c) { for (int x = x0; x <= x1; x++) plot(x, y, c); };
        auto vline = [&](int x, int y0, int y1, uint32_t c) { for (int y = y0; y <= y1; y++) plot(x, y, c); };
        if (b - t >= 23)
        {
            vline(l, t + 12, b - 12, rectColour);
            vline(l + 1, t + 12, b - 12, boxInner);
            vline(r, t + 12, b - 12, rectColour);
            vline(r - 1, t + 12, b - 12, boxInner);
        }
        if (r - l >= 23)
        {
            hline(l + 12, r - 12, t, rectColour);
            hline(l + 12, r - 12, t + 1, boxInner);
            hline(l + 12, r - 12, b, rectColour);
            hline(l + 12, r - 12, b - 1, boxInner);
        }
        canvas.DrawSprite(6, 0, l + 7, t + 7);
        canvas.DrawSprite(6, 1, r - 7, t + 7);
        canvas.DrawSprite(6, 2, l + 7, b - 7);
        canvas.DrawSprite(6, 3, r - 7, b - 7);
    }

    // 0x486E80's 0x201 / 0x200: the point under the mouse, as a share of the world, less half the view (0x7A)
    void BattleMap::MinimapScroll(int x, int y)
    {
        ScrollTo(land.WorldWidth() * (x - miniX) / std::max(1, miniW) - 320, land.WorldHeight() * (y - miniY) / std::max(1, miniH) - 240);
    }

    // The deployment (0x421FE0 for each list, in EnterBattleMap's order: HCON, HRES, ACON, ARES, HUNI, AUNI). Every
    // battle unit starts at its saved place; each war map object then gets a centre:
    //  - the other side's (its faction isn't the sector's owner): the pod's place (this battle's, or type 179 in the scenery
    //    list) for Martians, else the entry point from the sector it came from (0x436050)
    //  - the owner's: a command building's place (the last of 830 / 120 or 278 / 58 among all the battle units,
    //    either side's), else a pod's for Martians, else the entry point if it came from somewhere, else the middle
    // and the player's last centre is where the view starts. Its units are placed round it (Place).
    // The battle's own pod (LandPod) lands first, so it is a centre; one in the scenery list (type 179) would be
    // too, but none of the .nsb files has one.
    void BattleMap::Deploy()
    {
        units.clear();
        for (const BattleGroup& g : setup.groups)
            for (const auto& [x, y] : g.places)
            {
                Unit u{};
                u.type = g.type;
                u.x = x;
                u.y = y;
                u.heading = 0;
                u.players = g.Players();
                u.group = int(&g - &setup.groups[0]);
                u.ref = u.group;
                u.id = nextId++;
                units.push_back(u);
            }
        selected.assign(setup.groups.size(), false);
        groupPlayers.clear();
        for (const BattleGroup& g : setup.groups) groupPlayers.push_back(g.Players());
        // the battle is on (the report's +0x54, 0x439D22-0x439D83) by the lists' numbers of war map objects (0x4216D0):
        // (HUNI and (AUNI + ACON)) or ((HUNI + HCON) and AUNI); resources never count, and buildings alone on both
        // sides are no battle
        {
            int count[6] = {};
            for (const BattleGroup& g : setup.groups)
                if (g.list >= 0 && g.list < 6) count[g.list]++;
            battleOn = (count[0] && count[1] + count[3]) || (count[0] + count[2] && count[1]);
        }
        groupRetreat.assign(setup.groups.size(), 0);
        groupLeft.assign(setup.groups.size(), false);
        if (!setup.units) return;
        // the groups are made, with their grounds (0x4216E0 from 0x439A8D -> 0x4802E0), before the deployment asks
        // them where their vehicles can stand (0x480740)
        SetUpGrounds();
        // 0x439AC0: the battle world's list (+0x118: HUNI, HCON, HRES, AUNI, ACON, ARES as 0x4216E0 added them) is
        // activated (vt+0x14, 0x47D030): every object whose saved place isn't nothing (+0x78 + +0x7C + +0x84 not
        // 0.0) gets vt+0x24 and vt+0x14, so the buildings stand, their footprints down, before anything is placed.
        // *Provisional*: one that then answers vt+0x64 is taken out again (vt+0x78); not traced.
        {
            static const int Added[6] = { 0, 2, 4, 1, 3, 5 };
            size_t at = 0;
            std::vector<size_t> first;
            for (const BattleGroup& g : setup.groups) { first.push_back(at); at += g.places.size(); }
            for (int list : Added)
                for (size_t gi = 0; gi < setup.groups.size(); gi++)
                    if (setup.groups[gi].list == list)
                        for (size_t i = 0; i < setup.groups[gi].places.size(); i++)
                            if (const Unit& u = units[first[gi] + i]; u.x + u.y != 0.0f) StartUnit(first[gi] + i);
        }
        if (setup.pod) LandPod(sun);
        auto unit = [&](int type) -> const ObjOjd::UnitData* {
            auto u = setup.units->find(type);
            return u == setup.units->end() ? nullptr : &u->second;
        };
        const int midX = land.WorldWidth() / 2;
        const int midY = int((int64_t(land.Height()) * land.Cell()) >> 16) / 2;
        // each group's first unit in units
        std::vector<size_t> first;
        size_t at = 0;
        for (const BattleGroup& g : setup.groups) { first.push_back(at); at += g.places.size(); }
        static const int Order[6] = { 2, 4, 3, 5, 0, 1 };
        for (int list : Order)
        {
            // the command building, as the units stand now
            int hqX = -1, hqY = -1;
            for (const Unit& u : units)
                if (u.type == deploy::MartianHq(setup.sector) || u.type == deploy::HumanHq(setup.sector))
                {
                    hqX = int(u.x);
                    hqY = int(u.y);
                }
            // the list's faction: its first object's
            int faction = -1;
            for (size_t gi = 0; gi < setup.groups.size(); gi++)
                if (setup.groups[gi].list == list)
                {
                    if (const ObjOjd::UnitData* d = unit(setup.groups[gi].type)) faction = d->faction;
                    break;
                }
            for (size_t gi = 0; gi < setup.groups.size(); gi++)
            {
                const BattleGroup& g = setup.groups[gi];
                if (g.list != list || g.places.empty()) continue;
                const ObjOjd::UnitData* d = unit(g.type);
                const int moveClass = d ? d->field52 : 0;
                int cx = midX, cy = midY;
                // 0x4220CB: the pod (type 0xB3 in the world's +0x11C) a centre for a Martian list (its first object's
                // faction 1, 0x4406E0)
                const bool pod = podX != -1 && podY != -1 && faction == 1;
                if (setup.ownerFaction != faction)
                {
                    if (pod) { cx = podX; cy = podY; }
                    else deploy::EntryPoint(g.previous, setup.sector, moveClass, cx, cy);
                }
                else if (hqX != -1 || hqY != -1) { cx = hqX; cy = hqY; }
                else if (pod) { cx = podX; cy = podY; }
                else if (g.previous) deploy::EntryPoint(g.previous, setup.sector, moveClass, cx, cy);
                if (g.Players()) { startX = cx; startY = cy; }
                std::vector<size_t> members;
                for (size_t i = 0; i < g.places.size(); i++) members.push_back(first[gi] + i);
                Place(members, cx, cy);
            }
        }
    }

    // An object's start as the deployment places it (0x422756-0x422775: with +0x18 clear, vt+0x24, which makes its
    // renderer and its parts, 0x47EE10, then vt+0x14), so each one is in the world, its footprint down (vt+0xE4,
    // 0x48A950, its parts' with it as 0x47EF30 starts them), before the next is placed. A building's start
    // (0x48E9D0) first takes out the scenery standing on it (ClearScenery).
    // The rand() calls of one start, in the original's order: the light parts' rolls (0x48DE80) as the parts are
    // made and started, then the look round's count (0x4957A2). The phase isn't one of them (see the constructor).
    void BattleMap::StartUnit(size_t index)
    {
        if (units[index].started) return;
        units[index].started = true;
        NoteModel(units[index].type, false);
        if (IsBuilding(units[index])) ClearScenery(index);
        Unit& u = units[index];
        // a vehicle (category 1) drives: 0x490820 / 0x490E70 (the turn limit by move class, the phase from rand())
        // every object's +0xAC is its type's +0x44 word (the neighbours keep apart by it); an IOB object's is its
        // model's radius (0x47EB10)
        u.drive.radius = Radius44(u.type);
        u.health = MaxHealth(u.type);
        // 0x48E9D0: a building whose construction isn't done (+0x7C / +0x78 at most 0.9999; +0xC0 bit 0 clear)
        // starts with that much of its health, at least 1
        const float progress = Progress(u);
        const bool unfinished = double(progress) <= 0.9999;
        if (IsBuilding(u) && unfinished) u.health = std::max(1, int(float(u.health) * progress));
        if (auto d = setup.units->find(u.type); d != setup.units->end()) u.stance = d->second.field43;   // 0x495740
        SetUpGuns(u);
        // 0x48F7C0: an emplacement not on a vehicle whose construction is under 1.0 has no guns (+0x3C 0)
        if (IsEmplacement(u.type) && progress < 1.0f) u.guns.clear();
        if (auto d = setup.units->find(u.type); d != setup.units->end() && d->second.category == 1)
        {
            u.vehicle = true;
            const int mc = d->second.field52;
            u.drive.turnRate = mc >= 0 && mc <= 4 ? 64 : (mc == 5 || mc == 6) ? 8 : mc == 7 ? 16 : 0;
            u.drive.arrival = float(double(u.drive.radius) * setup.vehicles.arrivalRadius - -20.0);
            u.drive.phase = index < phases.size() ? phases[index] : int16_t(random::Rand());   // (see the constructor)
        }
        AddRenderer(u.type, u.x, u.y, u.heading, int(index));
        AddPartRenderers(index);
        // the look round's count (+0xDA) rand() % 30 (0x4957A2), so the units don't all look on one update: drawn
        // after the base start (0x495785 -> 0x48A8B0) has started the parts, the light parts' rolls among them
        units[index].scanCount = random::Rand() % 30;
        if (IsBuilding(u) && unfinished && u.players) AddCorners(index);
        // vt+0xE4: the mines' class (0x273-0x276, 0x48C8E0) has its own (0x48CB10): only once built (+0xC0 bit 0),
        // the vertices within its type's +0x46 get flag 8, not 2, which the ground's cost refuses to the groups
        // of the sector's owner (0x480680): a side keeps off its own minefield
        if (units[index].type >= 0x273 && units[index].type <= 0x276)
        {
            if (!unfinished) land.MarkOccupied(int(units[index].x), int(units[index].y), Radius46(units[index].type), 8);
        }
        else Occupy(units[index].type, units[index].x, units[index].y);
    }

    // 0x4222C0: a war map object's units. They keep their saved places unless the first stands at (0, 0), or is a
    // vehicle (category 1) and either the computer's or within the no-build border of the map's edge. Otherwise a
    // spot is looked for round the centre, ring by ring: the k x k block they stand in (k = ftol(sqrt(n) + 0.5),
    // each 2 x size apart, size the type's +0x46 word) must fit, e vertices round the spot (e = k x size in
    // vertices, rounded), on passable ground for vehicles with no unit already in the block's places, on ground a
    // building could go for the rest (0x47B610). The rings are 0.7075 x k x size apart, each with ftol(radius / step)
    // + 1 spots from angle 0; past the map's width and height the middle is used. Each unit then takes its place in
    // the block, row by row, and a heading of rand() & 0xFFF.
    // The vehicles' ground check (0x47EB00 -> 0x480740): every vertex of the area through the first unit's group's
    // cost (0x480680, not strict), which refuses ground the group can't cross and vertices an object stands on
    // (flag bit 1: the footprints of the buildings and scenery started so far, their parts' among them); an area
    // off the map fails. Then each place in the block against the world's objects (0x47FE10 -> 0x47FD40, the
    // buckets round the place): one that gets in the first unit's way (vt+0xBC) nearer (octagonal, whole
    // positions) than its radius (+0xAC) + the size refuses the spot. Each unit placed is started at once
    // (StartUnit), so the next object's checks see it.
    // The world's objects are the grid's (dword_4D2550's +0x118, the four buckets round the place, 0x449920): the
    // units started so far and the scenery.
    // *Provisional*: whether a building's parts are in the grid as well; the walkers' pass over low scenery
    // (0x492A04) isn't applied to the scenery here; +0x188 bit 13 (a fixed place) and +0x18C aren't read.
    void BattleMap::Place(std::vector<size_t> members, int cx, int cy)
    {
        if (members.empty()) return;
        Unit& head = units[members[0]];
        auto u = setup.units->find(head.type);
        if (u == setup.units->end()) return;
        const int n = int(members.size());
        const int size = Radius46(head.type);
        const int k = int(std::sqrt(double(n)) - -0.5);
        const int span = k * size;
        const int32_t cell = land.Cell();
        const int e = int((int64_t(int32_t(uint32_t(span) << 16)) + cell / 2) / cell);
        const int step = int(double(span) * 0.7075);
        const bool vehicle = u->second.category == 1;
        bool needs = head.x == 0.0f && head.y == 0.0f;
        if (vehicle)
        {
            if (head.players)
            {
                const int vx = int((int64_t(int32_t(uint32_t(int(head.x)) << 16)) + cell / 2) / cell);
                const int vy = int((int64_t(int32_t(uint32_t(int(head.y)) << 16)) + cell / 2) / cell);
                const int b = setup.noBuildBorder;
                if (vx < b || vy < b || vx > land.Width() - b || vy > land.Height() - b) needs = true;
            }
            else needs = true;
        }
        const int W = land.WorldWidth(), H = int((int64_t(land.Height()) * cell) >> 16);
        if (!needs)
        {
            // 0x4227F0: each told 0x80C3 with the map's middle (+0x1C / 2, the height in world units / 2)
            for (size_t idx : members) Muster(idx, W / 2, H / 2);
            return;
        }
        auto toVertex = [&](int v) { return int(int32_t(uint32_t(v) << 16) / cell); };
        int px = W / 2, py = H / 2;
        bool found = false, giveUp = false;
        float r = 0.0f;
        while (!found && !giveUp)
        {
            const float stepF = float(step);
            const int m = stepF != 0.0f ? int(r / stepF) : 0;
            const float angleStep = float(6.283185307 / double(m));
            float a = 0.0f;
            for (int i = 0; i <= m && !found; i++)
            {
                const int tx = int(std::cos(double(a)) * r + cx), ty = int(std::sin(double(a)) * r + cy);
                const int vx = toVertex(tx), vy = toVertex(ty);
                bool ok;
                if (vehicle)
                {
                    // 0x480740: off the map fails; else every vertex's cost for the group
                    const int x0 = vx - e, y0 = vy - e, x1 = vx + e + 1, y1 = vy + e + 1;
                    ok = x0 >= 0 && y0 >= 0 && x1 <= land.Width() && y1 <= land.Height();
                    for (int y = y0; y < y1 && ok; y++)
                        for (int x = x0; x < x1 && ok; x++)
                            if (CellCost(head.group, x, y, false) == 0) ok = false;
                    // each member's place in the block, asked by that member of the world's grid (0x4225B5 ->
                    // 0x47FE10: ObjectAt): the units started so far, the scenery, the buildings' parts and the
                    // bridges' segments
                    int bx = size + tx - span, by = size + ty - span, col = 0;
                    for (int j = 0; j < n && ok; j++)
                    {
                        Obstacle found;
                        if (ObjectAt(units[members[size_t(j)]], bx, by, size, found)) ok = false;
                        bx += 2 * size;
                        if (++col == k) { col = 0; by += 2 * size; bx = size + tx - span; }
                    }
                }
                else ok = land.CanBuildAt(vx - e, vy - e, vx + e + 1, vy + e + 1, false);
                if (ok) { found = true; px = tx; py = ty; }
                a = float(double(angleStep) + a);
            }
            r = float(double(r) + stepF);
            if (!(r <= float(W)) && float(H) < r) giveUp = true;
            if (step <= 0) giveUp = true;
        }
        if (!found) { px = W / 2; py = H / 2; }
        const int x0 = px + size - span;
        int x = x0, y = py + size - span, col = 0;
        for (size_t idx : members)
        {
            Unit& m = units[idx];
            m.x = float(x);
            m.y = float(y);
            m.heading = random::Rand() & 0xFFF;
            StartUnit(idx);          // 0x422756: not yet started (+0x18), vt+0x24 and vt+0x14
            Muster(idx, px, py);     // 0x422787: 0x80C3 with the spot
            x += 2 * size;
            if (++col == k) { col = 0; y += 2 * size; x = x0; }
        }
    }

    // 0x47EB10 (see BattleMap.h): the radius an IOB object takes from its model, kept by type as the game's write
    // over the unit table keeps it
    void BattleMap::NoteModel(int type, bool footprint)
    {
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return;
        const GType* g = res.FindGType(u->second.field28);
        if (!g || g->gclass != 1) return;
        const IsoModel* m = LoadIob(u->second.field28);
        if (!m) return;
        modelRadius[type] = int(float(std::max(2 * IsoObject::HalfWidth(m->model), IsoObject::Depth(m->model))) * 0.66f);
        if (footprint) modelFootprint.insert(type);
    }

    int BattleMap::Radius44(int type) const
    {
        if (auto m = modelRadius.find(type); m != modelRadius.end()) return m->second;
        auto u = setup.units->find(type);
        return u == setup.units->end() ? 0 : int(int16_t(u->second.hitPoints));
    }

    int BattleMap::Radius46(int type) const
    {
        if (modelFootprint.count(type))
            if (auto m = modelRadius.find(type); m != modelRadius.end()) return m->second;
        auto u = setup.units->find(type);
        return u == setup.units->end() ? 0 : int(int16_t(u->second.currentHitPoints));
    }

    // The factory's (0x446B30) types built on a class whose vt+0xB8 is true: WScenery (0x489380: 0xD5-0x10F,
    // 0x111-0x115, 721-741, 845-904, 973-985), 0x48B480 (696) and 0x4873F0 (987)
    bool BattleMap::SceneryClass(int type)
    {
        return (type >= 0xD5 && type <= 0x10F) || (type >= 0x111 && type <= 0x115) || (type >= 721 && type <= 741) ||
               (type >= 845 && type <= 904) || (type >= 973 && type <= 985) || type == 696 || type == 987;
    }

    // A building's start (0x48E9D0 at 0x48EB3A-0x48EBD0): every object of the scenery list (the world's +0x11C)
    // nearer (octagonal, whole positions) than its radius (+0xAC) + the building's type's +0x46 word is taken out
    // (vt+0x78, 0x4895A0: no explosion). A tree circle goes with its trees, its children.
    // A pod is taken out the same way (its vt+0x78 is 0x4895A0 too), and a bridge (0x48D2E0 -> 0x4895A0), which
    // breaks as it goes (BridgeDies).
    void BattleMap::ClearScenery(size_t building)
    {
        const Unit& b = units[building];
        const int reach = Radius46(b.type);
        const int bx = int(b.x), by = int(b.y);
        for (size_t c = 0; c < circles.size(); c++)
        {
            Circle& circle = circles[c];
            if (!circle.alive || Octagonal(bx, by, circle.x, circle.y) >= Radius44(circle.type) + reach) continue;
            circle.alive = false;
            for (size_t i = 0; i < props.size(); i++)
                if (props[i].alive && props[i].circle == int(c)) RemoveProp(i);
        }
        for (size_t i = 0; i < props.size(); i++)
        {
            const Prop& p = props[i];
            if (!p.alive || p.circle >= 0) continue;
            if (Octagonal(bx, by, int(p.x), int(p.y)) < Radius44(p.type) + reach) RemoveProp(i);
        }
        for (size_t i = 0; i < bridgeLinks.size(); i++)
            if (!bridgeLinks[i].gone && Octagonal(bx, by, bridgeLinks[i].x, bridgeLinks[i].y) < Radius44(bridgeLinks[i].type) + reach)
                BridgeDies(i, true);
    }

    // A battle unit's renderer by its GType's class (0x47EB10): a WOF model (2) posed at its place and heading, an
    // IOB model (1) or a sprite (0). Drawn with the ground like the scenery.
    // The unit's turrets' WOF models (0x47EE10 makes each part a child object with its renderer: 742 on the Armoured
    // Lorry), turned with the turret; posed each update at the unit's point plus the part's offset turned by the
    // unit's heading, its height the unit's plus the part's z (PoseParts)
    // A part with a record 27 box is held to a point of its unit's model (+0xB0, 0x47F340): the lorries' turret
    // to piece 1, point 0, the roof. The shadows (0x13B-0x14A) and the light parts (334-346) are made here too.
    // *Provisional*: the offset's turn (x along the heading, y across it) for the parts not held; the parts' own
    // parts (a turret's gun has a sprite, GType class 6) aren't drawn.
    void BattleMap::AddPartRenderers(size_t index)
    {
        Unit& u = units[index];
        u.attached.clear();
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        int turret = 0;
        // every part is a child (0x47EE10 -> 0x47D190, added at the tail of +0x50): a building loses them from the
        // last (LoseParts)
        u.partsLeft = int(t->second.parts.size());
        for (size_t pi = 0; pi < t->second.parts.size(); pi++)
        {
            const auto& part = t->second.parts[pi];
            const bool isTurret = (part.a >= 0x2E6 && part.a <= 0x313) || (part.a >= 0x389 && part.a <= 0x38C);
            const int thisTurret = isTurret ? turret++ : -1;
            // the vehicles' shadows (0x13B-0x14A, WShadow 0x48E680): gone as they start without "Show vehicle
            // shadows" (0x48E6D0)
            const bool isShadow = part.a >= 0x13B && part.a <= 0x14A;
            if (isShadow && (!setup.showShadows || ShadowBlend().empty())) continue;
            if (part.a >= 334 && part.a <= 346)
            {
                // a light part (0x48DE80): under a parent of category 0-3 it stays when rand() % 100 is at most 100 x
                // the hour's fourth float and the parent isn't Martian; then only with "Show lights" (AddLight). It
                // goes with its unit, at the part's offset (PoseParts).
                // (the rand() only under a parent of category 0-3, 0x48DE80)
                const float chance = setup.dayTime ? setup.dayTime->sets[0].f[3] : 0.0f;
                const int category = t->second.category;
                if (category >= 0 && category <= 3)
                {
                    const int roll = random::Rand() % 100;
                    if (double(chance) * 100.0 < double(roll) || t->second.faction == 1) continue;
                }
                const int x = int(u.x) + int16_t(part.b), y = int(u.y) + int16_t(part.c);
                const int li = AddLight(part.a, x, y, land.HeightAt(x, y) + int(u.drive.lift) + int16_t(part.d));
                if (li < 0) continue;
                // its view object goes with the unit's (a dead unit's are taken out together)
                for (ViewObject& o : objects)
                    if (o.light == li) o.unit = int(index);
                Attached a;
                a.part = int(pi);
                a.light = li;
                a.ox = int16_t(part.b);
                a.oy = int16_t(part.c);
                a.oz = int16_t(part.d);
                u.attached.push_back(a);
                continue;
            }
            auto pt = setup.units->find(part.a);
            if (pt == setup.units->end()) continue;
            const int gtype = pt->second.field28;
            const GType* g = res.FindGType(gtype);
            if (g && g->gclass == 1)
            {
                // an IOB part (a building's roofs, walls and chimneys: the Factory's F_1r, Vf_2, OR_Chim): at the
                // unit's point plus its offset, on the unit's ground (0x48E9D0 puts each child at its own +0xA0 plus
                // the building's ground less the child's: the building's ground, flat under all its parts).
                // *Provisional*: the offset isn't turned with a unit's heading (the buildings don't turn).
                const IsoModel* m = LoadIob(gtype);
                if (!m) continue;
                const int x = int(u.x) + int16_t(part.b), y = int(u.y) + int16_t(part.c);
                // a child (+0x44 set before its vt+0x24, 0x47EE89): its model's radius is its type's +0x46 too
                // (0x47EB10), and as its parent starts it (0x47EF30 -> its vt+0x14, 0x48A8B0) its footprint goes
                // down round its own point (vt+0xE4)
                NoteModel(part.a, true);
                Occupy(part.a, float(x), float(y));
                // the part's own parts are made and started with it (0x47EE10 on the child): each wing, leg and pod of
                // a Martian building has a shadow part (331) of its own, laid from the part's renderer at its point
                LayShadows(pt->second, float(x), float(y), IsoObject::HalfWidth(m->model), IsoObject::Depth(m->model));
                const int depth = int16_t(part.c) + IsoObject::Depth(m->model);
                ViewObject v{ int(u.y) + depth, x, land.ProjectY(y, land.HeightAt(int(u.x), int(u.y)) + int16_t(part.d)), m };
                v.unit = int(index);
                v.depth = depth;
                v.part = int(pi);
                objects.push_back(v);
                Attached a;
                a.part = int(pi);
                a.ox = int16_t(part.b);
                a.oy = int16_t(part.c);
                a.oz = int16_t(part.d);
                u.attached.push_back(a);
                continue;
            }
            if (!g || g->gclass != 2 || !(isTurret || isShadow)) continue;
            auto f = wofFiles.find(gtype);
            if (f == wofFiles.end())
            {
                auto data = res.LoadFile(g->file);
                f = wofFiles.emplace(gtype, data ? std::move(*data) : std::vector<uint8_t>{}).first;
            }
            if (f->second.empty()) continue;
            float scale = 65536.0f;
            int follow = 0, depth = 0;
            if (setup.six)
                if (auto six = setup.six->find(gtype); six != setup.six->end())
                {
                    scale = six->second.a;
                    follow = six->second.c;
                    depth = 2 * int16_t(six->second.d);
                }
            // the object's scale (+0xA4): 1.0 as it's made (0x47D779), times the part's float (0x47EEC0), then its
            // type's (+0xA8, 0x47EE24) as it starts; the model's scales then ftol(ftol(the GType's float) x it)
            // (0x49DCA0, 0x47ED08)
            scale = float(int(float(int(scale)) * (part.f * pt->second.scale)));
            std::unique_ptr<WofInstance> w;
            try { w = std::make_unique<WofInstance>(f->second, scale, follow); }
            catch (const std::exception&) { continue; }
            Attached a;
            a.part = int(pi);
            a.wof = w.get();
            a.ox = int16_t(part.b);
            a.oy = int16_t(part.c);
            a.oz = int16_t(part.d);
            a.turret = thisTurret;
            if (part.box[0] >= 0) { a.piece = part.box[0]; a.vertex = part.box[1]; }
            if (isShadow)
            {
                // 0x48E700: drawn darkening (+0x48 0x20), its x scale x (|the sun's cos| + 1); one behind its
                // unit (0x48E850: its renderer's +0x26 and +0x1E the parent's less 1)
                a.shadow = true;
                w->SetShadow(ShadowBlend().data());
                w->ScaleX(std::fabs(sun.across) - -1.0f);
                depth = -1;
                for (const ViewObject& o : objects)
                    if (o.unit == int(index) && o.light < 0 && (!o.wof || o.wof == u.wof)) { depth = o.depth - 1; break; }
            }
            u.attached.push_back(a);
            objects.push_back({ int(u.y) + depth, 0, 0, nullptr, w.get(), g->paletteSlot, 0, -1, int(index), depth, -1, -1, int(pi) });
            wofs.push_back(std::move(w));
        }
        PoseParts(u);
        // its shadow parts (0x14B-0x14D): a unit's are laid as they start like the scenery's (0x48E460), from its
        // renderer as it stands: an IOB model's half width and depth; a WOF's +0x28 / +0x2A are still what its
        // constructor gave them (0x49DB06: less and plus its GType's record 6 second word, 0x4F0532; the drawn
        // extents only replace them at its first draw, 0x49DC2C) and its +0x26 the posed model's (0x47F3A3)
        if (!u.vehicle)
        {
            int hw = 0, depth = 1;
            bool has = false;
            for (const ViewObject& o : objects)
            {
                if (o.unit != int(index) || o.part >= 0 || o.light >= 0) continue;
                if (o.model) { hw = IsoObject::HalfWidth(o.model->model); depth = IsoObject::Depth(o.model->model); has = true; }
                else if (o.wof && o.wof == u.wof)
                {
                    if (setup.six)
                        if (auto six = setup.six->find(t->second.field28); six != setup.six->end()) hw = int16_t(six->second.d);
                    depth = u.wof->PosedDepth();
                    has = true;
                }
                if (has) break;
            }
            if (has) LayShadows(t->second, u.x, u.y, hw, depth);
        }
    }

    // The guns' own visuals (vt+0x4C 0x495180 and vt+0xE4 0x495280, the quads made at the gun's start, vt+0xE0
    // 0x4950D0: four atlas quads of the gun type's GType, 16-pixel cells, +0x26 0x40). While the fire cycle is in 2
    // (firing) and the type has a GType (0x4950D0 also sets each quad's level, vt+0x18 = 0x40F040, to the type's +0xA1
    // / 4): quad i shows cell (the type's +0x2C x 16 + i + the phase), the phase (+0xA8)
    // going on by 4 each draw and back to 0 at 16; then the quads are laid from the gun's point (+0x78..+0x80) to
    // its target's (+0xBC..+0xC4), both projected (x, the iso y less the height x 0x2C3E63FB >> 14, plus the top),
    // in four steps of a quarter of the way, each one's half width (along the screen normal) going from the type's
    // record 12 +0xA to +0xC by a quarter of the difference a step; a quad's near corners are (X +- p, Y +- q) and
    // its far ones (X' +- p', Y' +- q'), the texture's far end at the target. Each is put in the view a quarter of
    // the way further back (its +0x1E from the gun's y + 40).
    // Drawn with the view's objects by their places (BeamItem keys), the shells' class's sprite at the target too.
    // *Provisional*: a gun not held to a point is taken at its unit's place and height; the sprite's flag (+0x14 bit
    // 0, the blitter's mode 2) is the 8-bit translucent blend, which a 16-bit screen doesn't use.
    void BattleMap::BuildBeams(std::vector<BeamItem>& out) const
    {
        out.clear();
        if (!setup.units || !setup.weapons) return;
        auto project = [&](int x, int y, int z, int& sx, int& sy) {
            sx = x - scrollX;
            sy = land.ProjectY(y, z) - scrollY;
        };
        for (const Unit& u : units)
        {
            if (u.fogged || u.hidden) continue;
            for (const Gun& g : u.guns)
            {
                // the shells' class's draw (0x4885C0) steps its sprite (0x40EFB0 with 0x100) on every draw, firing or
                // not; 0x495180 shows it only in state 2
                int flash = 0;
                if (g.cls == 0)
                    if (const Sprite* sp = res.GetSprite(0xFF); sp && sp->FrameCount() > 0)
                    {
                        flash = g.flashFrame % sp->FrameCount();
                        g.flashFrame = (g.flashFrame + 1) % sp->FrameCount();
                    }
                if (g.state != 2) continue;
                auto gt = setup.units->find(g.type);
                if (gt == setup.units->end() || !gt->second.field28) continue;
                const GType* gg = res.FindGType(gt->second.field28);
                if (!gg || gg->gclass != 6) continue;
                const std::vector<uint8_t>* atlas = res.GetRaw(gt->second.field28);
                if (!atlas || atlas->empty()) continue;
                auto st = slotTables.find(gg->paletteSlot);
                const ShadeTable* shade = st != slotTables.end() ? &st->second : res.GetShade(gg->paletteSlot);
                if (!shade) continue;
                const int level = (gt->second.fieldA1 & 0xFFFF) >> 2;
                auto w = setup.weapons->find(g.type);
                const int w1 = w != setup.weapons->end() ? int16_t(w->second[5]) : 0;
                const int w2 = w != setup.weapons->end() ? int16_t(w->second[6]) : 0;
                // 0x495180: the frames
                int frames[4];
                for (int i = 0; i < 4; i++) frames[i] = int(double(int64_t(gt->second.counterFrame) * 16 + i) + g.beamPhase);
                g.beamPhase = float(g.beamPhase - -4.0f);
                if (g.beamPhase == 16.0f) g.beamPhase = 0;
                // the gun's own point (PoseChildren)
                const int gx = g.x, gy = g.y, gz = g.z;
                // 0x495280: the quads
                int x0, y0, x1, y1;
                project(gx, gy, gz, x0, y0);
                project(g.tx, g.ty, g.tz, x1, y1);
                const double len = std::sqrt(double(x0 - x1) * (x0 - x1) + double(y0 - y1) * (y0 - y1));
                if (!(len > 0.0)) continue;
                const float dx = float(x1 - x0), dy = float(y1 - y0);
                const float ux = float(dx / len), uy = float(dy / len);
                const float stepX = dx * 0.25f, stepY = dy * 0.25f;
                float px = uy * float(w1), py = -ux * float(w1);
                const float pxg = uy * float(w2 - w1) * 0.25f, pyg = -ux * float(w2 - w1) * 0.25f;
                float X = float(x0), Y = float(y0);
                // each quad's place in the view (+0x1E): the gun's y + 40, a quarter of the way on each
                int key = gy + 40;
                const int keyStep = (g.ty - gy) / 4;
                for (int i = 0; i < 4; i++)
                {
                    const float X1 = X + stepX, Y1 = Y + stepY;
                    const float pn = px + pxg, qn = py + pyg;
                    BeamItem b;
                    b.key = key;
                    b.atlas = atlas;
                    b.shade = shade;
                    b.level = level;
                    const int q[4][2] = { { int(X1 - pn), int(Y1 - qn) }, { int(X1 + pn), int(Y1 + qn) },
                                          { int(X + px), int(Y + py) }, { int(X - px), int(Y - py) } };
                    std::memcpy(b.q, q, sizeof(q));
                    // 0x49D7E0: the cell's place in the 256-wide atlas
                    const int f = frames[i];
                    b.cell = size_t(f / 16) * 16 * 256 + size_t(f % 16) * 16;
                    out.push_back(b);
                    px = pn;
                    py = qn;
                    X = X1;
                    Y = Y1;
                    if (i < 3) key += keyStep;
                }
                // the shells' class (0x4884C0 / 0x4885C0 / 0x4885F0) also shows a sprite at the target, GType 0xFF
                // (BOTTOM2.SPR) at the type's +0xA1 / 4, a frame on each draw (0x40EFB0), 4 nearer than the last quad
                if (g.cls == 0)
                    if (const Sprite* sp = res.GetSprite(0xFF); sp && sp->FrameCount() > 0)
                    {
                        BeamItem b;
                        b.key = key - 4;
                        b.sprite = 0xFF;
                        b.frame = flash;
                        b.x = x1;
                        b.y = y1;
                        b.level = level;
                        out.push_back(b);
                    }
            }
        }
        std::stable_sort(out.begin(), out.end(), [](const BeamItem& a, const BeamItem& b) { return a.key < b.key; });
    }

    void BattleMap::DrawBeam(const BeamItem& b, Canvas& canvas, const TerrainView::Target& t) const
    {
        if (b.sprite)
        {
            const GType* g = res.FindGType(b.sprite);
            const IndexedImage* image = res.GetFrame(b.sprite, b.frame);
            const ShadeTable* shade = g ? res.GetShade(g->paletteSlot) : nullptr;
            if (g && image && shade) canvas.DrawImage(*image, shade->Level(b.level), b.x - g->hotX, b.y - g->hotY);
            return;
        }
        if (b.atlas && b.shade) DrawAtlasQuad(t, *b.shade, *b.atlas, b.cell, 16, b.q, b.level);
    }


    // dat\b16.hsh, loaded once (PolyLib's set-up 0x40FEA0 reads it for a 16-bit screen; b15.hsm and b8.shl are the
    // other depths')
    const std::vector<uint16_t>& BattleMap::ShadowBlend()
    {
        if (shadowBlend.empty())
            if (auto data = res.LoadFile("dat\\b16.hsh"); data && data->size() >= 64 * 512)
            {
                shadowBlend.resize(64 * 256);
                for (size_t i = 0; i < shadowBlend.size(); i++) shadowBlend[i] = uint16_t((*data)[i * 2] | ((*data)[i * 2 + 1] << 8));
            }
        return shadowBlend;
    }

    void BattleMap::PoseParts(Unit& u)
    {
        const double a = double(u.heading & 0xFFF) * 0.000244140625 * 6.283185307;
        const double c = std::cos(a), s = std::sin(a);
        for (Attached& p : u.attached)
        {
            if (p.light >= 0 && size_t(p.light) < lights.size())
            {
                // the light keeps to its unit: the part's offset turned with it, raised 16 (as AddLight)
                Light& l = lights[size_t(p.light)];
                l.x = int(u.x + float(p.ox * c - p.oy * s));
                l.y = int(u.y + float(p.ox * s + p.oy * c));
                l.z = land.HeightAt(l.x, l.y) + int(u.drive.lift) + p.oz + 16;
                for (ViewObject& o : objects)
                    if (o.light == p.light) { o.x = l.x; o.y = l.y; o.key = l.y + (-32 - l.range); }
                continue;
            }
            if (!p.wof) continue;   // an IOB part stays where it was put (a building's)
            if (p.shadow)
            {
                // 0x48E750: its height offset (+0xA0) takes it down to the ground (0 on a bridge link), and its x
                // offset (+0x98) is ((h / 12) + 1) x the sun's cos x -6, h the unit's height plus its model's lowest
                // screen point (+0x70; that alone on a link)
                const int lowest = u.wof ? u.wof->MaxY() : 0;
                const double h = u.drive.link ? double(lowest) : double(u.drive.lift) + lowest;
                p.oz = u.drive.link ? 0 : int(-u.drive.lift);
                p.ox = int(float((h * 0.08333333333333333 - -1.0) * sun.across * -6.0));
            }
            int heading = u.heading;
            if (p.turret >= 0 && size_t(p.turret) < u.turrets.size()) heading += u.turrets[size_t(p.turret)].turn;
            // held to a point of its unit's model (+0xB0): after the unit's pose (0x47F3C3) its offset is that point
            // less the unit's place (vt+0x38), so it's put on the point itself (vt+0x7C)
            if (int at[3]; p.piece >= 0 && u.wof && u.wof->WorldPoint(p.piece, p.vertex, at))
            {
                p.wof->Transform(land, heading & 0xFFF, at[0], at[1], at[2]);
                continue;
            }
            const int x = int(u.x + float(p.ox * c - p.oy * s));
            const int y = int(u.y + float(p.ox * s + p.oy * c));
            p.wof->Transform(land, heading & 0xFFF, x, y, int(float(land.HeightAt(x, y)) + u.drive.lift) + p.oz);
        }
        PoseChildren(u);
    }

    // The children's own points (+0x78..+0x80), as 0x47F340 poses each child after its parent: a turret on the point
    // of its unit's model it's held to (+0xB0), else at its offset turned with the unit; a gun on the point of its
    // parent's model (its turret's, else its unit's) it's held to, else at its offset turned with its parent. The
    // range tests, the shells' launch and the aim's jitter measure from these.
    void BattleMap::PoseChildren(Unit& u)
    {
        const double a = double(u.heading & 0xFFF) * 0.000244140625 * 6.283185307;
        const double c = std::cos(a), s = std::sin(a);
        const int ground = land.HeightAt(int(u.x), int(u.y)) + int(u.drive.lift);
        for (size_t k = 0; k < u.turrets.size(); k++)
        {
            Turret& t = u.turrets[k];
            const Attached* at = nullptr;
            for (const Attached& p : u.attached)
                if (p.turret == int(k)) { at = &p; break; }
            if (int w[3]; at && at->piece >= 0 && u.wof && u.wof->WorldPoint(at->piece, at->vertex, w)) { t.x = w[0]; t.y = w[1]; t.z = w[2]; continue; }
            t.x = int(u.x + float(t.ox * c - t.oy * s));
            t.y = int(u.y + float(t.ox * s + t.oy * c));
            t.z = ground + t.oz;
        }
        for (Gun& g : u.guns)
        {
            const WofInstance* parent = u.wof;
            int px = int(u.x), py = int(u.y), pz = ground, heading = u.heading;
            if (g.turret >= 0 && size_t(g.turret) < u.turrets.size())
            {
                const Turret& t = u.turrets[size_t(g.turret)];
                px = t.x; py = t.y; pz = t.z; heading += t.turn;
                parent = nullptr;
                for (const Attached& p : u.attached)
                    if (p.turret == g.turret) parent = p.wof;
            }
            if (int w[3]; g.piece >= 0 && parent && parent->WorldPoint(g.piece, g.vertex, w)) { g.x = w[0]; g.y = w[1]; g.z = w[2]; continue; }
            const double b = double(heading & 0xFFF) * 0.000244140625 * 6.283185307;
            const double cb = std::cos(b), sb = std::sin(b);
            g.x = int(float(px) + float(g.ox * cb - g.oy * sb));
            g.y = int(float(py) + float(g.ox * sb + g.oy * cb));
            g.z = pz + g.oz;
        }
    }

    void BattleMap::AddRenderer(int type, float x, float y, int heading, int unit)
    {
        if (!setup.units) return;
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return;
        const int gtype = u->second.field28;
        const GType* g = res.FindGType(gtype);
        if (!g) return;
        const int ix = int(x), iy = int(y);
        if (g->gclass == 2)
        {
            auto f = wofFiles.find(gtype);
            if (f == wofFiles.end())
            {
                auto data = res.LoadFile(g->file);
                f = wofFiles.emplace(gtype, data ? std::move(*data) : std::vector<uint8_t>{}).first;
            }
            if (f->second.empty()) return;
            float scale = 65536.0f;
            int follow = 0, depth = 0;
            if (setup.six)
                if (auto six = setup.six->find(gtype); six != setup.six->end())
                {
                    scale = six->second.a;
                    follow = six->second.c;
                    depth = 2 * int16_t(six->second.d);
                }
            // the object's scale (+0xA4: 1.0 times its type's +0xA8 float, 0x47EE24) on the model's (0x47ED08)
            scale = float(int(float(int(scale)) * u->second.scale));
            std::unique_ptr<WofInstance> w;
            try { w = std::make_unique<WofInstance>(f->second, scale, follow); }
            catch (const std::exception&) { return; }
            w->Transform(land, heading, ix, iy, land.HeightAt(ix, iy));
            objects.push_back({ iy + depth, 0, 0, nullptr, w.get(), g->paletteSlot, 0, -1, unit, depth });
            if (unit >= 0) units[size_t(unit)].wof = w.get();
            wofs.push_back(std::move(w));
        }
        else if (g->gclass == 1)
        {
            const IsoModel* m = LoadIob(gtype);
            if (!m) return;
            // with its unit, so it goes as the unit does
            ViewObject v{ iy + IsoObject::Depth(m->model), ix, land.ProjectY(iy, land.HeightAt(ix, iy)), m };
            v.unit = unit;
            v.depth = IsoObject::Depth(m->model);
            objects.push_back(v);
        }
        else if (g->gclass == 0 && res.GetFrame(gtype, 0))
            objects.push_back({ iy + 1, ix, land.ProjectY(iy, land.HeightAt(ix, iy)), nullptr, nullptr, g->paletteSlot, gtype });
    }

    // Each renderer's key is its +0x1E (the object's y, 0x47F516) + its +0x26, as the original's view list sorts them
    // (0x40D750), worked out each update after the poses:
    //   a unit's own WOF: +0x26 half the posed model's depth extent (0x47F340, every pose); on a bridge link the
    //     bridge adds 0x30 to +0x1E and 0x40 to +0x26 (0x491730 -> 0x4984C0), so a rider draws over the deck;
    //   a turret: the hull's +0x1E + 1 and +0x26 + 1 (0x489AE0), just after its hull;
    //   a shadow: the hull's less 1 each (0x48E850), only while the hull's height is 0 or more (else it stays);
    //   another WOF part: its own place and posed depth;
    //   a mine's or sinker's marker: +0x1E never set (0x4650) and +0x26 0x20 (0x48CD69, 0x499320), so after
    //     everything, at the object's +0x2C / +0x30, its point raised by its type's +0x41 (0x47F480 / 0x4406A0);
    //   the rest (IOB models, building parts, corners): the unit's y + their own depth.
    void BattleMap::RekeyViews()
    {
        struct Hull { int y = 0, depth = 0; bool set = false; };
        std::vector<Hull> hulls(units.size());
        for (ViewObject& o : objects)
        {
            if (o.unit < 0 || size_t(o.unit) >= units.size() || o.light >= 0) continue;
            const Unit& u = units[size_t(o.unit)];
            if (!o.wof || o.wof != u.wof || o.part >= 0) continue;
            int y = int(u.y), depth = o.wof->PosedDepth();
            if (u.drive.link) { y += 0x30; depth += 0x40; }
            o.depth = depth;
            o.key = y + depth;
            hulls[size_t(o.unit)] = { y, depth, true };
        }
        for (ViewObject& o : objects)
        {
            if (o.unit < 0 || size_t(o.unit) >= units.size() || o.light >= 0 || o.effect >= 0) continue;
            const Unit& u = units[size_t(o.unit)];
            const Hull& h = hulls[size_t(o.unit)];
            if (o.sprite == 0x359 || o.sprite == 0x35A)
            {
                const int ix = int(u.x), iy = int(u.y);
                int raise = 0;
                if (auto t = setup.units->find(u.type); t != setup.units->end()) raise = t->second.size41;
                o.x = ix;
                o.y = land.ProjectY(iy, land.HeightAt(ix, iy) + int(u.drive.lift)) - raise;
                o.depth = 0x20;
                o.key = 0x4650 + 0x20;
                continue;
            }
            if (o.wof && o.wof == u.wof && o.part < 0) continue;
            if (o.wof && o.part >= 0)
            {
                const Attached* a = nullptr;
                for (const Attached& at : u.attached)
                    if (at.part == o.part) { a = &at; break; }
                if (a && a->shadow)
                {
                    if (!h.set || u.drive.lift < 0.0f) continue;
                    o.depth = h.depth - 1;
                    o.key = h.y - 1 + o.depth;
                }
                else if (a && a->turret >= 0 && h.set)
                {
                    o.depth = h.depth + 1;
                    o.key = h.y + 1 + o.depth;
                }
                else
                {
                    o.depth = o.wof->PosedDepth();
                    o.key = o.wof->WorldY() + o.depth;
                }
                continue;
            }
            o.key = int(u.y) + o.depth;
        }
        // the renderer's level (vt+0x5C, from each object's update 0x48B3D0): ftol(the fog level there (0x48B050) x
        // 255) (0x48B2A0, 0x48B2D0, WScenery's 0x4897D0), or x its type's +0xA1 for a street lamp (type 0x110,
        // 0x48F2E0) or type 0x1C0 (0x48D000); a sprite keeps it >> 2 (0x40F040). The flying objects' vt+0x5C does
        // nothing (nullsub_3), nor do the lights'.
        std::vector<int> unitBright(units.size(), 255);
        for (size_t i = 0; i < units.size(); i++) unitBright[i] = int(FogLevelAt(units[i].type, units[i].x, units[i].y) * 255.0f);
        for (ViewObject& o : objects)
        {
            if (o.light >= 0 || o.effect >= 0) continue;
            if (o.unit >= 0 && size_t(o.unit) < units.size())
            {
                if (o.sprite == 0x359 || o.sprite == 0x35A) continue;   // the markers are renderers of their own
                o.bright = unitBright[size_t(o.unit)];
                continue;
            }
            if (o.prop >= 0 && size_t(o.prop) < props.size())
            {
                const Prop& p = props[size_t(o.prop)];
                int of = 255;
                if (p.type == 0x110 || p.type == 0x1C0)
                    if (auto t = setup.units->find(p.type); t != setup.units->end()) of = t->second.fieldA1 & 0xFF;
                o.bright = int(FogLevelAt(p.type, p.x, p.y) * float(of));
                if (o.sprite) o.level = o.bright >> 2;
            }
            else if (o.wof && o.unit < 0)
            {
                // a bridge's segments (types 0x1CC-0x1CE) are objects of type 0x1C0's class (the factory's 0x447541,
                // 0x48CFB0), children of the bridge each at its own point: their vt+0x5C is that class's (0x48D000),
                // the fog level round the point x the type's +0xA1 (150 for the three), so a deck is drawn at
                // 150 / 255 of a vehicle's brightness at best
                for (const BridgeLink& b : bridgeLinks)
                    for (size_t k = 0; k < b.segments.size() && k < b.segmentAt.size(); k++)
                        if (b.segments[k] == o.wof)
                        {
                            int of = 255;
                            if (auto t = setup.units->find(b.segmentType); t != setup.units->end()) of = t->second.fieldA1 & 0xFF;
                            o.bright = int(FogLevelAt(b.segmentType, float(b.segmentAt[k].first), float(b.segmentAt[k].second)) * float(of));
                        }
            }
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    float BattleMap::Progress(const Unit& u) const
    {
        if (u.ref < 0 || size_t(u.ref) >= setup.groups.size()) return 1.0f;
        return setup.groups[size_t(u.ref)].progress;
    }

    int BattleMap::AddGroup(bool players)
    {
        const int g = int(groupTargets.size());
        groupPlayers.push_back(players);
        selected.push_back(false);
        groupStance.push_back(1);
        groupTargets.push_back(GroupTarget{});
        groupMode.push_back(0);
        groupRange.push_back(100000);
        groupTicks.push_back(0);
        groupInterval.push_back(90);
        groupHomeX.push_back(0);
        groupHomeY.push_back(0);
        groupBusy.push_back(false);
        groupHalted.push_back(false);
        groupOrdered.push_back(false);
        groupArrived.push_back(0);
        groupSelfMove.push_back(false);
        grounds.push_back(GroundGroup{});
        return g;
    }

    // 0x4802E0: +0x4E the engagement range, from 100000 the least of each member's sight (+0x48) and its vt+0xB0, its
    // guns' longest record 12 +0 range, or 10000 with none (0x495850); +0x5C the think interval, its members' type
    // +0x92 words summed over the count (ftol; 0x480533, 0x4805B5-0x4805E1); +0x54 the stance, the members' +0xD8
    // averaged (ftol(sum + 0.5) / n) once they've started; and the ground costs (SetUpGrounds)
    void BattleMap::RecomputeGroup(int group)
    {
        if (group < 0 || size_t(group) >= groupRange.size()) return;
        int range = 100000, sum = 0, n = 0, stances = 0;
        for (const Unit& u : units)
        {
            if (u.group != group) continue;
            auto t = setup.units->find(u.type);
            if (t == setup.units->end()) continue;
            // the range only from the vehicle members (+0xB8 bit 0, 0x48043A-0x48050E)
            if (u.vehicle)
            {
                int r = u.guns.empty() ? 10000 : 0;
                for (const Gun& g : u.guns) r = std::max(r, WeaponData(g.type)[0]);
                range = std::min({ range, ObjOjd::Vision(t->second, setup.armour), r });
            }
            sum += int16_t(t->second.field92);
            stances += u.stance;
            n++;
        }
        groupRange[size_t(group)] = int16_t(range);    // a word (+0x4E): a group with no vehicle keeps 100000 as 0x86A0
        if (n) groupInterval[size_t(group)] = sum / n;
        if (n && size_t(group) >= setup.groups.size()) groupStance[size_t(group)] = int(double(stances) + 0.5) / n;
    }

    // 0x422910 / 0x483F90 / 0x484070: an icon for the first of the list's objects that is the sector's command
    // building, then for each of its objects of category 1 (its vehicles), in the list's order; one whose object has
    // left the battle is taken out (0x483FF0) and the rest laid out again. The layout: from 32, 32 (the windowed
    // game's dword_4BA05C in place of 32), each icon its width + 2 on from the last; one that would pass the
    // window's width less 32 starts a new row, the height + 2 down.
    // The icon is a window of GType 0x76's size showing the type's war map counter frame (+0x2C, 0x40F000).
    void BattleMap::LayOutGroupIcons() const
    {
        std::vector<GroupIcon> icons;
        if (!setup.units) { groupIcons.clear(); return; }
        auto add = [&](size_t gi) {
            const BattleGroup& g = setup.groups[gi];
            const bool alive = std::any_of(units.begin(), units.end(), [&](const Unit& u) { return u.ref == int(gi) && u.players; });
            if (!alive) return;
            GroupIcon icon;
            icon.ref = int(gi);
            auto t = setup.units->find(g.type);
            icon.frame = t == setup.units->end() ? 0 : t->second.counterFrame;
            const IndexedImage* image = res.GetFrame(0x76, icon.frame);
            icon.w = image ? image->width : 32;
            icon.h = image ? image->height : 32;
            for (const GroupIcon& old : groupIcons)
                if (old.ref == icon.ref) icon.clicked = old.clicked;
            icons.push_back(icon);
        };
        // the list is the player's units' (the world's +0x4C as the battle is set up, 0x439A7E)
        int list = -1;
        for (const BattleGroup& g : setup.groups)
            if (g.Players()) { auto t = setup.units->find(g.type); if (t != setup.units->end() && t->second.category == 1) { list = g.list; break; } }
        for (size_t gi = 0; gi < setup.groups.size() && list >= 0; gi++)
        {
            const BattleGroup& g = setup.groups[gi];
            if (g.list != list || !g.Players()) continue;
            if (g.type == deploy::MartianHq(setup.sector) || g.type == deploy::HumanHq(setup.sector)) { add(gi); break; }
        }
        for (size_t gi = 0; gi < setup.groups.size() && list >= 0; gi++)
        {
            const BattleGroup& g = setup.groups[gi];
            if (g.list != list || !g.Players()) continue;
            auto t = setup.units->find(g.type);
            if (t != setup.units->end() && t->second.category == 1) add(gi);
        }
        const int start = 32;
        int x = start, y = start;
        for (GroupIcon& icon : icons)
        {
            if (x + icon.w + 2 > 640 - start) { x = start; y += icon.h + 2; }
            icon.x = x;
            icon.y = y;
            x += icon.w + 2;
        }
        groupIcons = std::move(icons);
    }

    int BattleMap::GroupIconAt(int x, int y) const
    {
        LayOutGroupIcons();     // (the icons are windows in the game; here they're laid out as they're asked for)
        for (size_t i = 0; i < groupIcons.size(); i++)
        {
            const GroupIcon& g = groupIcons[i];
            if (x >= g.x && y >= g.y && x < g.x + g.w && y < g.y + g.h) return int(i);
        }
        return -1;
    }

    // 0x484920, the left button let go on an icon: its object selected alone (0x8088 after the selection's cleared);
    // in cursor mode 1, on one already selected, a second click under 10 frames after the last centres the view on
    // the selection instead (0x43A480).
    void BattleMap::ClickGroupIcon(size_t index)
    {
        GroupIcon& icon = groupIcons[index];
        const int since = Frame() - icon.clicked;
        icon.clicked = Frame();
        const bool isSelected = std::any_of(units.begin(), units.end(), [&](const Unit& u) { return u.ref == icon.ref && u.players && selected[size_t(u.group)]; });
        if (Mode() == 1 && isSelected)
        {
            if (since > 0 && since < 10) CentreOnSelection();
            return;
        }
        if (Mode() != 0 && Mode() != 1) return;
        SelectRefs({ icon.ref });
        for (size_t g = 0; g < selected.size(); g++) if (selected[g]) { SelectionSpeech(int(g)); break; }
    }

    // 0x4842E0: the icon; with its object selected (0x8012 to the icon, 0x484840) the four corners of GType 5 8 in
    // from its corners; under it the strength pips (0x449150 / 0x449340): one for each of the object's units, GType
    // 8 (hwpower.spr) at frame ftol(0.5 + 9 x its health / its type's), each the sprite's width - 1 on from the
    // last, the row centred under the icon.
    // *Provisional*: the icon's lit state (its renderer's level 1 under the mouse, and flashing while +0x94 counts
    // down) isn't drawn; the pips' row is placed by eye against the original (its sprite's own offsets aren't read).
    void BattleMap::DrawGroupIcons(Canvas& canvas) const
    {
        LayOutGroupIcons();
        for (const GroupIcon& icon : groupIcons)
        {
            canvas.DrawSprite(0x76, icon.frame, icon.x, icon.y);
            bool isSelected = false;
            std::vector<int> frames;
            for (const Unit& u : units)
            {
                if (u.ref != icon.ref || !u.players) continue;
                if (selected[size_t(u.group)]) isSelected = true;
                const int max = MaxHealth(u.type);
                frames.push_back(max > 0 ? int(0.5 - double(float(u.health) / float(max)) * -9.0) : 0);
            }
            if (isSelected)
            {
                canvas.DrawSprite(5, 0, icon.x + 8, icon.y + 8);
                canvas.DrawSprite(5, 1, icon.x + icon.w - 8, icon.y + 8);
                canvas.DrawSprite(5, 2, icon.x + 8, icon.y + icon.h - 8);
                canvas.DrawSprite(5, 3, icon.x + icon.w - 8, icon.y + icon.h - 8);
            }
            const IndexedImage* pip = res.GetFrame(8, 0);
            if (!pip || frames.empty()) continue;
            const int step = pip->width - 1;
            const int count = int(std::min<size_t>(frames.size(), 8));
            const int x0 = icon.x + icon.w / 2 - (step * count) / 2;
            const int y0 = icon.y + icon.h - 3;     // (by eye: on the original's rows in the owner's frames)
            for (int i = 0; i < count; i++) canvas.DrawSprite(8, frames[size_t(i)], x0 + i * step, y0);
        }
    }

    void BattleMap::SelectRefs(const std::vector<int>& refs)
    {
        ClearSelection();
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].players && std::find(refs.begin(), refs.end(), units[i].ref) != refs.end()) members.push_back(i);
        if (members.empty()) return;
        // already one whole group: it's the selection (0x4480F0 moves nothing)
        const int first = units[members[0]].group;
        const bool same = std::all_of(members.begin(), members.end(), [&](size_t i) { return units[i].group == first; }) &&
                          GroupCount(first) == int(members.size());
        if (same) { selected[size_t(first)] = true; return; }
        const int g = AddGroup(true);
        std::vector<int> old;
        for (size_t i : members)
        {
            if (std::find(old.begin(), old.end(), units[i].group) == old.end()) old.push_back(units[i].group);
            units[i].group = g;
        }
        RecomputeGroup(g);
        for (int o : old) RecomputeGroup(o);
        SetUpGrounds();
        selected[size_t(g)] = true;
    }

    // 0x48E9D0, for a building not yet finished (+0xC0 bit 0 clear) of the player's side (+0x24 0): four type 0x17B
    // objects (WEffect 0x487730, its plain kind) made children (0x47D190, after its parts, so a hit takes them
    // first) at (-h, -h), (h, -h), (-h, h) and (h, h) from it, h its type's +0x46 word, each drawn at its type's
    // +0xA1 level. They go with the building.
    void BattleMap::AddCorners(size_t index)
    {
        Unit& u = units[index];
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const int h = Radius46(u.type);
        const int at[4][2] = { { -h, -h }, { h, -h }, { -h, h }, { h, h } };
        const int first = int(t->second.parts.size());
        for (int k = 0; k < 4; k++)
        {
            const size_t before = objects.size();
            AddRenderer(0x17B, u.x + float(at[k][0]), u.y + float(at[k][1]), 0, int(index));
            for (size_t o = before; o < objects.size(); o++)
            {
                objects[o].part = first + k;
                objects[o].depth = objects[o].key - int(u.y);
                // at its type's +0xA1 (0x48EC7A: the renderer's vt+0x18 with the byte, 180 >> 2 = 45)
                if (auto ct = setup.units->find(0x17B); ct != setup.units->end()) objects[o].level = (ct->second.fieldA1 & 0xFF) >> 2;
            }
            Attached a;
            a.part = first + k;
            a.ox = at[k][0];
            a.oy = at[k][1];
            u.attached.push_back(a);
            u.partsLeft++;
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    void BattleMap::Begin()
    {
        if (begun) return;
        begun = true;
        // each building's own sound started as it's added (0x48E9D0)
        for (Unit& u : units)
            if (IsBuilding(u)) BuildingSound(u);
        // the pod's blasts were set off as the battle was built (behind the Briefing): heard now
        if (setup.sound)
            for (const auto& [id, x, y] : heldSounds) setup.sound->Play(id, 1.0f, 0, x, y);
        heldSounds.clear();
    }

    const BattleMap::IsoModel* BattleMap::LoadIob(int gtype)
    {
        auto m = models.find(gtype);
        if (m == models.end())
        {
            IsoModel im;
            if (const GType* g = res.FindGType(gtype))
            {
                if (auto file = res.LoadFile(g->file))
                {
                    try { im.model = IobModel::Load(*file); } catch (const std::exception&) { im.model = {}; }
                    if (!im.model.patches.empty()) im.lights = IsoObject::BaseLights(im.model, sun, levels);
                }
                im.slot = g->paletteSlot;
            }
            m = models.emplace(gtype, std::move(im)).first;
        }
        return m->second.model.patches.empty() ? nullptr : &m->second;
    }

    // A pod landed since the last battle (the sector's +0x64, set by 0x41A330 when it came down among the Humans):
    // EnterBattleMap (0x439AE5) makes one (type 0xB3: WPod 0x48F590, Pod.iob), adds it and starts it, then clears
    // the flag. As it starts (0x48F620) it picks a place at random, x rand() % the world's width and y rand() % its
    // height, until a vehicle could stand on every vertex within its radius (+0xAC, 200: 0x47B520); it's put there
    // at height 0, and 0x3DA goes off there twice, as its type has it (0x488070) and again with radius and power 300
    // (0x4880F0); then WScenery's start (0x4896D0).
    // *Provisional*: the explosions' firer (the pod's type at +0xC4) isn't kept; its shadow parts and lights
    // aren't added; a hundred tries at most.
    void BattleMap::LandPod(const Landscape::Sun& sun)
    {
        constexpr int Pod = 0xB3;
        auto u = setup.units->find(Pod);
        if (u == setup.units->end()) return;
        NoteModel(Pod, false);      // Pod.iob: its +0xAC is its model's radius (0x47EB10)
        const int radius = Radius44(Pod);
        const int r = (int(uint32_t(radius) << 16) + land.HalfCell()) / land.Cell();
        const int worldH = int((int64_t(land.Cell()) * land.Height()) >> 16);
        float x = 0, y = 0;
        bool found = false;
        for (int tries = 0; tries < 100 && !found; tries++)
        {
            x = float(random::Rand() % std::max(1, land.WorldWidth()));
            y = float(random::Rand() % std::max(1, worldH));
            const int vx = (int(uint32_t(int(x)) << 16) + land.HalfCell()) / land.Cell();
            const int vy = (int(uint32_t(int(y)) << 16) + land.HalfCell()) / land.Cell();
            found = land.IsAreaPassable(vx - r, vy - r, vx + r, vy + r);
        }
        if (!found) return;
        podX = int(x);
        podY = int(y);
        // the blasts first: the pod isn't in the world until its start (0x4896D0), so they don't reach it
        Explode(0x3DA, int(x), int(y), 0);
        Explode(0x3DA, int(x), int(y), 0, -1, 0, 0, 300, 300);
        const GType* g = res.FindGType(u->second.field28);
        if (g && g->gclass == 1)
        {
            auto m = models.find(u->second.field28);
            if (m == models.end())
            {
                IsoModel im;
                if (auto file = res.LoadFile(g->file))
                    try
                    {
                        im.model = IobModel::Load(*file);
                        im.lights = IsoObject::BaseLights(im.model, sun, levels);
                        im.slot = g->paletteSlot;
                    }
                    catch (const std::exception&) {}
                m = models.emplace(u->second.field28, std::move(im)).first;
            }
            if (!m->second.model.patches.empty())
            {
                const int ix = int(x), iy = int(y);
                props.push_back({ Pod, x, y, 0.0f, MaxHealth(Pod) });
                objects.push_back({ iy + IsoObject::Depth(m->second.model), ix, land.ProjectY(iy, land.HeightAt(ix, iy)), &m->second, nullptr, 0, 0,
                                    -1, -1, 0, -1, int(props.size()) - 1 });
                std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
            }
        }
        Occupy(Pod, x, y);
        land.SetPointLight(int(x), int(y), sun);
    }

    // EnterBattleMap's scenery (0x439770 with [Debug] "Load scenery"): dat\<sector>.nsb, placed as the game does
    // (core/Scenery.h). The objects with an RTypeISO model (GType class 1) and the bridges (core/Bridge.h) are drawn;
    // *not yet*: the sprites (class 0, 14: lamps, the buildings' shadows) and the ambient sound scenery.
    void BattleMap::LoadScenery(const Landscape::Sun& sun, char)
    {
        auto data = res.LoadFile("dat\\" + std::to_string(setup.sector) + ".nsb");
        if (!data || !setup.units) return;
        std::vector<SceneryObject> list;
        try { list = wow::LoadScenery(*data); }
        catch (const std::exception&) { return; }
        // WScenery::Load (0x4895E0, 0x489653): an object off the map (x past the world's width, y past its height, or
        // either under 0) is put at (0, 0), both at once
        {
            const int worldH = int((int64_t(land.Height()) * land.Cell()) >> 16);
            for (SceneryObject& o : list)
                if (o.x < 0 || o.y < 0 || o.x >= land.WorldWidth() || o.y >= worldH) { o.x = 0; o.y = 0; }
        }
        auto radius = [&](int type) {
            auto u = setup.units->find(type);
            return u == setup.units->end() ? 0 : int(int16_t(u->second.hitPoints));
        };
        const int worldW = land.WorldWidth(), worldH = land.Height() * land.Cell() >> 16;
        // its shadow parts (types 0x14B-0x14D, 0x48E2A0): as one starts (0x48E460) it lays its 32 x 32 mask (the
        // GType's .raw) on the ground beside the object, away from the sun: 2 x the half width plus 10 x |cos| x
        // its scale wide, the depth / sqrt 2 either side of y, mirrored when the sun's cos is 0 or less (0x47C900,
        // the moving light the ground view takes off). The half width and depth are the parent renderer's (+0x28
        // to +0x2A halved, +0x26).
        // WScenery's start (0x4896D0): after its parts, every object of a category 0, 2 or 3 type (the scenery is
        // all category 0, so all of it: 0x47D828 sets +0xB8 bit 1) puts the sun's point level (SetPointLight) on the
        // vertex it stands on, a dark spot the ground view takes off. The tree circles do too, at their centres.
        // *Provisional*: done here for the objects drawn (sprites and IOB models) and the circles; the classes of
        // the others (the ambient sound scenery) aren't checked.
        circles.clear();
        for (const SceneryObject& c : list)
            if (c.type >= 0xCF && c.type <= 0xD4)
            {
                land.SetPointLight(c.x, c.y, sun);
                circles.push_back({ c.type, c.x, c.y, true, MaxHealth(c.type) });
            }
        // the light parts (types 334-346, 0x48DD50) and which types have one
        auto lightPart = [&](int type) -> const ObjOjd::UnitData::Part* {
            auto u = setup.units->find(type);
            if (u == setup.units->end()) return nullptr;
            for (const auto& part : u->second.parts)
                if (part.a >= 334 && part.a <= 346) return &part;
            return nullptr;
        };
        auto category = [&](int type) {
            auto u = setup.units->find(type);
            return u == setup.units->end() ? -1 : u->second.category;
        };
        auto rolls = [&](int type) { const int c = category(type); return lightPart(type) && c >= 0 && c <= 3; };
        // 0x48DE80, as the light starts: for a parent of category 0 to 3 it stays when rand() % 100 is at most 100 x
        // the hour's fourth float (flt_4DA768: 1 at night, 0 by day) and the parent isn't Martian; then only with
        // "Show lights" on. It's raised 16 (+0xA0), and lights the ground (RTypeLight, 0x4A2980) with record 13's
        // float as its intensity (0x48E050) out to its type's +0x44 word.
        const float chance = setup.dayTime ? setup.dayTime->sets[0].f[3] : 0.0f;
        auto addLight = [&](const PlacedObject& o, const ObjOjd::UnitData& parent) {
            const ObjOjd::UnitData::Part* part = lightPart(o.type);
            if (!part) return;
            const int c = parent.category;
            if (c >= 0 && c <= 3)
            {
                if (double(chance) * 100.0 < double(o.roll)) return;
                if (parent.faction == 1) return;
            }
            if (!setup.showLights) return;
            auto pu = setup.units->find(part->a);
            if (pu == setup.units->end()) return;
            float intensity = 0;
            if (setup.lights)
                if (auto t = setup.lights->find(part->a); t != setup.lights->end()) intensity = t->second.f;
            const int range = int(int16_t(pu->second.hitPoints));
            const int x = int(o.x) + int16_t(part->b), y = int(o.y) + int16_t(part->c);
            const int z = land.HeightAt(x, y) + int(o.z) + int16_t(part->d) + 16;
            objects.push_back({ y + (-32 - range), x, y, nullptr, nullptr, 0, 0, int(lights.size()) });
            lights.push_back({ x, y, z, range, intensity });
        };
        for (const PlacedObject& o : PlaceScenery(list, setup.enemyCount, radius, rolls))
        {
            auto u = setup.units->find(o.type);
            if (u == setup.units->end()) continue;
            if (o.type >= 445 && o.type <= 447) { Occupy(o.type, o.x, o.y, o.tree); PlaceBridge(o); continue; }
            const int gtype = u->second.field28;
            const GType* g = res.FindGType(gtype);
            if (!g || (g->gclass != 0 && g->gclass != 1)) { Occupy(o.type, o.x, o.y, o.tree); continue; }
            if (g->gclass == 0)
            {
                // a sprite (RType 0x40EC50, the street lamps): drawn from its hot spot at the renderer's point, its
                // +0x26 1; the half width its shadow takes is half the sprite's width
                const IndexedImage* image = res.GetFrame(gtype, 0);
                Occupy(o.type, o.x, o.y, o.tree);
                if (!image) continue;
                const int ix = int(o.x), iy = int(o.y);
                const int h = int(float(land.HeightAt(ix, iy)) + o.z);
                props.push_back({ o.type, o.x, o.y, o.z, MaxHealth(o.type) });
                props.back().circle = o.circle;
                props.back().tree = o.tree;
                objects.push_back({ iy + 1, ix, land.ProjectY(iy, h), nullptr, nullptr, g->paletteSlot, gtype, -1, -1, 0, -1, int(props.size()) - 1 });
                // its level (+0x34, the sprite's vt+0x18 0x40F040 keeps the argument >> 2): WScenery's 0x4897D0 gives
                // ftol(255 x the fog level), a street lamp's (type 0x110, 0x48F2E0) and type 0x1C0's (0x48D000)
                // ftol(its type's +0xA1 x it); 1.0 out of the fog. (Was 0, the darkest: the lamps looked black.)
                objects.back().level = (o.type == 0x110 || o.type == 0x1C0 ? (u->second.fieldA1 & 0xFF) : 255) >> 2;
                LayShadows(u->second, o.x, o.y, image->width / 2, 1);
                if (const size_t before = lights.size(); addLight(o, u->second), lights.size() > before) props.back().light = int(before);
                land.SetPointLight(ix, iy, sun);
                continue;
            }
            auto m = models.find(gtype);
            if (m == models.end())
            {
                IsoModel im;
                auto file = res.LoadFile(g->file);
                if (!file) { models[gtype]; Occupy(o.type, o.x, o.y, o.tree); continue; }
                try { im.model = IobModel::Load(*file); }
                catch (const std::exception&) { models[gtype]; Occupy(o.type, o.x, o.y, o.tree); continue; }
                im.lights = IsoObject::BaseLights(im.model, sun, levels);
                im.slot = g->paletteSlot;
                m = models.emplace(gtype, std::move(im)).first;
            }
            if (m->second.model.patches.empty()) { Occupy(o.type, o.x, o.y, o.tree); continue; }
            const IobModel& model = m->second.model;
            // 0x47EB10: the collision radius, max(2 x half width, depth) x [Tweak] "ISO obj col rad mul" (0.66);
            // 0x47F960 keeps the object that far inside the map
            const int r = int(float(std::max(2 * IsoObject::HalfWidth(model), IsoObject::Depth(model))) * 0.66f);
            // its type's +0x44 from then on, and its +0x46 for the scenery classes (vt+0xB8)
            NoteModel(o.type, SceneryClass(o.type));
            float x = o.x, y = o.y;
            if (float(worldW - r) < x) x = float(worldW - r);
            if (float(worldH - r) < y) y = float(worldH - r);
            if (float(r) > x) x = float(r);
            if (float(r) > y) y = float(r);
            Occupy(o.type, x, y, o.tree);
            // 0x47F480: the renderer's point, the ground's height there plus the object's own (+0x80)
            const int ix = int(x), iy = int(y);
            const int h = int(float(land.HeightAt(ix, iy)) + o.z);
            props.push_back({ o.type, x, y, o.z, MaxHealth(o.type) });
            props.back().circle = o.circle;
            props.back().tree = o.tree;
            objects.push_back({ iy + IsoObject::Depth(model), ix, land.ProjectY(iy, h), &m->second, nullptr, 0, 0, -1, -1, 0, -1, int(props.size()) - 1 });
            LayShadows(u->second, x, y, (IsoObject::HalfWidth(model) - -IsoObject::HalfWidth(model)) / 2, IsoObject::Depth(model));
            if (const size_t before = lights.size(); addLight(PlacedObject{ o.type, x, y, o.z, {}, o.roll }, u->second), lights.size() > before)
                props.back().light = int(before);
            land.SetPointLight(ix, iy, sun);
        }
        // ActivateAll (0x47D030), after each object's start: one that answers vt+0x64 is taken out again (vt+0x78,
        // 0x4895A0, no explosion). For WScenery (0x489380), the street lamp (0x48F290), the pod's class (0x48F590:
        // 0xB3, 0xCB-0xCE) and the tree circles vt+0x64 is 0x489750: whether the sector's Damage Layer (the
        // world's +0x8C) is anything but 0 at the vertex nearest it. Scenery standing on ground that earlier
        // battles scorched doesn't come back. A circle goes with its trees. (The other classes' vt+0x64 is
        // 0x48B580, never: the units and buildings among them.)
        {
            const std::vector<uint8_t>& layer = land.Scorched();
            const int w = land.Width(), h = land.Height();
            auto damaged = [&](int x, int y) {
                if (layer.size() != size_t(w) * size_t(h)) return false;
                const int vx = std::clamp((int(uint32_t(x) << 16) + land.HalfCell()) / land.Cell(), 0, w - 1);
                const int vy = std::clamp((int(uint32_t(y) << 16) + land.HalfCell()) / land.Cell(), 0, h - 1);
                return layer[size_t(vy) * size_t(w) + size_t(vx)] != 0;
            };
            for (size_t c = 0; c < circles.size(); c++)
                if (circles[c].alive && damaged(circles[c].x, circles[c].y))
                {
                    circles[c].alive = false;
                    for (size_t i = 0; i < props.size(); i++)
                        if (props[i].alive && props[i].circle == int(c)) RemoveProp(i);
                }
            for (size_t i = 0; i < props.size(); i++)
            {
                const int t = props[i].type;
                const bool asks = (t >= 0xD5 && t <= 0x10F) || (t >= 0x111 && t <= 0x115) || (t >= 721 && t <= 741) ||
                                  (t >= 845 && t <= 904) || (t >= 973 && t <= 985) || t == 0x110 || t == 0xB3 || (t >= 0xCB && t <= 0xCE);
                if (props[i].alive && props[i].circle < 0 && asks && damaged(int(props[i].x), int(props[i].y))) RemoveProp(i);
            }
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    // The shadow parts of an object (types 0x14B-0x14D, 0x48E2A0), laid as they start (0x48E460): see LoadScenery.
    // The half width and depth are the parent renderer's (+0x28 to +0x2A halved, +0x26) as they stand then.
    void BattleMap::LayShadows(const ObjOjd::UnitData& unit, float x, float y, int halfWidth, int depth)
    {
        for (const auto& part : unit.parts)
        {
            if (part.a < 0x14B || part.a > 0x14D) continue;
            auto pu = setup.units->find(part.a);
            if (pu == setup.units->end()) continue;
            const GType* pg = res.FindGType(pu->second.field28);
            if (!pg) continue;
            auto mask = masks.find(pg->file);
            if (mask == masks.end())
            {
                auto data = res.LoadFile(pg->file);
                mask = masks.emplace(pg->file, data && data->size() >= 1024 ? *data : std::vector<uint8_t>{}).first;
            }
            if (mask->second.empty()) continue;
            const float px = x + float(int16_t(part.b)), py = y + float(int16_t(part.c));
            const float scale = part.f * pu->second.scale; // the part's, then its type's (0x47EE10)
            const int hw = halfWidth;
            const int across = int(double(2 * hw) - double(std::fabs(sun.across)) * double(scale) * -10.0);
            const int dy = (depth * 362) >> 9;
            const int y0 = int(py - float(dy)), y1 = int(py + float(dy));
            int x0, x1;
            const bool flip = !(sun.across > 0.0f);
            if (!flip) { x1 = int(px + float(hw)); x0 = x1 - across; }
            else { x0 = int(px - float(hw)); x1 = x0 + across; }
            land.ProjectLight(mask->second.data(), x0 - 8, y0 - 8, x1, y1, flip, sun);
        }
    }

    // vt+0xE4 (0x48A950) as an object is added to the world (0x48A8B0): an object whose type is of class 0, 2 or 3
    // (+0xB8 bit 1, 0x47D828) and not a tree (made as 0xCF-0xE5, bit 2: a tree made burnt too) marks the vertices within its type's +0x46 word of
    // it blocked (0x47C280 with 2: QuickDistance from the vertex nearest it, in whole cells), which the ground's
    // cost (0x480680) then refuses.
    // An IOB object's +0x46 is its model's radius when it has a parent or is of a scenery class (Radius46); taken
    // off again as it goes (vt+0xE8, 0x48A9A0).
    void BattleMap::Occupy(int type, float x, float y, bool tree)
    {
        if (tree || (type >= 0xCF && type <= 0xE5)) return;
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return;
        const int c = u->second.category;
        if (c != 0 && c != 2 && c != 3) return;
        land.MarkOccupied(int(x), int(y), Radius46(type), 2);
    }

    // A bridge (core/Bridge.h): its part, the segment model (Country 445 bridge_1, City 446 bridge_2, Railway 447
    // bridge_4), once for each segment, each posed at its point with the bridge's heading (0x497900), then its ends
    // rebuilt from the joints either side (0x498110): joint i is segment i's start and segment i - 1's end. The game
    // redoes the joints every update; the ground under a bridge doesn't change yet, so here it's done once.
    void BattleMap::PlaceBridge(const PlacedObject& o)
    {
        auto u = setup.units->find(o.type);
        const GType* g = nullptr;
        int gtype = 0;
        int segmentType = 0;
        float objectScale = 1.0f;
        for (const auto& part : u->second.parts)
        {
            auto pu = setup.units->find(part.a);
            if (pu == setup.units->end()) continue;
            const GType* pg = res.FindGType(pu->second.field28);
            // the segment's own scale (+0xA4): the part's float (0x47EEC0) times its type's (+0xA8, 0x47EE24)
            if (pg && pg->gclass == 2) { segmentType = part.a; g = pg; gtype = pu->second.field28; objectScale = part.f * pu->second.scale; break; }
        }
        if (!g) return;
        auto file = res.LoadFile(g->file);
        if (!file) return;
        float scale = 65536.0f;
        int follow = 0;
        if (setup.six)
            if (auto six = setup.six->find(gtype); six != setup.six->end())
            {
                scale = six->second.a;
                follow = six->second.c;
            }
        // the model's scales are ftol(ftol(the GType's float) x the object's) (0x47ED08), as a unit's parts'
        // (*provisional*: the segments made after the first, 0x497900, are given the first's)
        scale = float(int(float(int(scale)) * objectScale));
        const Bridge bridge = Bridge::Create(land, o.extra[0], o.extra[1], o.extra[2], o.extra[3]);
        AddBridgeNodes(bridge);
        const size_t link = bridgeLinks.size() - 1;
        bridgeLinks[link].type = o.type;
        bridgeLinks[link].x = int(o.x);
        bridgeLinks[link].y = int(o.y);
        bridgeLinks[link].health = MaxHealth(o.type);
        bridgeLinks[link].segmentType = segmentType;
        const size_t first = wofs.size();
        for (int i = 0; i < bridge.Count(); i++)
        {
            int x = 0, y = 0;
            bridge.SegmentAt(i, o.x, o.y, x, y);
            std::unique_ptr<WofInstance> w;
            try { w = std::make_unique<WofInstance>(*file, scale, follow); }
            catch (const std::exception&) { break; }
            w->Transform(land, bridge.Heading(), x, y, land.HeightAt(x, y));
            // posed as a child (0x4981B0 -> 0x47F340): its +0x26 half the posed segment's depth extent, before the
            // stitching, not record 6's
            const int segDepth = w->PosedDepth();
            objects.push_back({ y + segDepth, 0, 0, nullptr, w.get(), g->paletteSlot });
            bridgeLinks[link].segments.push_back(w.get());
            bridgeLinks[link].segmentAt.push_back({ x, y });
            wofs.push_back(std::move(w));
        }
        const int made = int(wofs.size() - first);
        for (int i = 0; i <= made; i++)
        {
            const BridgeJoint j = bridge.Joint(land, bridge.JointDistance(i), i);
            if (i > 0) wofs[first + size_t(i) - 1]->Stitch(j, true);
            if (i < made) wofs[first + size_t(i)]->Stitch(j, false);
        }
    }

    // The view's scroll (0x42F6F0 through the camera, 0x40E5A0): 0 to the world's size (the landscape's +0x1C and
    // +0x20, 0x484DA4) less the view's, less 1
    void BattleMap::ScrollTo(int x, int y)
    {
        scrollX = std::max(0, std::min(x, land.WorldWidth() - 640 - 1));
        scrollY = std::max(0, std::min(y, land.WorldHeight() - 480 - 1));
        // 0x8004 to the minimap: its rectangle at the scroll's share of the picture, kept inside it
        if (miniPicture)
        {
            rectX = std::max(0, miniW * scrollX / std::max(1, land.WorldWidth()));
            rectY = std::max(0, miniH * scrollY / std::max(1, land.WorldHeight()));
            if (rectW + rectX > miniW) rectX = miniW - rectW;
            if (rectH + rectY > miniH) rectY = miniH - rectH;
        }
    }

    // 0x485610: each press (and repeat) of a key is one step, sent as 0x7A with the view's new top left
    std::optional<Command> BattleMap::OnKey(int key)
    {
        if (endBox)
        {
            if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) return EndBoxCommand();
            return std::nullopt;
        }
        if (!popup.empty())
        {
            if (key == SDLK_ESCAPE) ClosePopup(true);
            return std::nullopt;
        }
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        const int32_t w = land.WorldWidth(), h = land.WorldHeight();
        // the steps as the compiler wrote them: h / 16 or h / 8; w / 7 or w / 3 (magic multiplies, rounded to 0)
        auto hi = [](int32_t a, int32_t m) { return int32_t((int64_t(a) * m) >> 32); };
        auto round0 = [](int32_t v) { return v + int32_t(uint32_t(v) >> 31); };
        switch (key)
        {
        case SDLK_ESCAPE: return Command{ MenuCommand, 0 };
        // Tab (0x80C8): the minimap hidden, or its dots made again and shown
        case SDLK_TAB:
            if (miniShown) miniShown = false;
            else { MakeDots(); miniShown = true; }
            break;
        case SDLK_PAGEUP: ScrollTo(scrollX, 0); break;
        case SDLK_PAGEDOWN: ScrollTo(scrollX, h); break;
        case SDLK_UP: ScrollTo(scrollX, scrollY - (ctrl ? h / 8 : h / 16)); break;
        case SDLK_DOWN: ScrollTo(scrollX, scrollY + (ctrl ? h / 8 : h / 16)); break;
        case SDLK_LEFT:
            ScrollTo(scrollX + (ctrl ? round0((hi(w, 0x55555555) - w) >> 1) : round0((hi(w, 0x6DB6DB6D) - w) >> 2)), scrollY);
            break;
        case SDLK_RIGHT:
            ScrollTo(scrollX + (ctrl ? round0(hi(w, 0x55555556)) : round0((hi(w, int32_t(0x92492493)) + w) >> 2)), scrollY);
            break;
        case SDLK_HOME: CentreOnSelection(); break;
        case SDLK_COMMA: if (const int g = StepObject(false); g >= 0) ViewOnGroup(g, false); break;
        case SDLK_PERIOD: if (const int g = StepObject(true); g >= 0) ViewOnGroup(g, true); break;
        case SDLK_BACKSPACE: Halt(); break;
        // The digits build packet 0x12 (select group n, 0 as 10), but the selection that runs the packets (0x4812B0)
        // has no case for it: in the game they do nothing, and so here.
        default: break;
        }
        return std::nullopt;
    }

    void BattleMap::OnKeyUp(int) {}

    void BattleMap::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
        if (miniDrag) MinimapScroll(x, y);
        // 0x200 with the button held (0x485D00 -> 0x49C250): the rectangle's far corner; it's a box once it's more
        // than 4 pixels from where it started either way (+0x18 / +0x1C)
        if (dragging)
        {
            dragX1 = x;
            dragY1 = y;
            if (std::abs(dragX1 - dragX0) > 4 || std::abs(dragY1 - dragY0) > 4) boxActive = true;
        }
        // a WHotButton (0x434390): the mouse coming onto an item (0x20) plays 0x194
        const int was = popupHover;
        popupHover = PopupItemAt(x, y);
        if (popupHover >= 0 && popupHover != was) sounds.push_back(0x194);
        // 0x80F1 with the item's command (0x433330): its row opened out, the others closed
        if (popupHover >= 0) popupOpenRow = popup[size_t(popupHover)].row;
    }

    // 0x484DF0: with the mouse within 4 pixels of an edge (32 in a window), the view moves by the mouse's distance
    // from the centre over (half the width / 40), both ways at once
    void BattleMap::EdgeScroll()
    {
        if (mouseX < 0 || !popup.empty()) return;
        const int margin = Screen::windowed ? 32 : 4, W = 640, H = 480;    // 0x484E2C
        if (mouseX > margin && mouseX < W - margin && mouseY > margin && mouseY < H - margin) return;
        const int div = (W / 2) / 40;
        ScrollTo(scrollX - (W / 2 - mouseX) / div, scrollY - (H / 2 - mouseY) / div);
    }

    BattleMap::~BattleMap()
    {
        if (setup.sound)
            for (const Unit& u : units)
                if (u.engineSound) setup.sound->Stop(u.engineSound);
    }

    // 0x481770: speech at the listener, the group's voice (0x449120: its first member's type's +0xA0) choosing the
    // samples
    // 0x439000, an object added to the selection (0x8088 to +0x48): with byte_4D2554 set and the selection holding just
    // it (0x4216D0 = 1), the player's (+0x24 0) vehicle (+0xB8 bit 0) says 0x109 in its type's voice (byte_51A190).
    // So a fresh selection speaks once, by its first object.
    // *Provisional*: byte_4D2554 is taken as set; a group stands for its war map object.
    void BattleMap::SelectionSpeech(int group)
    {
        for (const Unit& u : units)
            if (u.group == group)
            {
                if (u.players && u.vehicle) Speak(0x109, group);
                return;
            }
    }

    void BattleMap::Speak(int id, int group)
    {
        if (!setup.sound) return;
        int voice = 0;
        for (const Unit& u : units)
            if (u.group == group)
            {
                if (auto t = setup.units->find(u.type); t != setup.units->end()) voice = t->second.fieldA0;
                break;
            }
        setup.sound->Play(id, 1.0f, voice);
    }

    void BattleMap::Update()
    {
        ticks++;
        // the game loop (0x403680): the players' updates (the computer player's), then the queued messages (0x434FA0:
        // the delayed 0x80A0 deaths, posted by 0x48AB20, and the 0x80F4 posts), then the screen (0x43A980: the fog's
        // decay, the twinkles, the end check, then its children in start order)
        AiUpdate();
        for (size_t i = 0; i < units.size();)
            if (units[i].dieAt >= 0 && ticks >= units[i].dieAt) Die(i);
            else i++;
        // a forfeit's 0x76 (0x43AA18, posted 150 updates on through 0x4355A0): back to the war map
        if (forfeitAt >= 0 && ticks >= forfeitAt)
        {
            forfeitAt = -1;
            sent = Command{ ReturnCommand, setup.sector };
        }
        for (size_t k = 0; k < controlPosts.size();)
        {
            if (controlPosts[k].at > ticks) { k++; continue; }
            const ControlPost p = controlPosts[k];
            controlPosts.erase(controlPosts.begin() + std::ptrdiff_t(k));
            ControlArrives(p);
        }
        // the moves put off while the route finder was another group's (0x804F, 10 updates on)
        for (size_t k = 0; k < heldMoves.size();)
        {
            if (heldMoves[k].at > ticks) { k++; continue; }
            const HeldMove h = heldMoves[k];
            heldMoves.erase(heldMoves.begin() + std::ptrdiff_t(k));
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].group == h.group && std::find(h.ids.begin(), h.ids.end(), units[i].id) != h.ids.end()) members.push_back(i);
            OrderGroupMove(h.group, members, h.x, h.y);
        }
        FogTick();
        WaterTwinkles();
        // the listener (dword_4E9250 / dy): the view's centre
        if (setup.sound) setup.sound->SetListener(scrollX + 320, scrollY + 240);
        // the right button held for [Tweak] "Quick Cancel Timeout" updates opens the popup (0x805B -> 0x485DE0)
        if (rightDownAt >= 0 && popup.empty() && ticks - rightDownAt == setup.quickCancelTimeout && mouseX >= 0)
            OpenPopup(mouseX, mouseY);
        EdgeScroll();
        ZoomTick();
        CheckBattleEnd();
        // the groups were made before the units (0x4216E0), so their updates (0x483B70, the chase first) come first
        for (size_t g = 0; g < groupTicks.size(); g++) GroupTick(int(g));
        // the world list (0x47D610) in start order: each unit's whole update (0x4967E0 / the vehicles' 0x4913B0), then
        // its children's, its turrets and guns, made as it started (0x47EE10), before the next unit's
        bool moved = false;
        for (size_t i = 0; i < units.size(); i++)
        {
            const float x = units[i].x, y = units[i].y;
            Drive(i);
            // a retreating unit (steering bit 9) arrived at its route's end leaves the map (0x491A96)
            if (units[i].leaving)
            {
                LeaveMap(i);
                i--;
                continue;
            }
            ShipTick(i);
            StatusTick(units[i]);
            BuildingTick(units[i]);
            DroneTick(i);
            if (SinkTick(i)) { i--; moved = true; continue; }
            moved |= units[i].sink != 0;
            MarkerTick(i);
            // 0x496862: after the look round, health under 0 calls vt+0x78 and the update ends there (no smoulder): for
            // a vehicle 0x495BB0 takes it off the map with no explosion, so a kill whose look round comes before its
            // delayed 0x80A0 (10-29 updates) vanishes quietly; for anything else 0x48AC50 (the key building's message,
            // which the engine sends from Die)
            if (LookRound(i) && units[i].health < 0)
            {
                if (units[i].vehicle)
                {
                    Die(i, true);
                    i--;
                    moved = true;
                    continue;
                }
            }
            else Smoulder(i);
            moved |= units[i].x != x || units[i].y != y;
            // 0x47F630 from the object's update (0x48B3D0), unless +0xB8 bit 3 (vt+0x9C): with a renderer (+0x4C)
            // and +0xB8 bit 5 clear, its type is met (byte_51A19C, the research triggers' 0x45B9D0)
            if (!units[i].hidden) encountered.insert(units[i].type);   // (*provisional*: bit 5 taken as hidden)
            for (Turret& t : units[i].turrets) TurretTick(units[i], t);
            for (Gun& g : units[i].guns) GunTick(i, g);
            PoseParts(units[i]);
        }
        for (size_t c = 0; c < circles.size(); c++)
            if (circles[c].alive && circles[c].dieAt >= 0 && ticks >= circles[c].dieAt) CircleDies(c);
        for (size_t b = 0; b < bridgeLinks.size(); b++)
            if (!bridgeLinks[b].gone && bridgeLinks[b].dieAt >= 0 && ticks >= bridgeLinks[b].dieAt) BridgeDies(b, false);
        // the scenery: 0x80A0 arriving, the damage smoke
        for (size_t i = 0; i < props.size(); i++)
        {
            if (!props[i].alive) continue;
            if (props[i].dieAt >= 0 && ticks >= props[i].dieAt) { PropDies(i); continue; }
            if (props[i].updating) SmoulderProp(i);
        }
        UpdateEffects();
        UpdateLights();
        // the view list is rebuilt in key order every frame (0x40D750), and the keys follow the poses
        (void)moved;
        RekeyViews();
        if (miniShown && (miniFrames & 0xF) == 0) MakeDots();
        miniFrames++;
    }

    // 0x485DE0: "Battlemap menu" (WPopupMenu 0x432C60, font 0x71): Cancel (0x73), and Return to Warmap (0x76, icon
    // 0x2C, 0x47A) unless the player has a selection or both sides have objects on the map; then the items of
    // whoever answers 0x805C (the tool, the selection, the world: none yet). Placed and sounded as the war map's.
    void BattleMap::OpenPopup(int x, int y)
    {
        popup.clear();
        popupHover = -1;
        popupOpenRow = -1;
        auto add = [&](int command, int icon, int text) {
            Rect r = SpriteRect(icon, 0, 0);
            if (r.w <= 0) r.w = r.h = 32;
            popup.push_back({ command, icon, text, r });
        };
        add(0x73, 0x1C, 0x476);
        // Return to Warmap only with nothing selected (0x4391F0) and one side without units on the map (0x47D610 /
        // 0x47D660 on the battle world's list)
        bool ours = false, theirs = false;
        for (const Unit& u : units) (u.players ? ours : theirs) = true;
        if (Mode() == 0 && (!ours || !theirs)) add(ReturnCommand, 0x2C, 0x47A);
        // then the selection's (0x805C, 0x4482D0): all of one kind, its first member's orders; else only the orders
        // every member answers (0x448500: each asked 0x8130 / 0x8131 / 0x8132)
        // *Provisional*: "one kind" taken as one type; the world's own items (0x485EFD) aren't traced
        std::vector<const Unit*> members;
        for (const Unit& u : units)
            if (u.players && selected[size_t(u.group)]) members.push_back(&u);
        if (!members.empty())
        {
            const size_t base = popup.size();
            AddOrders(*members[0]);
            std::vector<int> seen{ members[0]->type };
            for (const Unit* m : members)
            {
                if (std::find(seen.begin(), seen.end(), m->type) != seen.end()) continue;
                seen.push_back(m->type);
                const std::vector<PopupItem> mine(popup.begin() + std::ptrdiff_t(base), popup.end());
                popup.resize(base);
                AddOrders(*m);
                std::vector<PopupItem> theirs(popup.begin() + std::ptrdiff_t(base), popup.end());
                popup.resize(base);
                for (const PopupItem& item : mine)
                    if (std::any_of(theirs.begin(), theirs.end(), [&](const PopupItem& o) { return o.command == item.command && o.param == item.param; }))
                        popup.push_back(item);
            }
        }
        LayOutPopup(x, y);
        sounds.push_back(0x193);
    }

    // the rows and the placement: frontend/PopupRows.h
    void BattleMap::LayOutPopup(int x, int y)
    {
        LayOutPopupRows(popup, x, y, 640, 480);
        popupOpenRow = -1;
    }

    // +0x3C (0x47EF30): how many of its children are weapons (+0xB8 bit 11): the turrets (0x489820, types
    // 0x2E6-0x313 and 0x389-0x38C) and the shot classes on 0x4949A0 (0x488930: 0x197-0x199, 0x2BF-0x2C4, 0x3AA,
    // 0x3EB-0x3ED; 0x4887E0: 0x2B9-0x2BB, 0x320-0x322, 0x3B5-0x3B7; 0x488460: 0x3AB; 0x48DBE0: 0x19A-0x19C)
    bool BattleMap::Armed(const ObjOjd::UnitData& unit)
    {
        auto weapon = [](int t) {
            return (t >= 0x2E6 && t <= 0x313) || (t >= 0x389 && t <= 0x38C) || (t >= 0x197 && t <= 0x19C) ||
                   (t >= 0x2B9 && t <= 0x2BB) || (t >= 0x2BF && t <= 0x2C4) || (t >= 0x320 && t <= 0x322) ||
                   t == 0x3AA || t == 0x3AB || (t >= 0x3B5 && t <= 0x3B7) || (t >= 0x3EB && t <= 0x3ED);
        };
        for (const auto& part : unit.parts)
            if (weapon(part.a)) return true;
        return false;
    }

    // 0x47DA80: which types may give a special order, some only once a research item is done (Martian items by
    // their place in the tree: 2 and 3 triggers, 32 / 37 / 44-47 / 54-56 units; Human 39, an enhancement)
    bool BattleMap::CanDo(int t, int command) const
    {
        auto m = [&](int index) { return setup.researchDone && setup.researchDone(false, index); };
        auto h = [&](int index) { return setup.researchDone && setup.researchDone(true, index); };
        auto in = [&](std::initializer_list<int> types) { return std::find(types.begin(), types.end(), t) != types.end(); };
        switch (command)
        {
        case 0x80EF: return in({ 0x16, 0x17, 0x18 });                       // Submerge
        case 0x80EE: return in({ 7, 8, 9 });                                // Tunnel
        case 0x80D5: return in({ 0x42, 0x43, 0x47, 0x44, 0x45 });           // Black Dust
        case 0x80D6: return in({ 0x48, 0x46 });
        case 0x80D7: return t == 0x49;
        case 0x80D8: return m(32) && in({ 0x47, 0x45 });                    // Shock Bomb
        case 0x80D9: return m(32) && in({ 0x48, 0x46 });
        case 0x80DA: return m(32) && t == 0x49;
        case 0x80DB: return t == 0x20;                                      // Explosives
        case 0x80DC: return in({ 0x11, 0x21 });
        case 0x80DD: return in({ 0x12, 0x22 });
        case 0x80DE: return in({ 0x11, 0x12, 0x20, 0x56, 0x50 });           // Infiltrate
        case 0x80DF: return in({ 0x21, 0x57, 0x51 });
        case 0x80E0: return in({ 0x22, 0x58, 0x52 });
        case 0x80E1: return (m(37) && t == 0x4A) || (m(46) && t == 0x50);  // Pulse
        case 0x80E2: return (m(37) && t == 0x4B) || (m(46) && t == 0x51);
        case 0x80E3: return m(37) && t == 0x4C;
        case 0x80E4: return (m(47) && in({ 0x50, 0x51, 0x52 })) || (m(54) && t == 0x56) || (h(39) && in({ 0xB, 0xC })); // Freeze
        case 0x80E5: return m(54) && t == 0x57;
        case 0x80E6: return m(54) && t == 0x58;
        case 0x80E7: return m(55) && t == 0x56;                             // Scare
        case 0x80E8: return m(55) && t == 0x57;
        case 0x80E9: return m(55) && t == 0x58;
        case 0x80EA: return m(56) && t == 0x56;                             // Control
        case 0x80EB: return m(56) && t == 0x57;
        case 0x80EC: return m(56) && t == 0x58;
        case 0x80F0:                                                        // Scan
            if ((m(44) || m(45)) && in({ 0x50, 0x51, 0x52 })) return true;
            // the command building of the battle's kind: the Martian Base (0x33E) where it's 3, else the
            // Communications Centre (0x78)
            return (m(2) || m(3)) && t == (setup.sector == 3 ? 0x33E : 0x78);
        default: return false;
        }
    }

    // 0x47E1E0, an object's orders: Build and Suspend / Resume (only while its sector is quiet, 0x43D010: not on a
    // battle's map); the stances Roam and Attack (armed and moving: the type's +0x52 not 0), Attack and Retaliate
    // (armed), Avoid (moving) and Retreat (moving, a battle on, its sector not quiet, reinforcement points,
    // dword_4F76E8); Abandon; Force Fire (armed); then each special order 0x47DA80 allows, in order.
    void BattleMap::AddOrders(const Unit& first)
    {
        auto t = setup.units->find(first.type);
        if (t == setup.units->end()) return;
        const bool armed = Armed(t->second), still = t->second.field52 == 0;
        auto add = [&](int command, int param, int icon, int text) {
            Rect r = SpriteRect(icon, 0, 0);
            if (r.w <= 0) r.w = r.h = 32;
            popup.push_back({ command, icon, text, r, param });
        };
        if (armed && !still) add(0x80CE, 3, 0x34F, 0x474);
        if (armed)
        {
            add(0x80CE, 2, 0x34E, 0x473);
            add(0x80CE, 1, 0x34D, 0x472);
        }
        if (!still) add(0x80CE, 0, 0x34C, 0x471);
        if (!still && battleOn && !rpoints.empty()) add(0x80CE, 0x80B7, 0x2D, 0x479);
        add(0x80CE, 0x8067, 0x34B, 0x46F);
        if (armed) add(0x80F3, 0x80ED, 0x1E, 0x48A);
        struct Special { int command, icon, text; };
        static constexpr Special specials[] = {
            { 0x80EF, 0x349, 0x48C }, { 0x80EE, 0x34A, 0x48B },
            { 0x80D5, 0x25, 0x482 }, { 0x80D6, 0x25, 0x482 }, { 0x80D7, 0x25, 0x482 },
            { 0x80D8, 0x26, 0x483 }, { 0x80D9, 0x26, 0x483 }, { 0x80DA, 0x26, 0x483 },
            { 0x80DB, 0x351, 0x484 }, { 0x80DC, 0x351, 0x484 }, { 0x80DD, 0x351, 0x484 },
            { 0x80DE, 0x352, 0x485 }, { 0x80DF, 0x352, 0x485 }, { 0x80E0, 0x352, 0x485 },
            { 0x80E1, 0x353, 0x486 }, { 0x80E2, 0x353, 0x486 }, { 0x80E3, 0x353, 0x486 },
            { 0x80E4, 0x354, 0x487 }, { 0x80E5, 0x354, 0x487 }, { 0x80E6, 0x354, 0x487 },
            { 0x80E7, 0x355, 0x488 }, { 0x80E8, 0x355, 0x488 }, { 0x80E9, 0x355, 0x488 },
            { 0x80EA, 0x356, 0x489 }, { 0x80EB, 0x356, 0x489 }, { 0x80EC, 0x356, 0x489 },
            { 0x80F0, 0x358, 0x48D } };
        for (const Special& sp : specials)
            if (CanDo(first.type, sp.command)) add(0x80F3, sp.command, sp.icon, sp.text);
    }

    void BattleMap::ClosePopup(bool escape)
    {
        popup.clear();
        popupHover = -1;
        popupOpenRow = -1;
        if (escape) sounds.push_back(0x196);
    }

    std::optional<Command> BattleMap::OnMouseButton(int x, int y, bool down)
    {
        if (endBox)
        {
            if (!down && EndBoxButtonRect().Contains(x, y)) return EndBoxCommand();
            return std::nullopt;
        }
        // the popup has the mouse (0x42E9B0). The game's (WPopupMenu 0x4330B0): a press or a release of either button
        // off its items closes it (0x42F2A0); an item acts as a button is let go on it (0x202 / 0x205). With
        // [Modern] "Sticky popup menus" an item acts as it's pressed and a press elsewhere closes the popup.
        if (!popup.empty())
        {
            const int item = PopupItemAt(x, y);
            if (setup.stickyPopup)
            {
                if (!down) return std::nullopt;
                if (item >= 0) return ActivatePopupItem(size_t(item));
                ClosePopup(false);
                return std::nullopt;
            }
            if (item < 0) { ClosePopup(false); return std::nullopt; }
            if (!down) return ActivatePopupItem(size_t(item));
            return std::nullopt;
        }
        if (!down)
        {
            const bool wasMini = miniDrag;
            miniDrag = false;
            // a group icon takes the mouse as it's pressed (0x42E9B0) and acts as it's let go (0x202, 0x484920)
            if (iconDown >= 0)
            {
                const int at = GroupIconAt(x, y);
                if (at == iconDown && size_t(at) < groupIcons.size()) ClickGroupIcon(size_t(at));
                iconDown = -1;
                dragging = false;
                return std::nullopt;
            }
            if (dragging && !wasMini) ReleaseLeft(x, y);
            dragging = false;
            return std::nullopt;
        }
        if (const int at = GroupIconAt(x, y); at >= 0)
        {
            iconDown = at;
            return std::nullopt;
        }
        if (popup.empty() && down && InMinimap(x, y))
        {
            miniDrag = true;   // the minimap takes the mouse (0x42E9B0) until the button's let go
            MinimapScroll(x, y);
            return std::nullopt;
        }
        // 0x201 (cursor modes 0 and 1): a drag rectangle starts here (0x49C230) and the view keeps the mouse
        dragging = true;
        boxActive = false;
        dragX0 = dragX1 = x;
        dragY0 = dragY1 = y;
        return std::nullopt;
    }

    // A popup item clicked (WHotButton 0x434390 on 0x202 / 0x205): its click sound, the popup closed, its command
    std::optional<Command> BattleMap::ActivatePopupItem(size_t index)
    {
        const PopupItem& item = popup[index];
        int command = item.command;
        const int param = item.param;
        sounds.push_back(0x195);    // the item's click (0x434390 on 0x202 / 0x205)
        ClosePopup(false);
        if (command == ReturnCommand) return Command{ ReturnCommand, setup.sector };
        if (command == 0x80CE && param >= 0 && param <= 3)
        {
            // packet 0x15 -> 0x4817C0: the stance (0x80B2) to the group and each member (+0xD8); the group
            // keeps it at +0x54 and, for Roam and Attack, its centre as home (0x483AE0)
            for (size_t g = 0; g < selected.size() && g < groupStance.size(); g++)
                if (selected[g]) SetStance(int(g), param);
            return std::nullopt;
        }
        if (command == 0x80CE && param == 0x80B7)
        {
            // Retreat (0x4817C0 -> 0x480EF0) for each selected group of the player's
            for (size_t g = 0; g < selected.size(); g++)
                if (selected[g]) Retreat(int(g));
            return std::nullopt;
        }
        if (command == 0x80CE && param == 0x8067)
        {
            // Abandon (0x4817C0): each member gets 0x8067, which sends itself 0x80A0 at once (0x489FA0)
            for (size_t i = 0; i < units.size();)
                if (units[i].players && selected[size_t(units[i].group)]) Die(i);
                else i++;
            return std::nullopt;
        }
        if (command == 0x80F3)
        {
            // Force Fire and the specials (0x4817C0): each selected group's mode (+0x1DC) for the next click
            for (size_t g = 0; g < selected.size(); g++)
                if (selected[g]) GroupOrder(int(g), param);
            return std::nullopt;
        }
        if (command == 0x80CE)
        {
            SDL_Log("order 0x%X 0x%X: not in yet", command, param);
            return std::nullopt;
        }
        ClearSelection(); // 0x73, Cancel
        return std::nullopt;
    }

    int BattleMap::PopupItemAt(int x, int y) const
    {
        for (size_t i = 0; i < popup.size(); i++) if (PopupShown(i) && popup[i].rect.Contains(x, y)) return int(i);
        return -1;
    }

    // 0x204 / 0x205: held, the popup (above); let go quickly, 0x73 (Cancel) to the selection
    std::optional<Command> BattleMap::OnRightButton(int x, int y, bool down)
    {
        if (!popup.empty())
        {
            // held open: let go on an item it acts, anywhere else (or pressed off the items) the popup closes
            rightDownAt = -1;
            if (setup.stickyPopup) return std::nullopt;
            const int item = PopupItemAt(x, y);
            if (item < 0) { ClosePopup(false); return std::nullopt; }
            if (!down) return ActivatePopupItem(size_t(item));
            return std::nullopt;
        }
        if (down) rightDownAt = ticks;
        else
        {
            // 0x205 let go before the popup came: the drag rectangle goes and 0x73 (Cancel) clears the selection
            if (rightDownAt >= 0)
            {
                dragging = boxActive = false;
                ClearSelection();
            }
            rightDownAt = -1;
        }
        return std::nullopt;
    }

    int BattleMap::TipOwner(int x, int y) const
    {
        if (const int i = PopupItemAt(x, y); i >= 0) return 100 + i;
        return -1;
    }

    std::string BattleMap::TipText(int owner, int) const
    {
        if (owner >= 100 && size_t(owner - 100) < popup.size()) return res.String(popup[size_t(owner - 100)].text);
        return {};
    }

    void BattleMap::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        TerrainView::Target t{ canvas.Pixels(), canvas.Width(), 0, 0, canvas.Width(), canvas.Height() };
        // the ground row by row, each object drawn once the rows reach its depth (0x49F610)
        size_t next = 0;
        TerrainView::Window window;
        // the lights as the models' faces see them (0x49ED90): their level the landscape's .INT table at the
        // intensity they have now x 256 (+0x48, 0x40CB10)
        std::vector<WofInstance::PointSource> faceLights;
        for (const Light& l : lights)
        {
            if (!l.alive) continue;
            const float now = l.fading ? l.current : l.intensity;
            int at = int(std::clamp(double(now) * 256.0, 0.0, 255.0));
            const float level = levels.size() >= 256 ? float(levels[size_t(at)]) : float(at);
            faceLights.push_back({ l.x, l.y, l.z, l.range, level });
        }
        std::vector<BeamItem> beams;
        BuildBeams(beams);
        size_t nextBeam = 0;
        auto beamsTo = [&](int key) {
            for (; nextBeam < beams.size() && beams[nextBeam].key < key; nextBeam++) DrawBeam(beams[nextBeam], canvas, t);
        };
        auto between = [&](int limit) {
            for (; next < objects.size() && objects[next].key < limit; next++)
            {
                const ViewObject& o = objects[next];
                beamsTo(o.key);
                if (o.unit >= 0 && size_t(o.unit) < units.size() && units[size_t(o.unit)].fogged) continue;   // 0x48B2D0
                if (o.light >= 0)
                {
                    const Light& l = lights[size_t(o.light)];
                    if (!l.alive) continue;
                    TerrainView::AddLight(land, window, l.x, l.y, l.z, l.range, l.intensity);
                    continue;
                }
                const int slot = o.model ? o.model->slot : o.slot;
                auto st = slotTables.find(slot);
                const ShadeTable* shade = st != slotTables.end() ? &st->second : res.GetShade(slot);
                if (!shade) continue;
                if (o.wof)
                {
                    o.wof->Draw(sun, *shade, t, -scrollX, -scrollY, faceLights, setup.wofBrightness, float(o.bright) * (1.0f / 255.0f));
                    continue;
                }
                if (o.sprite)
                {
                    // 0x40EE10: the frame at the point less the hot spot, through the slot's table at level 0
                    const int frame = o.effect >= 0 ? effects[size_t(o.effect)].counter >> 8 : 0;
                    const IndexedImage* image = res.GetFrame(o.sprite, frame);
                    const GType* g = res.FindGType(o.sprite);
                    // a flying object's sprite is at its type's +0xA1 / 4 (0x487846: its renderer's vt+0x18, the
                    // sprite's 0x40F040, sets +0x34, the table's level); the others at 0
                    int level = o.level >= 0 ? o.level : 0;
                    if (o.level < 0 && o.effect >= 0 && size_t(o.effect) < effects.size())
                        if (auto et = setup.units->find(effects[size_t(o.effect)].type); et != setup.units->end()) level = (et->second.fieldA1 & 0xFFFF) >> 2;
                    if (image && g) canvas.DrawImage(*image, shade->Level(level), o.x - g->hotX - scrollX, o.y - g->hotY - scrollY);
                    continue;
                }
                const IobModel& m = o.model->model;
                // 0x4113F0: the two nearest lights (dword_4B8E28) whose ranges reach the object (its point, +0x38..+0x3C,
                // within the range + its radius +0x3E, the octagonal distance, each further than the last) add to a
                // copy of its levels at their level x [Game] "ISO Brightness" (0.5), at most 63 x its level / 255
                // (255 here: cap 63)
                // its base lights at the renderer's level (0x49E2C0 -> 0x49DF40 when the fog changes it): made once
                // for each model and level
                const std::vector<uint8_t>* base = &o.model->lights;
                if (o.bright != 255)
                {
                    auto& cached = isoLights[{ o.model, o.bright }];
                    if (cached.empty()) cached = IsoObject::BaseLights(m, sun, levels, o.bright);
                    base = &cached;
                }
                const std::vector<uint8_t>* use = base;
                std::vector<uint8_t> lit;
                if (!faceLights.empty())
                {
                    int ox = 0, oy = 0, oz = 0, radius = 0, type = -1;
                    if (o.prop >= 0 && size_t(o.prop) < props.size())
                    {
                        const Prop& pr = props[size_t(o.prop)];
                        ox = int(pr.x); oy = int(pr.y); oz = land.HeightAt(ox, oy) + int(pr.z); type = pr.type;
                    }
                    else if (o.unit >= 0 && size_t(o.unit) < units.size())
                    {
                        const Unit& un = units[size_t(o.unit)];
                        ox = int(un.x); oy = int(un.y); oz = land.HeightAt(ox, oy) + int(un.drive.lift); type = un.type;
                    }
                    if (type >= 0)
                    {
                        radius = Radius44(type);
                        int last = -1;
                        for (int n = 0; n < 2; n++)
                        {
                            int best = 1000000;
                            const WofInstance::PointSource* pick = nullptr;
                            for (const WofInstance::PointSource& l : faceLights)
                            {
                                const int d = Octagonal(ox, oy, l.x & 0xFFFF, l.y & 0xFFFF);
                                if (d < (l.range & 0xFFFF) + radius && d < best && d > last) { best = d; pick = &l; }
                            }
                            last = best;
                            if (!pick) break;
                            if (lit.empty()) lit = *base;
                            // at most max(8, ftol(the level / 255 x 63)) (0x4111B0)
                            IsoObject::AddPointLight(m, lit, ox - (pick->x & 0xFFFF), oy - (pick->y & 0xFFFF), oz - (pick->z & 0xFFFF),
                                                     int(pick->level * setup.isoBrightness), pick->range & 0xFFFF, radius,
                                                     int(float(o.bright) * (1.0f / 255.0f) * 63.0f));
                        }
                        if (!lit.empty()) use = &lit;
                    }
                }
                IsoObject::Draw(m, *use, o.x - IsoObject::HalfWidth(m) - scrollX, o.y + IsoObject::OffsetY(m) - scrollY,
                                *shade, dither, t);
            }
        };
        // the fog the ground darkens by (0x4A0045: FogMap +0x18, the width +0x84, the smooth index +0x9C, the
        // darkness +0x90)
        TerrainView::Fog fogView{ fog.map.empty() ? nullptr : fog.map.data(), land.Width(), fog.smooth, fog.darkness };
        TerrainView::Render(land, tiles, table, levels, dither, t, -scrollX, -scrollY, fog.enabled ? &fogView : nullptr, between, &window);
        beamsTo(0x7FFFFFFF);
        DrawSelection(canvas);
        DrawGroupIcons(canvas);
        // the minimap (0x486CB0): the picture, the dots (2 x 2 each, 0x4A1DD0), the view's rectangle (0x49C7C0)
        if (miniShown && miniPicture)
            if (const ShadeTable* shade = res.GetShade(21))
            {
                canvas.DrawImage(*miniPicture, shade->Level(0), miniX, miniY);
                uint32_t* px = canvas.Pixels();
                const int cw = canvas.Width(), ch = canvas.Height();
                auto plot = [&](int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < cw && y < ch) px[size_t(y) * size_t(cw) + size_t(x)] = c; };
                for (const Dot& d : dots)
                    for (int j = 0; j < 2; j++)
                        for (int i = 0; i < 2; i++) plot(miniX + d.x + i, miniY + d.y + j, d.colour);
                const int x0 = miniX + rectX, y0 = miniY + rectY, x1 = x0 + rectW - 1, y1 = y0 + rectH - 1;
                for (int x = x0; x <= x1; x++) { plot(x, y0, rectColour); plot(x, y1, rectColour); }
                for (int y = y0; y <= y1; y++) { plot(x0, y, rectColour); plot(x1, y, rectColour); }
            }
        // the popup over everything, each item's next frame under the mouse
        for (size_t i = 0; i < popup.size(); i++)
        {
            if (!PopupShown(i)) continue;
            const PopupItem& item = popup[i];
            bool lit = int(i) == popupHover && res.GetFrame(item.icon, 1);
            canvas.DrawSprite(item.icon, lit ? 1 : 0, item.rect.x, item.rect.y);
        }
        // the battle's end box (0x432520's layout, as the war map draws it): the backdrop (GType 0x68) centred 4 above
        // the bottom, in Martian games ma_scrn (0x69) over it at 50%; the text (font 0x73, level 21) wrapped at 320
        // pixels and centred, 0x2B below the top (0x17 below ma_scrn's in Martian games); the button (0x6A)
        if (endBox)
        {
            const bool human = setup.human;
            const Rect b = EndBoxRect();
            canvas.DrawSprite(0x68, 0, b.x, b.y);
            if (!human) canvas.DrawSprite(0x69, 0, b.x + 0x0B, b.y - 0x5E, 0, Canvas::Blend::Translucent);
            const Font* font = res.GetFont(0x73);
            const int lineH = font ? font->Height() : 12;
            std::vector<std::string> lines;
            std::string line, word;
            auto width = [&](const std::string& t) { return font ? font->Measure(t) : int(t.size()) * 8; };
            auto flush = [&]() {
                if (word.empty()) return;
                std::string t = line.empty() ? word : line + " " + word;
                if (!line.empty() && width(t) > 320) { lines.push_back(line); line = word; }
                else line = t;
                word.clear();
            };
            for (char c : endBox->text)
                if (c == ' ') flush();
                else if (c == '\n') { flush(); lines.push_back(line); line.clear(); }
                else word += c;
            flush();
            if (!line.empty()) lines.push_back(line);
            const int ox = human ? b.x : b.x + 0x0B;
            int y = (human ? b.y : b.y - 0x5E) + (human ? 0x2B : 0x17);
            for (const std::string& l : lines)
            {
                canvas.DrawString(0x73, l, { ox, y, b.w, lineH }, TextCentreX, 21);
                y += lineH;
            }
            const Rect bt = EndBoxButtonRect();
            const bool over = bt.Contains(mouseX, mouseY);
            canvas.DrawSprite(0x6A, over && res.GetFrame(0x6A, 1) ? 1 : 0, bt.x, bt.y);
        }
    }

    Rect BattleMap::EndBoxRect() const
    {
        const IndexedImage* f = res.GetFrame(0x68, 0);
        const int w = f ? f->width : 0, h = f ? f->height : 0;
        return { (640 - w) / 2, 480 - h - 4, w, h };
    }

    Rect BattleMap::EndBoxButtonRect() const
    {
        const Rect b = EndBoxRect();
        const IndexedImage* f = res.GetFrame(0x6A, 0);
        const bool human = setup.human;
        return { b.x + (human ? 0x52 : 0x87), b.y + (human ? 0x76 : 0x28), f ? f->width : 0, f ? f->height : 0 };
    }

    // The box's button: 0x76 back to the war map; 0x8024 only closes it, the battle map going on (Return to Warmap
    // is then in the popup, with one side gone)
    std::optional<Command> BattleMap::EndBoxCommand()
    {
        const int command = endBox ? endBox->command : 0;
        endBox.reset();
        if (command == ReturnCommand) return Command{ ReturnCommand, setup.sector };
        return std::nullopt;
    }

    // 0x43A980, each update with the battle on (+0x54) and no box up yet (dword_4F7450): each side's objects in the
    // battle (0x47D610 / 0x47D660: those whose side, 0x4406E0, is 0 / 1), the player's taken as its own, plus its
    // construction list's objects in the sector (+0x54 / +0x58, 0x4216D0; a side keeps the sector while its
    // buildings stand). The player's at 0: the report filled (0x43A800)
    // and the box "Sector Lost" (0x49D) when the player held the sector (the report's +0x6C), else "Attack Failed"
    // (0x49B), its button 0x76. The other side's at 0: the report filled, and "Invaders Repelled!" (0x49C) with
    // 0x8024 when the player held it, else "Attack Successful!" (0x49A), with 0x76 if that ends the game (+0x55:
    // after the owner is recomputed, sector 3 the Humans' (dword_4EB05C) or London the Martians' (dword_4EC49C)),
    // else 0x8024.
    // *Provisional*: the units are what's counted (the game counts every object with a side in the battle's list);
    // a construction's war map object is taken as gone with its last battle unit; the report's +0x6C is taken as the side that held
    // the sector as the battle began;.
    void BattleMap::CheckBattleEnd()
    {
        // 0x43AA18, before the battle-on test: with no box up, a forfeit (byte_4E9B94, the key building lost with no
        // battle on) posts 0x76 to the screen 150 updates on (0x111 / 0x76 through 0x4355A0) and marks the box up
        if (forfeit && !battleDecided)
        {
            battleDecided = true;
            forfeitAt = ticks + 150;
            return;
        }
        if (!battleOn || battleDecided) return;
        int ours = 0, theirs = 0;
        for (const Unit& u : units) (u.players ? ours : theirs)++;
        // plus the sector's construction lists (0x43AA59 / 0x43AAF6: +0x54 the player's, +0x58 the other side's,
        // 0x4216D0): a construction stands while any of its battle units does, and one with no battle places (never
        // on this map) always counts
        for (size_t g = 0; g < setup.groups.size(); g++)
        {
            const BattleGroup& bg = setup.groups[g];
            if (bg.list != 2 && bg.list != 3) continue;
            bool counts = bg.places.empty();
            if (!counts)
                for (const Unit& u : units) if (u.ref == int(g)) { counts = true; break; }
            if (counts) (bg.Players() ? ours : theirs)++;
        }
        if (ours != 0 && theirs != 0) return;
        battleDecided = true;
        const int playerSide = setup.human ? 0 : 1;
        const bool held = setup.ownerFaction == playerSide;
        EndBox box;
        if (ours == 0)
        {
            box.text = res.String(held ? 0x49D : 0x49B);
            box.command = ReturnCommand;
        }
        else
        {
            const bool gameOver = (setup.sector == 3 && setup.human) || (setup.sector == 30 && !setup.human);
            box.text = res.String(held ? 0x49C : 0x49A);
            box.command = !held && gameOver ? ReturnCommand : 0x8024;
        }
        endBox = box;
        popup.clear();
    }

    std::vector<int> BattleMap::LostRefs() const
    {
        std::vector<int> out;
        for (size_t g = 0; g < setup.groups.size(); g++)
        {
            if (setup.groups[g].places.empty() || setup.groups[g].ref < 0) continue;
            if (g < groupLeft.size() && groupLeft[g]) continue;     // it left by a reinforcement point
            bool left = false;
            for (const Unit& u : units) if (u.ref == int(g)) { left = true; break; }
            if (!left) out.push_back(setup.groups[g].ref);
        }
        return out;
    }
}
