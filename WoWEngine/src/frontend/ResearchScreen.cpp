#include "frontend/ResearchScreen.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>

namespace wow
{
    namespace
    {
        constexpr int Backdrop = 0x85;
        constexpr int ResearchPalette = 0x2EF; // hr
        constexpr int ExitSprite = 0x7C;
        constexpr int TextFont = 0x72;
        constexpr int PlanCommand = 0xBE, PageCommand = 0xBF;
    }

    ResearchScreen::ResearchScreen(Resources& res, Research& research, std::map<int, ObjOjd::ResearchPageInfo> pageInfo,
                                   Colours colours, int openRate)
        : Screen(Id, res, Backdrop, ResearchPalette), research(research), pageInfo(std::move(pageInfo)), colours(colours),
          openRate(openRate)
    {
        // 0x458150: the cabinet opens (0x19C) and the folders settle before the view is made
        sounds.push_back(0x19C);
        research.SettlePlans(openRate);
        const bool human = res.Human();
        exitRect = { human ? 0x1AF : 0x1B7, human ? 0x1A4 : 0x1BD, SpriteWidth(ExitSprite), SpriteHeight(ExitSprite) };
        Layout();
        BuildDetail();
    }

    int ResearchScreen::SpriteWidth(int gtype) const
    {
        const IndexedImage* f = res.GetFrame(gtype, 0);
        return f ? int(f->width) : 0;
    }

    int ResearchScreen::SpriteHeight(int gtype) const
    {
        const IndexedImage* f = res.GetFrame(gtype, 0);
        return f ? int(f->height) : 0;
    }

    // 0x459200 / 0x45C9A0 / 0x45E550: each shown plan at (x, y), its click area at (x + 0xE, y) 0xC3 x 0x16 (none
    // for the open plan) Human, (x + 0x23, y) 0xF0 x 0x20 Martian; y on by 0x16 / 0x20 (and on a Martian folder open
    // past 0x20, by the rest); each visible page unrolled at all at (x + 3 / x + 0x28, y), its click area 0x1D6 x
    // 0x14 / 0xF8 x 0x18, y on by its height (and Human x on by a tenth of it). Then y on from where the last page
    // started by how far the folder is open (Martian at most 0x20), and Human x on by 2.
    void ResearchScreen::Layout()
    {
        const bool human = res.Human();
        const auto& plans = research.Plans();
        const auto& pages = research.Pages();
        planPos.assign(plans.size(), { 0, 0 });
        pagePos.assign(pages.size(), { 0, 0 });
        hotspots.clear();
        int x = human ? 32 : 4, y = human ? 30 : 10;
        for (int p = 0; p < int(plans.size()); p++)
        {
            if (!research.PlanShown(p)) continue;
            const Research::Plan& plan = plans[size_t(p)];
            planPos[size_t(p)] = { x, y };
            if (human) hotspots.push_back({ { x + 0xE, y, p == research.OpenPlan() ? 0 : 0xC3, 0x16 }, PlanCommand, p });
            else hotspots.push_back({ { x + 0x23, y, 0xF0, 0x20 }, PlanCommand, p });
            y += human ? 0x16 : 0x20;
            if (!human && plan.open > 0x20) y += plan.open - 0x20;
            int lastX = x, lastY = y;
            for (int g : plan.pages)
            {
                const Research::Page& page = pages[size_t(g)];
                if (!research.PageVisible(g) || page.height <= 0) continue;
                lastX = x;
                lastY = y;
                const int px = x + (human ? 3 : 0x28);
                pagePos[size_t(g)] = { px, y };
                hotspots.push_back({ { px, y, human ? 0x1D6 : 0xF8, human ? 0x14 : 0x18 }, PageCommand, g });
                y += page.height;
                if (human) x += page.height / 10;
            }
            x = lastX;
            y = lastY + (human ? plan.open : std::min(plan.open, 0x20));
            if (human) x += 2;
        }
    }

    // The page's detail (0x45D3F0 Human, 0x45DD50 Martian). The blueprint window (0x45F4B0) at (0x118, 0x16) Human or
    // (0x143, 0x2F) Martian, as big as the page's picture: res_det (0x8C, Human) and the picture, then its texts
    // (font 0x72), each a window of its own inside it, w being the picture's width:
    //   Human: each text centred in w - 0x4A from 0x36 ((w - its width - 0x4A) / 2 + 0x36); the title at y 0x12; the
    //          scientist's text wrapped to w - 0x4A at y = the title's height + 0x18; then y = that text's y and height
    //          + 0x12; "Specifications:" (0x462) if anything on the page of kind 0 or 1 is done (state 2), y on by its
    //          height + 6, then each such item's record 42 text, wrapped, y on by its height, and 0x12 after the
    //          section; "Enhancements:" (0x463) the same for done enhancements
    //   Martian: each text centred in the picture (flag 0x10); the title at y 6; the scientist's text wrapped to
    //          w - 8 at y = the title's height + 9; then gaps of 6 after it, 3 after a heading and 6 after a section
    // Colours: the title "Research Heading", the scientist "Research Detail", the headings "Research Heading", the
    // items "Research Specification" / "Research Enhancement". With anything available, the proposals list
    // (0x45F140): res_prop / mr_prop (0x90) at (0x12A, 0x114) Human or (0x144, 0x123) Martian, its "Proposals For
    // research:" (0x464) centred at y 0xA, then from 0x10 (Martian 0xD) below that each available item (0x45EEA0:
    // its name and time, 0x45B6D0) centred, one under another; coloured "Research Current" for the research being
    // done, "Research New" for one not looked at, else "Research Proposal"; a click on one researches it (0x84).
    void ResearchScreen::BuildDetail()
    {
        detail.clear();
        proposals = {};
        const int g = research.ShownPage();
        if (g < 0 || size_t(g) >= research.Pages().size()) return;
        const Font* font = res.GetFont(TextFont);
        if (!font) return;
        const bool human = res.Human();
        const Research::Page& page = research.Pages()[size_t(g)];
        auto info = pageInfo.find(page.number);
        const int picture = info != pageInfo.end() ? info->second.picture : 0;
        const int w = picture ? SpriteWidth(picture) : 0;
        blueprint = { human ? 0x118 : 0x143, human ? 0x16 : 0x2F, w, picture ? SpriteHeight(picture) : 0 };
        // a text window: its lines, size and place (centred as the side does it)
        auto add = [&](const std::string& text, int wrap, int y, int level, int item, Rect area) {
            Line l;
            l.text = wrap > 0 ? WrapText(*font, text, wrap) : text;
            int lines = 1, widest = 0;
            size_t at = 0;
            for (;;)
            {
                size_t nl = l.text.find('\n', at);
                widest = std::max(widest, font->Measure(std::string_view(l.text).substr(at, nl == std::string::npos ? std::string::npos : nl - at)));
                if (nl == std::string::npos) break;
                lines++;
                at = nl + 1;
            }
            l.w = widest;
            l.h = lines * font->Height() + (lines - 1) * 2; // RTypeText's height (0x40FBF0)
            l.x = area.x + (human && area.w == blueprint.w ? (area.w - widest - 0x4A) / 2 + 0x36 : (area.w - widest) / 2);
            l.y = area.y + y;
            l.level = level;
            l.item = item;
            detail.push_back(l);
            return l.h;
        };
        std::string name = info != pageInfo.end() ? res.String(info->second.name) : std::string();
        if (research.CurrentPage() == g) name = "(" + name + ")";
        const int wrap = human ? w - 0x4A : w - 8;
        int y = human ? 0x12 : 6;
        const int titleH = add(name, 0, y, colours.heading, -1, blueprint);
        y = titleH + (human ? 0x18 : 9);
        if (info != pageInfo.end() && info->second.speech)
            y += add(res.String(info->second.speech), wrap, y, colours.detail, -1, blueprint) + (human ? 0x12 : 6);
        const auto& items = research.Items();
        auto section = [&](int heading, auto pick, int level) {
            bool any = false;
            for (int i : page.items) if (pick(items[size_t(i)])) any = true;
            if (!any) return;
            y += add(res.String(heading), 0, y, colours.heading, -1, blueprint) + (human ? 6 : 3);
            for (int i : page.items)
                if (pick(items[size_t(i)])) y += add(res.String(items[size_t(i)].researchedText), wrap, y, level, -1, blueprint);
            y += human ? 0x12 : 6;
        };
        section(0x462, [](const Research::Item& it) { return it.state == Research::Done && (it.kind == Research::UnitItem || it.kind == Research::BuildingItem); }, colours.specification);
        section(0x463, [](const Research::Item& it) { return it.state == Research::Done && it.kind == Research::EnhancementItem; }, colours.enhancement);
        bool available = false;
        for (int i : page.items) if (items[size_t(i)].state == Research::Available) available = true;
        if (!available) return;
        proposals = { human ? 0x12A : 0x144, human ? 0x114 : 0x123, SpriteWidth(0x90), SpriteHeight(0x90) };
        int py = 0xA;
        add(res.String(0x464), 0, py, colours.heading, -1, proposals);
        py += human ? 0x10 : 0xD;
        for (int i : page.items)
        {
            const Research::Item& it = items[size_t(i)];
            if (it.state != Research::Available) continue;
            const int level = research.CurrentNumber() == i ? colours.current : (it.fresh == 0 || it.fresh == 1) ? colours.fresh : colours.proposal;
            py += add(research.NameWithTime(it, human), 0, py, level, i, proposals);
        }
    }

    void ResearchScreen::Update()
    {
        // 0xBC: a step of the folders a tick until they settle, laid out again each time
        if (animating)
        {
            animating = research.AnimatePlans(openRate);
            Layout();
        }
        if (speechDelay > 0 && --speechDelay == 0) { speechStart = PageSpeech(); speechDelay = -1; }
        // 0x45F3D0 while the page's blueprint is up
        if (research.ShownPage() >= 0 && ++glowTicks >= 2)
        {
            glowTicks = 0;
            if (focusedLine != glowingLine)
            {
                if (glowingLine >= 0 && size_t(glowingLine) < detail.size()) detail[size_t(glowingLine)].level = colours.menuItems;
                glowingLine = focusedLine;
            }
            glowLevel += glowStep;
            if (glowLevel > colours.glowLast) { glowLevel--; glowStep = -1; }
            if (glowLevel < colours.glowFirst) { glowLevel++; glowStep = 1; }
            if (focusedLine >= 0 && size_t(focusedLine) < detail.size()) detail[size_t(focusedLine)].level = glowLevel;
        }
    }

    int ResearchScreen::PageSpeech() const
    {
        const int g = research.ShownPage();
        if (g < 0) return 0;
        auto info = pageInfo.find(research.Pages()[size_t(g)].number);
        return info != pageInfo.end() ? res.Text().Extra(info->second.speech) : 0;
    }

    void ResearchScreen::PageGoes()
    {
        speechFade = true;
        speechDelay = -1;
        focusedLine = glowingLine = -1;
    }

    // 0x8114 (asked by a proposal's tool tip, 0x45F0E0): "Makes Available:" (0x46E) and, a line each, the items
    // still locked that have this one among their prerequisites and every other one done (0x45BA30), by their
    // research names (0x45B690); no tip when there are none
    std::string ResearchScreen::MakesAvailable(int number) const
    {
        const auto& items = research.Items();
        std::string text = res.String(0x46E);
        bool any = false;
        for (const Research::Item& it : items)
        {
            if (it.kind == Research::TriggerItem || it.state != Research::Locked) continue;
            bool among = false, ready = true;
            for (int p : it.pre)
            {
                if (p < 0 || size_t(p) >= items.size()) continue;
                if (p == number) { among = true; continue; }
                if (items[size_t(p)].state != Research::Done && items[size_t(p)].state != Research::Obsolete) ready = false;
            }
            if (!among || !ready) continue;
            text += "\n" + res.String(it.researchName);
            any = true;
        }
        return any ? text : std::string();
    }

    void ResearchScreen::Draw(Canvas& canvas) const
    {
        const bool human = res.Human();
        const auto& plans = research.Plans();
        const auto& pages = research.Pages();
        auto hovered = [&](int command, int param) {
            return hover >= 0 && hotspots[size_t(hover)].command == command && hotspots[size_t(hover)].param == param;
        };
        // 0x45CDD0: a page's tab and, once unrolled (or always in Human games), its name
        auto drawPage = [&](int g) {
            const Research::Page& page = pages[size_t(g)];
            const auto [x, y] = pagePos[size_t(g)];
            const int sprite = (page.plan == research.OpenPlan() && research.LastVisiblePage(page.plan) == g) ? 0x8B : 0x8D;
            canvas.DrawSprite(sprite, hovered(PageCommand, g) ? 1 : 0, x, y);
            if (page.height != page.full && !human) return;
            auto info = pageInfo.find(page.number);
            std::string name = info != pageInfo.end() ? res.String(info->second.name) : std::string();
            const bool current = research.CurrentPage() == g;
            if (current) name = "(" + name + ")";
            const int level = current ? colours.current : research.PageHasNew(g) ? colours.fresh : colours.normal;
            canvas.DrawString(TextFont, name, { x + (human ? 6 : 0x1C), y + (human ? 6 : 8), 0, 0 }, 0, level);
        };
        // 0x45C1A0: each shown plan's folder, its name, then its pages
        for (int p = 0; p < int(plans.size()); p++)
        {
            if (!research.PlanShown(p)) continue;
            const Research::Plan& plan = plans[size_t(p)];
            const auto [x, y] = planPos[size_t(p)];
            const int sprite = p == research.OpenPlan() ? 0x89 : 0x8A;
            int frame = hovered(PlanCommand, p) ? 1 : 0;
            if (!human)
            {
                const Sprite* s = res.GetSprite(sprite);
                const int frames = s ? int(s->Info().frames) : 1;
                frame = plan.full ? (frames - 1) * plan.open / plan.full : 0;
            }
            canvas.DrawSprite(sprite, frame, x, y);
            std::string name = res.String(plan.text);
            const bool current = research.CurrentPlan() == p;
            if (current) name = "<<" + name + ">>";
            bool fresh = false;
            for (int g : plan.pages) if (research.PageHasNew(g)) fresh = true;
            const int level = current ? colours.current : fresh ? colours.fresh : colours.normal;
            canvas.DrawString(TextFont, name, { x + (human ? 0x6F : 0x9B), y + (human ? 0xB : 0x10), 0, 0 }, TextCentreX | TextCentreY, level);
            if (human)
            {
                if (plan.open > 0)
                    for (int g : plan.pages) if (pages[size_t(g)].height > 0) drawPage(g);
            }
            else if (plan.open == plan.full)
            {
                bool rolling = false;
                for (int g : plan.pages)
                    if (research.PageVisible(g) && pages[size_t(g)].height != pages[size_t(g)].full) rolling = true;
                if (!rolling)
                    for (int g : plan.pages)
                        if (research.PageVisible(g) && pages[size_t(g)].height > 0) drawPage(g);
            }
        }
        // the Human overlays over the tree (0x86 HU_RSCHT, 0x87 RES_LAMP, 0x88 RES_WIRE). The game draws them after
        // its children (0x45E930); the page and proposals over them is how it looks in the game (the owner, update
        // 265: the lamp's cord had been drawn over the papers' text)
        if (human)
            for (int overlay : { 0x86, 0x87, 0x88 }) canvas.DrawSprite(overlay, 0, 0, 0);
        // the page's detail
        const int g = research.ShownPage();
        if (g >= 0 && size_t(g) < pages.size())
        {
            auto info = pageInfo.find(pages[size_t(g)].number);
            // 0x45F690: res_det, the picture, then its children (the texts); the proposals list's sprite. The
            // Martians' picture and proposals are see-through, as the game shows them (the owner, update 265)
            const Canvas::Blend look = human ? Canvas::Blend::Opaque : Canvas::Blend::Translucent;
            if (human) canvas.DrawSprite(0x8C, 0, blueprint.x, blueprint.y);
            if (info != pageInfo.end() && info->second.picture) canvas.DrawSprite(info->second.picture, 0, blueprint.x, blueprint.y, 0, look);
            if (proposals.w) canvas.DrawSprite(0x90, 0, proposals.x, proposals.y, 0, look);
            for (const Line& l : detail) canvas.DrawString(TextFont, l.text, { l.x, l.y, 0, 0 }, 0, l.level);
        }
        canvas.DrawSprite(ExitSprite, exitHeld ? 1 : 0, exitRect.x, exitRect.y);
    }

    // The keyboard (0x45ECB0, on the focused folder or page): Up and Down move the focus to the one before or
    // after (0x8008 / 0x8007 to their parent; round the ends, *provisional*), Enter and Space click it (0x201); Esc
    // closes the screen (0x45E9E0)
    std::optional<Command> ResearchScreen::OnKey(int sdlKey)
    {
        if (sdlKey == SDLK_ESCAPE) return Command{ CloseCommand, 0 };
        if (hotspots.empty()) return std::nullopt;
        const int n = int(hotspots.size());
        if (sdlKey == SDLK_UP || sdlKey == SDLK_DOWN)
        {
            const int step = sdlKey == SDLK_DOWN ? 1 : -1;
            hover = hover < 0 ? (step > 0 ? 0 : n - 1) : (hover + step + n) % n;
            focusedLine = -1;
        }
        else if ((sdlKey == SDLK_RETURN || sdlKey == SDLK_KP_ENTER || sdlKey == SDLK_SPACE) && hover >= 0)
            Activate(size_t(hover));
        return std::nullopt;
    }

    // The mouse takes the focus to what it's over (0x20 -> 0x42EBE0); a folder or page shows frame 1 while it has it
    void ResearchScreen::OnMouseMove(int x, int y)
    {
        hover = -1;
        for (size_t i = 0; i < hotspots.size(); i++)
            if (hotspots[i].rect.Contains(x, y)) hover = int(i);
        if (research.ShownPage() >= 0)
            for (size_t i = 0; i < detail.size(); i++)
            {
                const Line& l = detail[i];
                if (l.item >= 0 && Rect{ l.x, l.y, l.w, l.h }.Contains(x, y)) { focusedLine = int(i); hover = -1; }
            }
        if (hover >= 0) focusedLine = -1;
    }

    std::optional<Command> ResearchScreen::OnMouseButton(int x, int y, bool down)
    {
        if (exitRect.Contains(x, y))
        {
            if (down) { exitHeld = true; return std::nullopt; }
            if (exitHeld) { exitHeld = false; return Command{ CloseCommand, 0 }; }
        }
        exitHeld = false;
        if (!down) return std::nullopt;
        // the proposals list and the blueprint window are over the tabs: a proposal clicked is researched (0x84,
        // sound 0x132); the blueprint clicked plays the speech again (0x808F, 0x45F710)
        if (research.ShownPage() >= 0)
        {
            for (const Line& l : detail)
                if (l.item >= 0 && Rect{ l.x, l.y, l.w, l.h }.Contains(x, y))
                {
                    if (research.Choose(l.item)) sounds.push_back(0x132);
                    BuildDetail();
                    return std::nullopt;
                }
            if (proposals.Contains(x, y)) return std::nullopt;
            if (blueprint.Contains(x, y))
            {
                speechStart = PageSpeech();
                speechDelay = -1;
                return std::nullopt;
            }
        }
        // the hotspots (0x45ECB0) send their command on the press
        for (size_t i = hotspots.size(); i-- > 0;)
            if (hotspots[i].rect.Contains(x, y)) { Activate(i); return std::nullopt; }
        return std::nullopt;
    }

    void ResearchScreen::Activate(size_t i)
    {
        const Hotspot h = hotspots[i];
        if (h.command == PlanCommand)
        {
            // 0xBE: the folder opens (0x199) or closes (0x19A), the page goes (0x458D10, 0x19B), and the folders move
            sounds.push_back(research.ClickPlan(h.param) ? 0x199 : 0x19A);
            sounds.push_back(0x19B);
            if (research.ShownPage() >= 0) PageGoes();
            research.ShowPage(-1);
            detail.clear();
            animating = true;
        }
        else if (h.param != research.ShownPage())
        {
            // 0xBF: a new page (0x19B), the old one's speech faded (0x45E4A0), its detail, its speech 30 ticks on
            sounds.push_back(0x19B);
            if (research.ShownPage() >= 0) PageGoes();
            research.ShowPage(h.param);
            BuildDetail();
            speechDelay = 30;
        }
    }

    int ResearchScreen::TipOwner(int x, int y) const
    {
        if (research.ShownPage() >= 0)
            for (size_t i = 0; i < detail.size(); i++)
            {
                const Line& l = detail[i];
                if (l.item >= 0 && Rect{ l.x, l.y, l.w, l.h }.Contains(x, y)) return 10000 + int(i);
            }
        for (size_t i = hotspots.size(); i-- > 0;) if (hotspots[i].rect.Contains(x, y)) return int(i);
        return -1;
    }

    // 0x45C6E0 / 0x45D1A0 (0x80A3): a folder's or a page's name; 0x8114: an item's "Makes Available:" list
    std::string ResearchScreen::TipText(int owner, int level) const
    {
        if (level == 1 && owner >= 10000 && size_t(owner - 10000) < detail.size()) return MakesAvailable(detail[size_t(owner - 10000)].item);
        if (level != 1 || owner < 0 || size_t(owner) >= hotspots.size()) return {};
        const Hotspot& h = hotspots[size_t(owner)];
        if (h.command == PlanCommand) return res.String(research.Plans()[size_t(h.param)].text);
        auto info = pageInfo.find(research.Pages()[size_t(h.param)].number);
        return info != pageInfo.end() ? res.String(info->second.name) : std::string();
    }
}
