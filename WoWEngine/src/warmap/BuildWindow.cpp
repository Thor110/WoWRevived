// The Build List window (0x460B00, "Build List", class 0x804A) for one object's factory, opened from the war map's
// popup (Build, 0x813E with 0x8065, 0x428E41: the factory's war map mode +0x20 set, then its Open, 0x45FA80). See
// docs/WARMAP.md "The Build List window".
#include "warmap/WarMap.h"

#include <cstdio>

namespace wow
{
    namespace
    {
        constexpr int ItemSprite = 0x78, SlotSprite = 0x76, RepeatSprite = 0x6B;
        constexpr int ListBack = 0x2FA, QueueBack = 0x2FB, Promote = 0x2FC;
        constexpr int ListUp = 0x2FD, ListDown = 0x2FE, QueueUp = 0x2FF, QueueDown = 0x300;
        constexpr int ItemFont = 0x6E, TtcFont = 0x70;
        // word_4CA7B8, 4 bytes apart, by the repeat count: the badge's frame (none below 2)
        constexpr int RepeatFrame[5] = { 0, 0, 1, 2, 5 };
    }

    WarMap::BuildLayout WarMap::BuildWindowLayout() const
    {
        BuildLayout l;
        if (!buildWindow) return l;
        const bool human = res.Human();
        const int left = buildLeft, top = buildTop;
        // the list panel (0x461500) at Left, Top: the backdrop, its width less 42 (Human) or 8, and its two
        // scroll buttons
        Rect back = SpriteRect(ListBack, left, top);
        l.list = back;
        l.listUp = SpriteRect(ListUp, left + (human ? 0x1B : 0x1F), top + (human ? 9 : 0x15));
        l.listDown = SpriteRect(ListDown, left + (human ? 0x1A : 0x1F), top + (human ? 0x12C : 0x12B));
        // the items (0x4619A0): five from the scroll position, at x 32, y from 46, each the last's bottom + 2
        const auto options = BuildOptions(buildWindow->ref);
        int y = top + 0x2E;
        for (int i = 0; i < 5 && size_t(buildWindow->scroll + i) < options.size(); i++)
        {
            const int type = options[size_t(buildWindow->scroll + i)];
            auto u = unitData.find(type);
            const IndexedImage* img = res.GetFrame(ItemSprite, u == unitData.end() ? 0 : u->second.counterFrame);
            Rect r{ left + 0x20, y, img ? img->width : 48, img ? img->height : 48 };
            l.items.push_back({ type, r });
            y = r.y + r.h + 2;
        }
        // the queue panel (0x460F10) at Left + 0x58, Top + 0x71: the head's slot, four more, the two scroll buttons
        const int qx = left + 0x58, qy = top + 0x71;
        l.queue = SpriteRect(QueueBack, qx, qy);
        l.queueUp = SpriteRect(QueueUp, qx + 6, qy + (human ? 0x34 : 0x38));
        l.queueDown = SpriteRect(QueueDown, qx + (human ? 5 : 6), qy + (human ? 0xAC : 0xAA));
        l.ttc = { qx + 0x14, qy + 0x29 };
        auto f = factories.find(buildWindow->ref);
        const std::vector<BuildEntry> empty;
        const auto& q = f == factories.end() ? empty : f->second.queue.entries;
        Rect slot = SpriteRect(SlotSprite, qx + 8, qy + (human ? 6 : 0xD));
        l.slots.push_back({ q.empty() ? -1 : 0, slot });
        int sx = qx + (human ? 0xA : 8), sy = qy + (human ? 0x45 : 0x44);
        for (int i = 1; i <= 4; i++)
        {
            const int index = buildWindow->queueScroll + i; // 0x461490
            Rect r = SpriteRect(SlotSprite, sx, sy);
            l.slots.push_back({ size_t(index) < q.size() ? index : -1, r });
            sy = r.y + r.h + 1;
        }
        return l;
    }

    void WarMap::OpenBuildWindow(int ref)
    {
        if (factories.find(ref) == factories.end()) return;
        buildWindow = BuildWindowState{ ref };
    }

    void WarMap::DrawBuildWindow(Canvas& canvas) const
    {
        if (!buildWindow) return;
        auto f = factories.find(buildWindow->ref);
        if (f == factories.end()) return;
        const BuildLayout l = BuildWindowLayout();
        const bool human = res.Human();
        canvas.DrawSprite(ListBack, 0, l.list.x, l.list.y);
        canvas.DrawSprite(ListUp, 0, l.listUp.x, l.listUp.y);
        canvas.DrawSprite(ListDown, 0, l.listDown.x, l.listDown.y);
        // each item (0x462A40): its counter picture lit while the mouse is on it, the build time in days
        // ("%0.1f": the type's days x its battle units, slowed by the sector's supply, 0x460740) in font 0x6E at
        // its bottom right; one that can't be ordered shows the picture 0x96 frames on, and no time
        const int at = CounterOf(buildWindow->ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : 0;
        float m = 1.0f;
        if (sector >= 1 && sector <= warmap::SectorCount)
        {
            const auto& r = economy[size_t(sector)].ratio;
            const size_t k = size_t(SlotBase(at < 0 || counters[size_t(at)].players));   // (MODERN: the side's slots)
            m = r[k] * r[k + 1] * r[k + 2];
        }
        for (size_t i = 0; i < l.items.size(); i++)
        {
            const auto& [type, r] = l.items[i];
            auto u = unitData.find(type);
            const int frame = u == unitData.end() ? 0 : u->second.counterFrame;
            if (!ItemEnabled(buildWindow->ref, type))
            {
                canvas.DrawSprite(ItemSprite, frame + 0x96, r.x, r.y);
                continue;
            }
            canvas.DrawSprite(ItemSprite, frame, r.x, r.y, buildWindow->hover == 200 + int(i) ? 1 : 0);
            if (u == unitData.end()) continue;
            const float days = float(u->second.battleUnits) * ObjOjd::BuildTable(u->second, human, manufacture) / (1.0f / m);
            char text[32];
            std::snprintf(text, sizeof(text), "%0.1f", double(days));
            const Font* font = res.GetFont(ItemFont);
            const int th = font ? font->Height() : 8;
            canvas.DrawString(ItemFont, text, { r.x, r.y + r.h - th, r.w, th }, TextRight);
        }
        canvas.DrawSprite(QueueBack, 0, l.queue.x, l.queue.y);
        canvas.DrawSprite(QueueUp, 0, l.queueUp.x, l.queueUp.y);
        canvas.DrawSprite(QueueDown, 0, l.queueDown.x, l.queueDown.y);
        // the slots (0x4624A0): the entry's counter picture (lit under the mouse) and, with more than one to make,
        // the repeat badge (GType 0x6B) at its bottom right; the others' promote button (0x2FC)
        const auto& q = f->second.queue.entries;
        for (size_t i = 0; i < l.slots.size(); i++)
        {
            const auto& [index, r] = l.slots[i];
            if (index < 0) continue;
            const BuildEntry& e = q[size_t(index)];
            auto u = unitData.find(e.type);
            canvas.DrawSprite(SlotSprite, u == unitData.end() ? 0 : u->second.counterFrame, r.x, r.y,
                              buildWindow->hover == 300 + int(i) ? 1 : 0);
            if (e.repeat > 1 && e.repeat <= 4)
            {
                const IndexedImage* badge = res.GetFrame(RepeatSprite, RepeatFrame[e.repeat]);
                if (badge) canvas.DrawSprite(RepeatSprite, RepeatFrame[e.repeat], r.x + r.w - badge->width, r.y + r.h - badge->height);
            }
            if (index != 0) canvas.DrawSprite(Promote, 0, PromoteRect(r).x, PromoteRect(r).y);
        }
        // "TTC" (0x461340): when the head will be done, its hours left slowed by the supply, as a route's arrival
        // is shown (0x41B9A0), centred at (0x14, 0x29) in font 0x70
        if (!q.empty())
        {
            std::string ttc = warmap::ArrivalText(game.dateTime, q.front().hours * m, human,
                [&](int month) { return res.String(warmap::MonthText(month)); });
            canvas.DrawString(TtcFont, ttc, { l.ttc.first - 40, l.ttc.second, 80, 12 }, TextCentreX);
        }
    }

    // The promote button's place in a slot: its top right (*provisional*: 0x4621C0 aligns it with 0x42F020(9))
    Rect WarMap::PromoteRect(const Rect& slot) const
    {
        Rect p = SpriteRect(Promote, 0, 0);
        return { slot.x + slot.w - p.w, slot.y, p.w, p.h };
    }

    int WarMap::BuildWindowHit(int x, int y) const
    {
        if (!buildWindow) return -1;
        const BuildLayout l = BuildWindowLayout();
        if (l.listUp.Contains(x, y)) return 1;
        if (l.listDown.Contains(x, y)) return 2;
        if (l.queueUp.Contains(x, y)) return 3;
        if (l.queueDown.Contains(x, y)) return 4;
        for (size_t i = 0; i < l.items.size(); i++) if (l.items[i].second.Contains(x, y)) return 200 + int(i);
        for (size_t i = 0; i < l.slots.size(); i++)
            if (l.slots[i].first >= 0)
            {
                if (l.slots[i].first != 0 && PromoteRect(l.slots[i].second).Contains(x, y)) return 400 + int(i);
                if (l.slots[i].second.Contains(x, y)) return 300 + int(i);
            }
        if (l.list.Contains(x, y) || l.queue.Contains(x, y)) return 0;
        return -1;
    }

    // A click in the window: the scroll buttons move the list (0x7E25 / 0x2FD, 0x2FE: one at a time, while
    // there are more than five past it) or the queue (0x2FF / 0x300); an item orders it (0x460D70: a unit with
    // 0x17 -> 0x4603B0, a construction with 0x16 -> 0x460240, the war map placing nothing); a slot takes a repeat
    // off or the entry out (0x4625B0, either button); its promote button moves it to the front (0x4605A0); the
    // window's own background closes it (0x201 / 0x204 in war map mode). Elsewhere the map has the mouse.
    // In battle mode (opened from the battle map, the factory's +0x20 clear) a construction's item starts the
    // placement instead (0x460D70: 0x42E790 with cursor mode 4, or 6 for the fences and walls 0x1C3-0x1C8, 5 for
    // the military bridge 0x1BC), the background doesn't close the window (0x460EF1), and a slot's cancel kills
    // the construction on the battle map (0x462120: 0x80A0) rather than taking it from the sector.
    BuildHost::Click WarMap::BuildWindowAct(int x, int y, bool down)
    {
        Click result;
        const int hit = BuildWindowHit(x, y);
        if (hit < 0) return result;
        result.taken = true;
        const bool battle = buildWindow->battle;
        if (down && hit == 0) { if (!battle) buildWindow.reset(); return result; } // 0x201 / 0x204 on the window itself
        if (down) { buildWindow->pressed = hit; return result; }
        const int was = std::exchange(buildWindow->pressed, -1);
        if (was != hit) return result;
        const int ref = buildWindow->ref;
        const auto options = BuildOptions(ref);
        auto f = factories.find(ref);
        const size_t queued = f == factories.end() ? 0 : f->second.queue.entries.size();
        if (hit == 1) { if (buildWindow->scroll > 0) buildWindow->scroll--; return result; }
        if (hit == 2) { if (size_t(buildWindow->scroll + 5) < options.size()) buildWindow->scroll++; return result; }
        if (hit == 3) { if (buildWindow->queueScroll > 0) buildWindow->queueScroll--; return result; }
        if (hit == 4) { if (size_t(buildWindow->queueScroll + 5) < queued) buildWindow->queueScroll++; return result; }
        const BuildLayout l = BuildWindowLayout();
        if (hit >= 200 && hit < 300)
        {
            const int type = l.items[size_t(hit - 200)].first;
            if (!ItemEnabled(ref, type)) return result;
            auto u = unitData.find(type);
            if (u != unitData.end() && u->second.category == 1) AddType(ref, type);
            else if (u != unitData.end() && u->second.category <= 3)
            {
                if (battle) result.placeType = type;
                else OrderConstruction(ref, type);
            }
            return result;
        }
        if (hit >= 300 && hit < 400) { result.killRef = SlotClicked(ref, l.slots[size_t(hit - 300)].first); return result; }
        if (hit >= 400) { PromoteEntry(ref, l.slots[size_t(hit - 400)].first); return result; }
        return result;
    }

    // The tool tips (0x807C): an item's name, "Requires <building>" (0x51E) when it has a prerequisite, and
    // "This item can only be constructed on the BattleMap" (0x494) for a constructor vehicle's (0x462B50); a
    // slot's, the name (0x4625B0)
    std::string WarMap::BuildWindowTip(int hit) const
    {
        if (!buildWindow) return {};
        const BuildLayout l = BuildWindowLayout();
        int type = 0;
        if (hit >= 200 && hit < 300 && size_t(hit - 200) < l.items.size()) type = l.items[size_t(hit - 200)].first;
        else if (hit >= 300 && hit < 400)
        {
            auto f = factories.find(buildWindow->ref);
            const int index = l.slots[size_t(hit - 300)].first;
            if (f != factories.end() && index >= 0) type = f->second.queue.entries[size_t(index)].type;
        }
        auto u = unitData.find(type);
        if (u == unitData.end()) return {};
        std::string text = res.String(u->second.nameText);
        if (hit >= 300) return text;
        if (u->second.prerequisite)
        {
            auto p = unitData.find(u->second.prerequisite);
            text += "\n" + res.String(0x51E) + " " + (p == unitData.end() ? std::string() : res.String(p->second.nameText));
        }
        bool mapOnly = false;
        ItemEnabled(buildWindow->ref, type, &mapOnly);
        if (mapOnly) text += "\n" + res.String(0x494);
        return text;
    }
}
