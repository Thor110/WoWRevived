#include "battlemap/BattleMap.h"
#include "frontend/Canvas.h"
#include "frontend/Resources.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
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
        if (s.loadScenery) LoadScenery(sun, letter);

        // The start view (0x439B7E): on the player's HQ (the Human constructions list is searched for type 278 in
        // London or 314 elsewhere in a Human game, 830 in sector 3 or 120 elsewhere in a Martian one, and centred
        // on its units, 0x42A2A0 / 0x42A340), else on the deployment's centre (dword_4F742C / 4F7434), else on the
        // map's middle; projected (with the height there), less half the screen. *Not yet*: the battle map has no
        // objects or deployment, so it's always the middle.
        int cx = land.WorldWidth() / 2;
        int cy = (land.Height() * land.Cell() >> 16) / 2;
        ScrollTo(cx - 320, land.ProjectY(cy, land.HeightAt(cx, cy)) - 240);
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
        for (const PlacedObject& o : PlaceScenery(list, setup.enemyCount, radius))
        {
            auto u = setup.units->find(o.type);
            if (u == setup.units->end()) continue;
            if (o.type >= 445 && o.type <= 447) { PlaceBridge(o); continue; }
            const int gtype = u->second.field28;
            const GType* g = res.FindGType(gtype);
            if (!g) continue;
            if (g->gclass == 0)
            {
                // a sprite (RType 0x40EC50, the street lamps): drawn from its hot spot at the renderer's point, its
                // +0x26 1; the half width its shadow takes is half the sprite's width
                const IndexedImage* image = res.GetFrame(gtype, 0);
                if (!image) continue;
                const int ix = int(o.x), iy = int(o.y);
                const int h = int(float(land.HeightAt(ix, iy)) + o.z);
                objects.push_back({ iy + 1, ix, land.ProjectY(iy, h), nullptr, nullptr, g->paletteSlot, gtype });
                shadows(u->second, o.x, o.y, image->width / 2, 1);
                land.SetPointLight(ix, iy, sun);
                continue;
            }
            if (g->gclass != 1) continue;
            auto m = models.find(gtype);
            if (m == models.end())
            {
                IsoModel im;
                auto file = res.LoadFile(g->file);
                if (!file) { models[gtype]; continue; }
                try { im.model = IobModel::Load(*file); }
                catch (const std::exception&) { models[gtype]; continue; }
                im.lights = IsoObject::BaseLights(im.model, sun, levels);
                im.slot = g->paletteSlot;
                m = models.emplace(gtype, std::move(im)).first;
            }
            if (m->second.model.patches.empty()) continue;
            const IobModel& model = m->second.model;
            // 0x47EB10: the collision radius, max(2 x half width, depth) x [Tweak] "ISO obj col rad mul" (0.66);
            // 0x47F960 keeps the object that far inside the map
            const int r = int(float(std::max(2 * IsoObject::HalfWidth(model), IsoObject::Depth(model))) * 0.66f);
            float x = o.x, y = o.y;
            if (float(worldW - r) < x) x = float(worldW - r);
            if (float(worldH - r) < y) y = float(worldH - r);
            if (float(r) > x) x = float(r);
            if (float(r) > y) y = float(r);
            // 0x47F480: the renderer's point, the ground's height there plus the object's own (+0x80)
            const int ix = int(x), iy = int(y);
            const int h = int(float(land.HeightAt(ix, iy)) + o.z);
            objects.push_back({ iy + IsoObject::Depth(model), ix, land.ProjectY(iy, h), &m->second });
            shadows(u->second, x, y, (IsoObject::HalfWidth(model) - -IsoObject::HalfWidth(model)) / 2, IsoObject::Depth(model));
            land.SetPointLight(ix, iy, sun);
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
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
    }

    // 0x485610: each press (and repeat) of a key is one step, sent as 0x7A with the view's new top left
    std::optional<Command> BattleMap::OnKey(int key)
    {
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
        // *Not yet*: the digits (select group n, packet 0x12), Backspace (halt, 0x11), Tab (0x80C8, the panel)
        // and Home (centre on the selection, 0x43A480)
        default: break;
        }
        return std::nullopt;
    }

    void BattleMap::OnKeyUp(int) {}

    void BattleMap::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
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

    void BattleMap::Update()
    {
        ticks++;
        // the right button held for [Tweak] "Quick Cancel Timeout" updates opens the popup (0x805B -> 0x485DE0)
        if (rightDownAt >= 0 && popup.empty() && ticks - rightDownAt == setup.quickCancelTimeout && mouseX >= 0)
            OpenPopup(mouseX, mouseY);
        EdgeScroll();
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
        add(ReturnCommand, 0x2C, 0x47A); // no objects on the map yet, so one side always has none
        int w = 0, h = 0;
        for (const PopupItem& item : popup) { w = std::max(w, item.rect.w); h += item.rect.h; }
        int left = std::clamp(x - popup[0].rect.w / 2, 3, 640 - 3 - w);
        int top = std::clamp(y - popup[0].rect.h / 4, 3, 480 - 3 - h);
        for (PopupItem& item : popup) { item.rect.x = left; item.rect.y = top; top += item.rect.h; }
        sounds.push_back(0x193);
    }

    void BattleMap::ClosePopup(bool escape)
    {
        popup.clear();
        popupHover = -1;
        if (escape) sounds.push_back(0x196);
    }

    std::optional<Command> BattleMap::OnMouseButton(int x, int y, bool down)
    {
        if (popup.empty() || !down) return std::nullopt;
        for (const PopupItem& item : popup)
            if (item.rect.Contains(x, y))
            {
                int command = item.command;
                ClosePopup(false);
                if (command == ReturnCommand) return Command{ ReturnCommand, setup.sector };
                return std::nullopt; // 0x73, Cancel: nothing selected yet
            }
        ClosePopup(false);
        return std::nullopt;
    }

    // 0x205: let go quickly it's 0x73, Cancel (nothing to cancel yet)
    std::optional<Command> BattleMap::OnRightButton(int, int, bool down)
    {
        if (!popup.empty()) { rightDownAt = -1; return std::nullopt; }
        if (down) rightDownAt = ticks;
        else rightDownAt = -1;
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
        auto between = [&](int limit) {
            for (; next < objects.size() && objects[next].key < limit; next++)
            {
                const ViewObject& o = objects[next];
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
                    const IndexedImage* image = res.GetFrame(o.sprite, 0);
                    const GType* g = res.FindGType(o.sprite);
                    if (image && g) canvas.DrawImage(*image, shade->Level(0), o.x - g->hotX - scrollX, o.y - g->hotY - scrollY);
                    continue;
                }
                const IobModel& m = o.model->model;
                IsoObject::Draw(m, o.model->lights, o.x - IsoObject::HalfWidth(m) - scrollX, o.y + IsoObject::OffsetY(m) - scrollY,
                                *shade, dither, t);
            }
        };
        TerrainView::Render(land, tiles, table, levels, dither, t, -scrollX, -scrollY, nullptr, between);
        // the popup over everything, each item's next frame under the mouse
        for (size_t i = 0; i < popup.size(); i++)
        {
            const PopupItem& item = popup[i];
            bool lit = int(i) == popupHover && res.GetFrame(item.icon, 1);
            canvas.DrawSprite(item.icon, lit ? 1 : 0, item.rect.x, item.rect.y);
        }
    }
}
