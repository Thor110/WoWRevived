#include "battlemap/BattleMap.h"
#include "frontend/Canvas.h"
#include "frontend/Resources.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <algorithm>
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
        Landscape::Sun sun = Landscape::SunFor(angle, set);
        sun.shade = Landscape::ShadeLevel(levels, set[2], 0.0f);
        land.LightTheMap(sun);
        dither = DitherTables::Build(Need(res, "dat\\dith.dat"));

        // The start view (0x439B7E): on the player's HQ (the Human constructions list is searched for type 278 in
        // London or 314 elsewhere in a Human game, 830 in sector 3 or 120 elsewhere in a Martian one, and centred
        // on its units, 0x42A2A0 / 0x42A340), else on the deployment's centre (dword_4F742C / 4F7434), else on the
        // map's middle; projected (with the height there), less half the screen. *Not yet*: the battle map has no
        // objects or deployment, so it's always the middle.
        int cx = land.WorldWidth() / 2;
        int cy = (land.Height() * land.Cell() >> 16) / 2;
        ScrollTo(cx - 320, land.ProjectY(cy, land.HeightAt(cx, cy)) - 240);
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
        TerrainView::Render(land, tiles, table, levels, dither, t, -scrollX, -scrollY);
        // the popup over everything, each item's next frame under the mouse
        for (size_t i = 0; i < popup.size(); i++)
        {
            const PopupItem& item = popup[i];
            bool lit = int(i) == popupHover && res.GetFrame(item.icon, 1);
            canvas.DrawSprite(item.icon, lit ? 1 : 0, item.rect.x, item.rect.y);
        }
    }
}
