#include "battlemap/BattleMap.h"
#include "platform/Sound.h"
#include "frontend/Canvas.h"
#include "frontend/Resources.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace wow
{
    namespace
    {
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
        SetUpRouteFinder();     // 0x47AF10, with the landscape, before anything is placed (the bridges add nodes)
        if (s.loadScenery) LoadScenery(sun, letter);

        Deploy();
        SetUpReinforcements();
        for (size_t i = 0; i < units.size(); i++)
        {
            Unit& u = units[i];
            // a vehicle (category 1) drives: 0x490820 / 0x490E70 (the turn limit by move class, the phase from rand())
            // every object's +0xAC is its type's +0x44 word (the neighbours keep apart by it)
            if (auto d = setup.units->find(u.type); d != setup.units->end()) u.drive.radius = int16_t(d->second.hitPoints);
            u.health = MaxHealth(u.type);
            if (auto d = setup.units->find(u.type); d != setup.units->end()) u.stance = d->second.field43;   // 0x495740
            SetUpGuns(u);
            if (auto d = setup.units->find(u.type); d != setup.units->end() && d->second.category == 1)
            {
                u.vehicle = true;
                const int mc = d->second.field52;
                u.drive.turnRate = mc >= 0 && mc <= 4 ? 64 : (mc == 5 || mc == 6) ? 8 : mc == 7 ? 16 : 0;
                u.drive.radius = int16_t(d->second.hitPoints);
                u.drive.arrival = float(double(u.drive.radius) * setup.vehicles.arrivalRadius - -20.0);
                u.drive.phase = int16_t(random::Rand());
            }
            AddRenderer(u.type, u.x, u.y, u.heading, int(i));
            AddPartRenderers(i);
        }
        for (const Unit& u : units) Occupy(u.type, u.x, u.y);
        SetUpGrounds();
        groupStance.assign(setup.groups.size(), 1);
        // the groups' targets, modes and engagement ranges (0x4802E0: +0x4E, from 100000 the least of each member's
        // sight (+0x48) and its vt+0xB0, its guns' longest record 12 +0 range, or 10000 with none (0x495850))
        groupTargets.assign(setup.groups.size(), GroupTarget{});
        groupMode.assign(setup.groups.size(), 0);
        groupRange.assign(setup.groups.size(), 100000);
        for (const Unit& u : units)
        {
            if (u.group < 0 || size_t(u.group) >= groupRange.size()) continue;
            auto t = setup.units->find(u.type);
            if (t == setup.units->end()) continue;
            int range = u.guns.empty() ? 10000 : 0;
            for (const Gun& g : u.guns) range = std::max(range, WeaponData(g.type)[0]);
            int& r = groupRange[size_t(u.group)];
            r = std::min({ r, ObjOjd::Vision(t->second, setup.armour), range });
        }
        groupTicks.assign(setup.groups.size(), 0);
        groupHomeX.assign(setup.groups.size(), 0);
        groupHomeY.assign(setup.groups.size(), 0);
        AiStart();     // the battle begun, told to the computer player (0x8075 -> 0x445210)
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });

        // The start view (0x439B7E): on the player's HQ (the player's constructions list, HCON, searched for type
        // 278 in London or 314 elsewhere in a Human game, 830 in sector 3 or 120 elsewhere in a Martian one), the
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
    // (0x40B120, 6-bit RGB): the rectangle 47, 0, 0 (Human) or 12, 42, 4 (Martian); a dot (0x480070) the side's
    // vehicle colour (0, 63, 0 / 63, 0, 0) for a category 1 object of the player's, its other colour (32, 63, 0 /
    // 63, 32, 0) for the player's others, and the other side's (0, 0, 63 / 0, 63, 0).
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
            rectColour = six(0x2F, 0, 0);
            boxInner = six(0x28, 0, 0);
            vehicleColour = six(0, 0x3F, 0);
            otherColour = six(0x20, 0x3F, 0);
            enemyColour = six(0, 0, 0x3F);
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
            dots.push_back({ x & 0xFF, y & 0xFF, colour });
        }
    }

    // An object's screen point (+0x2C / +0x30, the view's coordinates): its renderer's point less the scroll
    bool BattleMap::UnitScreen(const Unit& u, int& sx, int& sy) const
    {
        const int ix = int(u.x), iy = int(u.y);
        sx = ix - scrollX;
        sy = land.ProjectY(iy, land.HeightAt(ix, iy)) - scrollY;
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
                if (sx >= l && sx < r && sy >= t && sy < b) hits.push_back(u.group);
            }
            if (hits.empty()) return;
            selected.assign(selected.size(), false);
            for (int g : hits) selected[size_t(g)] = true;
            return;
        }
        const Unit* best = HotspotAt(x, y);
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
                if (wy && force) ForceFire(wx, wy);
                else if (wy) OrderMove(wx, wy);     // (a group in mode 1 fires there instead, 0x481CA0)
            }
            return;
        }
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        if (Mode() == 0)
        {
            if (!best->players || selected[size_t(best->group)]) return;
            if (ctrl && !vehicle(*best)) return;
            ClearSelection();
            selected[size_t(best->group)] = true;
            return;
        }
        // mode 1 (0x497794): with Ctrl force fire at the ground under it (packet 0x13); else the player's selects
        // its group and the other side's is attacked (packet 0x10)
        if (ctrl)
        {
            const int wx = x + scrollX;
            const int wy = land.ScreenYToWorldY(wx, y + scrollY);
            if (wy) ForceFire(wx, wy);
        }
        else if (best->players)
        {
            ClearSelection();
            selected[size_t(best->group)] = true;
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
        if (const Unit* u = HotspotAt(x, y))
        {
            if (mode == 0 && u->players) return 1;
            if (mode == 1) return (!u->players || ctrl) ? 5 : 1;
        }
        if (mode == 1) return ctrl ? 5 : 4;
        return 0;
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
        ScrollTo(cx - 320, land.ProjectY(cy, land.HeightAtNearest(cx, cy)) - 240);
    }

    // The selected units' brackets (0x4972F0 / 0x497000: GType 5, hselec.spr, frames 0-3 at the hotspot's corners 8 in)
    // and power bar (GType 7, hpower.spr, at the middle of its bottom 8 up; its frame the frame count x (1 - damage
    // / max), at most the last: the last, as nothing is damaged yet); then the drag box (0x49C3E0): two lines a side
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
                canvas.DrawSprite(7, std::max(0, bar->FrameCount() - 1), (l + r) / 2, b - 8);
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
    //  - the other side's (its faction isn't the sector's owner): a Martian pod's place (type 179 in the scenery
    //    list) for Martians, else the entry point from the sector it came from (0x436050)
    //  - the owner's: a command building's place (the last of 830 / 120 or 278 / 314 among all the battle units,
    //    either side's), else a pod's for Martians, else the entry point if it came from somewhere, else the middle
    // and the player's last centre is where the view starts. Its units are placed round it (Place).
    // *Not yet*: the pods (no pod object is made yet), so a Martian never starts at one.
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
                u.id = nextId++;
                units.push_back(u);
            }
        selected.assign(setup.groups.size(), false);
        // the battle is on (the report's +0x54) when both sides have something here
        {
            bool ours = false, theirs = false;
            for (const Unit& u : units) (u.players ? ours : theirs) = true;
            battleOn = ours && theirs;
        }
        groupRetreat.assign(setup.groups.size(), 0);
        groupLeft.assign(setup.groups.size(), false);
        if (!setup.units) return;
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
                if (setup.ownerFaction != faction)
                    deploy::EntryPoint(g.previous, setup.sector, moveClass, cx, cy);
                else if (hqX != -1 || hqY != -1) { cx = hqX; cy = hqY; }
                else if (g.previous) deploy::EntryPoint(g.previous, setup.sector, moveClass, cx, cy);
                if (g.Players()) { startX = cx; startY = cy; }
                std::vector<size_t> members;
                for (size_t i = 0; i < g.places.size(); i++) members.push_back(first[gi] + i);
                Place(members, cx, cy);
            }
        }
    }

    // 0x4222C0: a war map object's units. They keep their saved places unless the first stands at (0, 0), or is a
    // vehicle (category 1) and either the computer's or within the no-build border of the map's edge. Otherwise a
    // spot is looked for round the centre, ring by ring: the k x k block they stand in (k = ftol(sqrt(n) + 0.5),
    // each 2 x size apart, size the type's +0x46 word) must fit, e vertices round the spot (e = k x size in
    // vertices, rounded), on passable ground for vehicles with no unit already in the block's places, on ground a
    // building could go for the rest (0x47B610). The rings are 0.7075 x k x size apart, each with ftol(radius / step)
    // + 1 spots from angle 0; past the map's width and height the middle is used. Each unit then takes its place in
    // the block, row by row, and a heading of rand() & 0xFFF.
    // *Provisional*: the vehicles' ground check (0x47EB00 -> 0x480740, each vertex through the unit's own mover) is
    // taken as IsAreaPassable, and the other objects' (0x47FE10, the battle world's grid, 0x47FD40 for each of its
    // four layers) as circles: the size against each unit placed so far at its collision radius (Radius); the
    // scenery isn't counted; +0x188 bit 13 (a fixed place) and +0x18C aren't read.
    void BattleMap::Place(std::vector<size_t> members, int cx, int cy)
    {
        if (members.empty()) return;
        Unit& head = units[members[0]];
        auto u = setup.units->find(head.type);
        if (u == setup.units->end()) return;
        const int n = int(members.size());
        const int size = int(int16_t(u->second.currentHitPoints));
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
        if (!needs) return;
        const int W = land.WorldWidth(), H = int((int64_t(land.Height()) * cell) >> 16);
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
                    ok = land.IsAreaPassable(vx - e, vy - e, vx + e + 1, vy + e + 1);
                    // each member's place in the block, against the units placed so far
                    int bx = size + tx - span, by = size + ty - span, col = 0;
                    for (int j = 0; j < n && ok; j++)
                    {
                        for (size_t oi = 0; oi < units.size(); oi++)
                        {
                            if (std::find(members.begin(), members.end(), oi) != members.end()) continue;
                            const Unit& other = units[oi];
                            const double dx = double(other.x) - bx, dy = double(other.y) - by, reach = double(size) + Radius(other.type);
                            if (dx * dx + dy * dy < reach * reach) { ok = false; break; }
                        }
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
            x += 2 * size;
            if (++col == k) { col = 0; y += 2 * size; x = x0; }
        }
    }

    // A unit's collision radius: an IOB model's max(2 x half width, depth) x 0.66 (0x47EB10), else its type's +0x46
    double BattleMap::Radius(int type)
    {
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return 0;
        if (const GType* g = res.FindGType(u->second.field28); g && g->gclass == 1)
        {
            auto m = models.find(u->second.field28);
            if (m == models.end())
            {
                IsoModel im;
                if (auto file = res.LoadFile(g->file))
                {
                    try { im.model = IobModel::Load(*file); } catch (const std::exception&) {}
                    if (!im.model.patches.empty()) im.lights = IsoObject::BaseLights(im.model, sun, levels);
                }
                im.slot = g->paletteSlot;
                m = models.emplace(u->second.field28, std::move(im)).first;
            }
            if (!m->second.model.patches.empty())
                return double(int(float(std::max(2 * IsoObject::HalfWidth(m->second.model), IsoObject::Depth(m->second.model))) * 0.66f));
        }
        return double(int16_t(u->second.currentHitPoints));
    }

    // A battle unit's renderer by its GType's class (0x47EB10): a WOF model (2) posed at its place and heading, an
    // IOB model (1) or a sprite (0). Drawn with the ground like the scenery.
    // The unit's turrets' WOF models (0x47EE10 makes each part a child object with its renderer: 742 on the Armoured
    // Lorry), turned with the turret; posed each update at the unit's point plus the part's offset turned by the
    // unit's heading, its height the unit's plus the part's z (PoseParts)
    // *Provisional*: the offset's turn (x along the heading, y across it) and the height; the other WOF parts (the
    // lorries' 324, WOF 739, which looks to be a shadow drawn some other way) and the parts' own parts (a turret's gun
    // has a sprite, GType class 6) aren't drawn.
    void BattleMap::AddPartRenderers(size_t index)
    {
        Unit& u = units[index];
        u.attached.clear();
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        int turret = 0;
        for (const auto& part : t->second.parts)
        {
            const bool isTurret = (part.a >= 0x2E6 && part.a <= 0x313) || (part.a >= 0x389 && part.a <= 0x38C);
            const int thisTurret = isTurret ? turret++ : -1;
            auto pt = setup.units->find(part.a);
            if (pt == setup.units->end()) continue;
            const int gtype = pt->second.field28;
            const GType* g = res.FindGType(gtype);
            if (!g || g->gclass != 2 || !isTurret) continue;
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
            std::unique_ptr<WofInstance> w;
            try { w = std::make_unique<WofInstance>(f->second, scale, follow); }
            catch (const std::exception&) { continue; }
            Attached a;
            a.wof = w.get();
            a.ox = int16_t(part.b);
            a.oy = int16_t(part.c);
            a.oz = int16_t(part.d);
            a.turret = thisTurret;
            u.attached.push_back(a);
            objects.push_back({ int(u.y) + depth, 0, 0, nullptr, w.get(), g->paletteSlot, 0, -1, int(index), depth });
            wofs.push_back(std::move(w));
        }
        PoseParts(u);
    }

    void BattleMap::PoseParts(Unit& u)
    {
        if (u.attached.empty()) return;
        const double a = double(u.heading & 0xFFF) * 0.000244140625 * 6.283185307;
        const double c = std::cos(a), s = std::sin(a);
        for (const Attached& p : u.attached)
        {
            const int x = int(u.x + float(p.ox * c - p.oy * s));
            const int y = int(u.y + float(p.ox * s + p.oy * c));
            int heading = u.heading;
            if (p.turret >= 0 && size_t(p.turret) < u.turrets.size()) heading += u.turrets[size_t(p.turret)].turn;
            p.wof->Transform(land, heading & 0xFFF, x, y, int(float(land.HeightAt(x, y)) + u.drive.lift) + p.oz);
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
            auto m = models.find(gtype);
            if (m == models.end())
            {
                IsoModel im;
                auto file = res.LoadFile(g->file);
                if (!file) { models[gtype]; return; }
                try { im.model = IobModel::Load(*file); }
                catch (const std::exception&) { models[gtype]; return; }
                im.lights = IsoObject::BaseLights(im.model, sun, levels);
                im.slot = g->paletteSlot;
                m = models.emplace(gtype, std::move(im)).first;
            }
            if (m->second.model.patches.empty()) return;
            objects.push_back({ iy + IsoObject::Depth(m->second.model), ix, land.ProjectY(iy, land.HeightAt(ix, iy)), &m->second });
        }
        else if (g->gclass == 0 && res.GetFrame(gtype, 0))
            objects.push_back({ iy + 1, ix, land.ProjectY(iy, land.HeightAt(ix, iy)), nullptr, nullptr, g->paletteSlot, gtype });
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
        auto shadows = [&](const ObjOjd::UnitData& unit, float x, float y, int halfWidth, int depth) {
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
        };
        // WScenery's start (0x4896D0): after its parts, every object of a category 0, 2 or 3 type (the scenery is
        // all category 0, so all of it: 0x47D828 sets +0xB8 bit 1) puts the sun's point level (SetPointLight) on the
        // vertex it stands on, a dark spot the ground view takes off. The tree circles do too, at their centres.
        // *Provisional*: done here for the objects drawn (sprites and IOB models) and the circles; the classes of
        // the others (the ambient sound scenery) aren't checked.
        for (const SceneryObject& c : list)
            if (c.type >= 0xCF && c.type <= 0xD4) land.SetPointLight(c.x, c.y, sun);
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
            if (o.type >= 445 && o.type <= 447) { Occupy(o.type, o.x, o.y); PlaceBridge(o); continue; }
            const int gtype = u->second.field28;
            const GType* g = res.FindGType(gtype);
            if (!g || (g->gclass != 0 && g->gclass != 1)) { Occupy(o.type, o.x, o.y); continue; }
            if (g->gclass == 0)
            {
                // a sprite (RType 0x40EC50, the street lamps): drawn from its hot spot at the renderer's point, its
                // +0x26 1; the half width its shadow takes is half the sprite's width
                const IndexedImage* image = res.GetFrame(gtype, 0);
                Occupy(o.type, o.x, o.y);
                if (!image) continue;
                const int ix = int(o.x), iy = int(o.y);
                const int h = int(float(land.HeightAt(ix, iy)) + o.z);
                props.push_back({ o.type, o.x, o.y, o.z, MaxHealth(o.type) });
                objects.push_back({ iy + 1, ix, land.ProjectY(iy, h), nullptr, nullptr, g->paletteSlot, gtype, -1, -1, 0, -1, int(props.size()) - 1 });
                shadows(u->second, o.x, o.y, image->width / 2, 1);
                if (const size_t before = lights.size(); addLight(o, u->second), lights.size() > before) props.back().light = int(before);
                land.SetPointLight(ix, iy, sun);
                continue;
            }
            auto m = models.find(gtype);
            if (m == models.end())
            {
                IsoModel im;
                auto file = res.LoadFile(g->file);
                if (!file) { models[gtype]; Occupy(o.type, o.x, o.y); continue; }
                try { im.model = IobModel::Load(*file); }
                catch (const std::exception&) { models[gtype]; Occupy(o.type, o.x, o.y); continue; }
                im.lights = IsoObject::BaseLights(im.model, sun, levels);
                im.slot = g->paletteSlot;
                m = models.emplace(gtype, std::move(im)).first;
            }
            if (m->second.model.patches.empty()) { Occupy(o.type, o.x, o.y); continue; }
            const IobModel& model = m->second.model;
            // 0x47EB10: the collision radius, max(2 x half width, depth) x [Tweak] "ISO obj col rad mul" (0.66);
            // 0x47F960 keeps the object that far inside the map
            const int r = int(float(std::max(2 * IsoObject::HalfWidth(model), IsoObject::Depth(model))) * 0.66f);
            float x = o.x, y = o.y;
            if (float(worldW - r) < x) x = float(worldW - r);
            if (float(worldH - r) < y) y = float(worldH - r);
            if (float(r) > x) x = float(r);
            if (float(r) > y) y = float(r);
            Occupy(o.type, x, y);
            // 0x47F480: the renderer's point, the ground's height there plus the object's own (+0x80)
            const int ix = int(x), iy = int(y);
            const int h = int(float(land.HeightAt(ix, iy)) + o.z);
            props.push_back({ o.type, x, y, o.z, MaxHealth(o.type) });
            objects.push_back({ iy + IsoObject::Depth(model), ix, land.ProjectY(iy, h), &m->second, nullptr, 0, 0, -1, -1, 0, -1, int(props.size()) - 1 });
            shadows(u->second, x, y, (IsoObject::HalfWidth(model) - -IsoObject::HalfWidth(model)) / 2, IsoObject::Depth(model));
            if (const size_t before = lights.size(); addLight(PlacedObject{ o.type, x, y, o.z, {}, o.roll }, u->second), lights.size() > before)
                props.back().light = int(before);
            land.SetPointLight(ix, iy, sun);
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    // vt+0xE4 (0x48A950) as an object is added to the world (0x48A8B0): an object whose type is of class 0, 2 or 3
    // (+0xB8 bit 1, 0x47D828) and not a tree (0xCF-0xE5, bit 2) marks the vertices within its type's +0x46 word of
    // it blocked (0x47C280 with 2: QuickDistance from the vertex nearest it, in whole cells), which the ground's
    // cost (0x480680) then refuses.
    // *Provisional*: an RTypeISO type's +0x46 is taken from OBJ.ojd (0x47EB10 overwrites it with the model's radius
    // only for an object with a parent or vt+0xB8); nothing is taken off again yet (vt+0xE8 as it goes).
    void BattleMap::Occupy(int type, float x, float y)
    {
        if (type >= 0xCF && type <= 0xE5) return;
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return;
        const int c = u->second.category;
        if (c != 0 && c != 2 && c != 3) return;
        land.MarkOccupied(int(x), int(y), int16_t(u->second.currentHitPoints), 2);
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
        for (const auto& part : u->second.parts)
        {
            auto pu = setup.units->find(part.a);
            if (pu == setup.units->end()) continue;
            const GType* pg = res.FindGType(pu->second.field28);
            if (pg && pg->gclass == 2) { g = pg; gtype = pu->second.field28; break; }
        }
        if (!g) return;
        auto file = res.LoadFile(g->file);
        if (!file) return;
        float scale = 65536.0f;
        int follow = 0, depth = 0;
        if (setup.six)
            if (auto six = setup.six->find(gtype); six != setup.six->end())
            {
                scale = six->second.a;
                follow = six->second.c;
                depth = 2 * int16_t(six->second.d); // the renderer's +0x26 (0x49DB1C)
            }
        const Bridge bridge = Bridge::Create(land, o.extra[0], o.extra[1], o.extra[2], o.extra[3]);
        AddBridgeNodes(bridge);
        const size_t first = wofs.size();
        for (int i = 0; i < bridge.Count(); i++)
        {
            int x = 0, y = 0;
            bridge.SegmentAt(i, o.x, o.y, x, y);
            std::unique_ptr<WofInstance> w;
            try { w = std::make_unique<WofInstance>(*file, scale, follow); }
            catch (const std::exception&) { break; }
            w->Transform(land, bridge.Heading(), x, y, land.HeightAt(x, y));
            objects.push_back({ y + depth, 0, 0, nullptr, w.get(), g->paletteSlot });
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
        popupHover = -1;
        for (size_t i = 0; i < popup.size(); i++) if (popup[i].rect.Contains(x, y)) popupHover = int(i);
    }

    // 0x484DF0: with the mouse within 4 pixels of an edge (32 in a window), the view moves by the mouse's distance
    // from the centre over (half the width / 40), both ways at once
    void BattleMap::EdgeScroll()
    {
        if (mouseX < 0 || !popup.empty()) return;
        const int margin = 4, W = 640, H = 480;
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
        // the listener (dword_4E9250 / dy): the view's centre
        if (setup.sound) setup.sound->SetListener(scrollX + 320, scrollY + 240);
        // 0x80A0 arriving (posted by 0x48AB20)
        for (size_t i = 0; i < units.size();)
            if (units[i].dieAt >= 0 && ticks >= units[i].dieAt) Die(i);
            else i++;
        // the right button held for [Tweak] "Quick Cancel Timeout" updates opens the popup (0x805B -> 0x485DE0)
        if (rightDownAt >= 0 && popup.empty() && ticks - rightDownAt == setup.quickCancelTimeout && mouseX >= 0)
            OpenPopup(mouseX, mouseY);
        EdgeScroll();
        CheckBattleEnd();
        AiUpdate();
        for (size_t g = 0; g < groupTicks.size(); g++) GroupTick(int(g));
        ChaseTargets();
        bool moved = false;
        for (size_t i = 0; i < units.size(); i++)
        {
            const float x = units[i].x, y = units[i].y;
            Drive(i);
            // a retreating unit at its reinforcement point leaves the map (*provisional*: within 256 of it, |dx| + |dy|, the group spreading as it arrives)
            if (const int g = units[i].group; g >= 0 && size_t(g) < groupRetreat.size() && groupRetreat[size_t(g)])
            {
                const int k = NearestPoint(int(units[i].x), int(units[i].y), -1);
                if (k >= 0 && std::abs(int(units[i].x) - rpoints[size_t(k)].x) + std::abs(int(units[i].y) - rpoints[size_t(k)].y) <= 256 &&
                    rpoints[size_t(k)].sector == groupRetreat[size_t(g)])
                {
                    LeaveMap(i);
                    i--;
                    continue;
                }
            }
            StatusTick(units[i]);
            LookRound(i);
            Smoulder(i);
            moved |= units[i].x != x || units[i].y != y;
        }
        // the guns (their own updates, each a child of its unit or its turret)
        for (size_t i = 0; i < units.size(); i++)
        {
            for (Turret& t : units[i].turrets) TurretTick(units[i], t);
            for (Gun& g : units[i].guns) GunTick(i, g);
            PoseParts(units[i]);
        }
        // the scenery: 0x80A0 arriving, the damage smoke
        for (size_t i = 0; i < props.size(); i++)
        {
            if (!props[i].alive) continue;
            if (props[i].dieAt >= 0 && ticks >= props[i].dieAt) { PropDies(i); continue; }
            SmoulderProp(i);
        }
        // the flying objects (effects, smoke, shells)
        UpdateEffects();
        UpdateLights();
        if (moved)
        {
            for (ViewObject& o : objects)
                if (o.unit >= 0) o.key = int(units[size_t(o.unit)].y) + o.depth;
            std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
        }
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
        // then the selection's: its first member's orders
        for (const Unit& u : units)
            if (u.players && selected[size_t(u.group)]) { AddOrders(u); break; }
        int w = 0, h = 0;
        for (const PopupItem& item : popup) { w = std::max(w, item.rect.w); h += item.rect.h; }
        int left = std::clamp(x - popup[0].rect.w / 2, 3, 640 - 3 - w);
        int top = std::clamp(y - popup[0].rect.h / 4, 3, 480 - 3 - h);
        for (PopupItem& item : popup) { item.rect.x = left; item.rect.y = top; top += item.rect.h; }
        sounds.push_back(0x193);
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
        if (escape) sounds.push_back(0x196);
    }

    std::optional<Command> BattleMap::OnMouseButton(int x, int y, bool down)
    {
        if (endBox)
        {
            if (!down && EndBoxButtonRect().Contains(x, y)) return EndBoxCommand();
            return std::nullopt;
        }
        if (!down)
        {
            const bool wasMini = miniDrag;
            miniDrag = false;
            if (dragging && !wasMini) ReleaseLeft(x, y);
            dragging = false;
            return std::nullopt;
        }
        if (popup.empty() && down && InMinimap(x, y))
        {
            miniDrag = true;   // the minimap takes the mouse (0x42E9B0) until the button's let go
            MinimapScroll(x, y);
            return std::nullopt;
        }
        if (popup.empty())
        {
            // 0x201 (cursor modes 0 and 1): a drag rectangle starts here (0x49C230) and the view keeps the mouse
            dragging = true;
            boxActive = false;
            dragX0 = dragX1 = x;
            dragY0 = dragY1 = y;
            return std::nullopt;
        }
        for (const PopupItem& item : popup)
            if (item.rect.Contains(x, y))
            {
                int command = item.command;
                const int param = item.param;
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
        ClosePopup(false);
        return std::nullopt;
    }

    // 0x204 / 0x205: held, the popup (above); let go quickly, 0x73 (Cancel) to the selection
    std::optional<Command> BattleMap::OnRightButton(int, int, bool down)
    {
        if (!popup.empty()) { rightDownAt = -1; return std::nullopt; }
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
        for (size_t i = 0; i < popup.size(); i++) if (popup[i].rect.Contains(x, y)) return 100 + int(i);
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
        auto between = [&](int limit) {
            for (; next < objects.size() && objects[next].key < limit; next++)
            {
                const ViewObject& o = objects[next];
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
                    o.wof->Draw(sun, *shade, t, -scrollX, -scrollY);
                    continue;
                }
                if (o.sprite)
                {
                    // 0x40EE10: the frame at the point less the hot spot, through the slot's table at level 0
                    const int frame = o.effect >= 0 ? effects[size_t(o.effect)].counter >> 8 : 0;
                    const IndexedImage* image = res.GetFrame(o.sprite, frame);
                    const GType* g = res.FindGType(o.sprite);
                    if (image && g) canvas.DrawImage(*image, shade->Level(0), o.x - g->hotX - scrollX, o.y - g->hotY - scrollY);
                    continue;
                }
                const IobModel& m = o.model->model;
                IsoObject::Draw(m, o.model->lights, o.x - IsoObject::HalfWidth(m) - scrollX, o.y + IsoObject::OffsetY(m) - scrollY,
                                *shade, dither, t);
            }
        };
        TerrainView::Render(land, tiles, table, levels, dither, t, -scrollX, -scrollY, nullptr, between, &window);
        DrawSelection(canvas);
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
    // war map objects waiting off the map (+0x54 / +0x58, 0x4216D0). The player's at 0: the report filled (0x43A800)
    // and the box "Sector Lost" (0x49D) when the player held the sector (the report's +0x6C), else "Attack Failed"
    // (0x49B), its button 0x76. The other side's at 0: the report filled, and "Invaders Repelled!" (0x49C) with
    // 0x8024 when the player held it, else "Attack Successful!" (0x49A), with 0x76 if that ends the game (+0x55:
    // sector 3 taken by the Humans with no Martian base left, dword_4EB05C, or London by the Martians, dword_4EC49C),
    // else 0x8024.
    // *Provisional*: the units are what's counted (the game counts every object with a side in the battle's list);
    // the objects waiting off the map (reinforcements) aren't in; the report's +0x6C is taken as the side that held
    // the sector as the battle began; the game's end is taken as sector 3 won by a Human player or 30 by a Martian.
    void BattleMap::CheckBattleEnd()
    {
        if (!battleOn || battleDecided) return;
        int ours = 0, theirs = 0;
        for (const Unit& u : units) (u.players ? ours : theirs)++;
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
            for (const Unit& u : units) if (u.group == int(g)) { left = true; break; }
            if (!left) out.push_back(setup.groups[g].ref);
        }
        return out;
    }
}
