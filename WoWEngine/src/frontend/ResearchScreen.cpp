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
        constexpr int BlueprintX = 0x118, BlueprintY = 0x16; // the blueprint window (0x45F4B0); the detail's texts are its children
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

    // The page's detail (0x45D3F0; *provisional* past the title and the scientist's text: the three lists'
    // places aren't traced yet, nor the Martian layout 0x45DD50). The blueprint (0x45F4B0) at (0x118, 0x16): res_det
    // (0x8C, Human games) and the page's picture over it; the texts are its children, placed inside it. The title, "(name)" for the page being researched,
    // font 0x72 in "Research Heading", at y 0x12, centred in the space left of the picture ((w - title - 0x4A) / 2 +
    // 0x36, the picture's width w); the scientist's text (the page's speech text) wrapped to w - 0x4A in "Research
    // Detail", 0x18 below the title's height. Then "Specifications:" (0x462) with what's done there (record 42's
    // text, "Research Specification"), "Enhancements:" (0x463, "Research Enhancement") and "Proposals For research:"
    // (0x464) with what can be researched ("Research Proposal"), each clickable (0x84).
    void ResearchScreen::BuildDetail()
    {
        detail.clear();
        const int g = research.ShownPage();
        if (g < 0 || size_t(g) >= research.Pages().size()) return;
        const Research::Page& page = research.Pages()[size_t(g)];
        auto info = pageInfo.find(page.number);
        const int picture = info != pageInfo.end() ? info->second.picture : 0;
        const int w = picture ? SpriteWidth(picture) : 0x118;
        const Font* font = res.GetFont(TextFont);
        const int lineH = (font ? font->Height() : 10) + 2;
        auto width = [&](const std::string& t) { return font ? font->Measure(t) : 0; };
        std::string name = info != pageInfo.end() ? res.String(info->second.name) : std::string();
        if (research.CurrentPage() == g) name = "(" + name + ")";
        const bool human = res.Human();
        const int left = human ? 0x36 : 4;
        const int span = human ? 0x4A : 8;
        detail.push_back({ name, (w - width(name) - span) / 2 + left, 0x12, 0, colours.heading, TextFont, false, -1 });
        int y = lineH + 0x18;
        if (info != pageInfo.end() && info->second.speech)
        {
            detail.push_back({ res.String(info->second.speech), left, y, w - span, colours.detail, TextFont, true, -1 });
            y += lineH * 4 + 0x12; // *provisional*: the wrapped text's own height
        }
        const auto& items = research.Items();
        auto section = [&](int heading, auto pick, auto text, int level, bool clickable) {
            std::vector<int> list;
            for (int i : page.items) if (pick(items[size_t(i)])) list.push_back(i);
            if (list.empty()) return;
            detail.push_back({ res.String(heading), left, y, 0, colours.heading, TextFont, false, -1 });
            y += lineH;
            for (int i : list)
            {
                detail.push_back({ text(items[size_t(i)]), left, y, w - span, level, TextFont, false, clickable ? i : -1 });
                y += lineH;
            }
            y += 0x12;
        };
        const bool playingHuman = human;
        section(0x462, [](const Research::Item& it) { return (it.state == Research::Done || it.state == Research::Obsolete) && it.kind != Research::EnhancementItem; },
                [&](const Research::Item& it) { return res.String(it.researchedText); }, colours.specification, false);
        section(0x463, [](const Research::Item& it) { return (it.state == Research::Done || it.state == Research::Obsolete) && it.kind == Research::EnhancementItem; },
                [&](const Research::Item& it) { return res.String(it.researchedText); }, colours.enhancement, false);
        section(0x464, [](const Research::Item& it) { return it.state == Research::Available; },
                [&](const Research::Item& it) { return research.NameWithTime(it, playingHuman); }, colours.proposal, true);
    }

    void ResearchScreen::Update()
    {
        // 0xBC: a step of the folders a tick until they settle, laid out again each time
        if (animating)
        {
            animating = research.AnimatePlans(openRate);
            Layout();
        }
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
        // the page's detail
        const int g = research.ShownPage();
        if (g >= 0 && size_t(g) < pages.size())
        {
            auto info = pageInfo.find(pages[size_t(g)].number);
            // 0x45F690: res_det, the picture, then its children (the texts, placed inside it)
            if (human) canvas.DrawSprite(0x8C, 0, BlueprintX, BlueprintY);
            if (info != pageInfo.end() && info->second.picture) canvas.DrawSprite(info->second.picture, 0, BlueprintX, BlueprintY);
            for (const Line& l : detail)
            {
                std::string text = l.wrap && l.w > 0 ? canvas.Wrap(l.font, l.text, l.w) : l.text;
                canvas.DrawString(l.font, text, { BlueprintX + l.x, BlueprintY + l.y, 0, 0 }, 0, l.level);
            }
        }
        // the Human overlays over the tree, then the exit button
        if (human)
            for (int overlay : { 0x86, 0x87, 0x88 }) canvas.DrawSprite(overlay, 0, 0, 0);
        canvas.DrawSprite(ExitSprite, exitHeld ? 1 : 0, exitRect.x, exitRect.y);
    }

    std::optional<Command> ResearchScreen::OnKey(int sdlKey)
    {
        if (sdlKey == SDLK_ESCAPE) return Command{ CloseCommand, 0 }; // 0x45E9E0
        return std::nullopt;
    }

    void ResearchScreen::OnMouseMove(int x, int y)
    {
        hover = -1;
        for (size_t i = 0; i < hotspots.size(); i++)
            if (hotspots[i].rect.Contains(x, y)) hover = int(i);
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
        // the blueprint window is over the tabs: a click on it is its own (a proposal, 0x84; anywhere else 0x808F,
        // the speech again, 0x45F710)
        const int g = research.ShownPage();
        if (g >= 0 && size_t(g) < research.Pages().size())
        {
            auto info = pageInfo.find(research.Pages()[size_t(g)].number);
            const int picture = info != pageInfo.end() ? info->second.picture : 0;
            const int bw = std::max(SpriteWidth(0x8C), picture ? SpriteWidth(picture) : 0);
            const int bh = std::max(SpriteHeight(0x8C), picture ? SpriteHeight(picture) : 0);
            if (Rect{ BlueprintX, BlueprintY, res.Human() ? bw : (picture ? SpriteWidth(picture) : 0), res.Human() ? bh : (picture ? SpriteHeight(picture) : 0) }.Contains(x, y))
            {
                // a proposal clicked (0x84): it's researched (speech 0x132)
                const Font* font = res.GetFont(TextFont);
                const int lineH = (font ? font->Height() : 10) + 2;
                for (const Line& l : detail)
                    if (l.item >= 0 && Rect{ BlueprintX + l.x, BlueprintY + l.y, l.w, lineH }.Contains(x, y))
                    {
                        if (research.Choose(l.item)) sounds.push_back(0x132);
                        BuildDetail();
                        return std::nullopt;
                    }
                if (info != pageInfo.end())
                    if (int speech = res.Text().Extra(info->second.speech)) sounds.push_back(speech);
                return std::nullopt;
            }
        }
        // the hotspots (0x45ECB0) send their command on the press
        for (size_t i = hotspots.size(); i-- > 0;)
        {
            const Hotspot& h = hotspots[i];
            if (!h.rect.Contains(x, y)) continue;
            if (h.command == PlanCommand)
            {
                // 0xBE: the folder opens (0x199) or closes (0x19A), the page goes (0x458D10, 0x19B), and the folders move
                sounds.push_back(research.ClickPlan(h.param) ? 0x199 : 0x19A);
                sounds.push_back(0x19B);
                research.ShowPage(-1);
                detail.clear();
                animating = true;
            }
            else if (h.param != research.ShownPage())
            {
                // 0xBF: a new page (0x19B), its detail
                sounds.push_back(0x19B);
                research.ShowPage(h.param);
                BuildDetail();
                auto info = pageInfo.find(research.Pages()[size_t(h.param)].number);
                if (info != pageInfo.end())
                    if (int speech = res.Text().Extra(info->second.speech)) sounds.push_back(speech); // *provisional*: at once, not 30 ticks on
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    int ResearchScreen::TipOwner(int x, int y) const
    {
        for (size_t i = hotspots.size(); i-- > 0;) if (hotspots[i].rect.Contains(x, y)) return int(i);
        return -1;
    }

    // 0x45C6E0 / 0x45D1A0 (0x80A3): a folder's or a page's name; 0x8114: an item's "Makes Available:" list
    std::string ResearchScreen::TipText(int owner, int level) const
    {
        if (level != 1 || owner < 0 || size_t(owner) >= hotspots.size()) return {};
        const Hotspot& h = hotspots[size_t(owner)];
        if (h.command == PlanCommand) return res.String(research.Plans()[size_t(h.param)].text);
        auto info = pageInfo.find(research.Pages()[size_t(h.param)].number);
        return info != pageInfo.end() ? res.String(info->second.name) : std::string();
    }
}
