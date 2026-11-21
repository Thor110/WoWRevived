#include "core/Research.h"
#include "core/Random.h"
#include "core/ResearchTrees.h"

#include <cstring>

namespace wow
{
    bool Research::notifications = false;

    namespace
    {
        // The triggers' rules (0x45A1C0 / 0x45B320): trigger n is done once any of these types has been met
        struct TriggerRule { int trigger; std::vector<int> types; };
        const std::vector<TriggerRule>& TriggerRules(bool human)
        {
            static const std::vector<TriggerRule> Human = {
                { 0, { 0x50, 0x51, 0x52 } },                        // Scanning Machines
                { 3, { 0x4D, 0x4E, 0x4F } },                        // Flying Machines
                { 4, { 0x53, 0x54, 0x55 } },                        // Constrictors
                { 7, { 0x4A, 0x4B, 0x4C, 0x50, 0x51, 0x52 } },      // Electric or Scanning Machines
                { 1, { 0x197, 0x198, 0x199 } },
                { 2, { 0x2C2, 0x2C3, 0x2C4 } },
                { 5, { 0x2BF, 0x2C0, 0x2C1 } },
                // trigger 6 ("Reading Martian symbols") isn't set here
            };
            static const std::vector<TriggerRule> Martian = {
                { 1, { 7, 8, 9 } },                                 // Tunnelling Track Layers
                { 2, { 0x13, 0x14, 0x15, 0x16, 0x17, 0x18 } },      // Ironclads and Submersibles
                { 4, { 0x273, 0x274, 0x275, 0x276 } },              // mines
                { 5, { 0x273, 0x274, 0x275, 0x276 } },
                // triggers 0 and 3 aren't set here
            };
            return human ? Human : Martian;
        }
    }

    const std::vector<Research::Improvement>& Research::Improvements(bool human)
    {
        // [Tweak] settings (0x4592C0-0x459500, 0x45A4C0-0x45A640), in the order UpdateTriggers applies them
        static const std::vector<Improvement> Human = {
            { 0x256, "HeatResistantArmourMultiplier", 0.85f },
            { 0x257, "InsulatedHighEnergyResistantArmourMultiplier", 0.75f },
            { 0x259, "HighExplosiveRoundsMultiplier", 1.15f },
            { 0x25A, "ImprovedPowderChargeMultiplier", 1.15f },
            { 0x25B, "AdvancedPowderChargeMultiplier", 1.30f },
            { 0x25C, "FineAirFiltersMultiplier", 0.85f },
            { 0x25D, "ChemicalFiltersMultiplier", 0.75f },
            { 0x25E, "EnhancedSightingApparatusMultiplier", 1.20f },
            { 0x266, "TunnelReinforcementApparatusMultiplier", 0.75f },
            { 0x265, "EnhancedDrillMultiplier", 1.25f },
        };
        static const std::vector<Improvement> Martian = {
            { 0x27B, "ShockResistantArmourMultiplier", 0.85f },
            { 0x27C, "ImpactReflectingArmourMultiplier", 0.75f },
            { 0x27D, "EnhancedRayFocusMechanismMultiplier", 1.15f },
            { 0x27E, "TightBeamMultiplier", 1.30f },
            { 0x289, "HoverBombsMultiplier", 1.25f },
            { 0x27F, "SecondaryBeamAmplifiersMultiplier", 1.15f },
            { 0x280, "MotionTrackingDeviceMultiplier", 1.20f },
        };
        return human ? Human : Martian;
    }

    Research::Research(bool human, bool local, const std::map<int, ObjOjd::UnitData>& units, const TextTable& text,
                       const ObjOjd::ResearchModifiers& modifiers, bool playingHuman, Listener listener)
        : human(human), local(local), text(&text), listener(std::move(listener))
    {
        using research::Op;
        notifications = false;
        auto add = [&](int kind, int type, const int* pre) {
            Item item;
            item.number = int(items.size());
            item.kind = kind;
            item.type = type;
            if (kind == TriggerItem)
            {
                item.total = 1;
                item.name = "Trigger"; // off_4CA494
            }
            else
            {
                auto unit = units.find(type);
                if (unit != units.end())
                {
                    item.total = ObjOjd::ResearchHours(unit->second, playingHuman, modifiers); // word_51A0F8
                    item.name = text.Get(unit->second.nameText);
                    item.researchName = unit->second.researchName;
                    item.researchingText = unit->second.researchingText;
                    item.researchedText = unit->second.researchedText;
                }
                item.page = pages.empty() ? -1 : int(pages.size()) - 1;
                if (!pages.empty()) pages.back().items.push_back(item.number);
                byType[type] = item.number;
            }
            for (int i = 0; i < 4; i++) item.pre[size_t(i)] = pre ? pre[i] : -1;
            items.push_back(item);
            if (kind != TriggerItem) TryMakeAvailable(items.back()); // AddUnit and the rest check it at once
        };
        const auto steps = human ? std::span<const research::TreeStep>(research::HumanTree) : std::span<const research::TreeStep>(research::MartianTree);
        for (const research::TreeStep& s : steps)
        {
            switch (s.op)
            {
            case Op::Plan: plans.push_back({ s.value, {}, 0, human ? 0x16 : 0x26 }); break;
            case Op::Page:
                pages.push_back({ s.value, int(plans.size()) - 1, 1, {}, 0, human ? 0x18 : 0x14 });
                plans.back().pages.push_back(int(pages.size()) - 1);
                break;
            case Op::Trigger: add(TriggerItem, 0, nullptr); break;
            case Op::Unit: add(UnitItem, s.value, s.pre); break;
            case Op::Building: add(BuildingItem, s.value, s.pre); break;
            case Op::Enhancement: add(EnhancementItem, s.value, s.pre); break;
            }
        }
        UnlockPass();
        notifications = true;
    }

    bool Research::PageVisible(int page) const
    {
        for (int i : pages[size_t(page)].items)
            if (items[size_t(i)].state == Available || items[size_t(i)].state == Done) return true;
        return false;
    }

    bool Research::PageHasNew(int page) const
    {
        for (int i : pages[size_t(page)].items)
            if (items[size_t(i)].fresh == 0 || items[size_t(i)].fresh == 1) return true;
        return false;
    }

    bool Research::PlanActive(int plan) const
    {
        for (int p : plans[size_t(plan)].pages) if (PageVisible(p)) return true;
        return false;
    }

    int Research::OpenPlan() const
    {
        if (human) return int(plans.size()) - 1;
        for (int p = int(plans.size()) - 1; p >= 0; p--) if (PlanActive(p)) return p;
        return -1;
    }

    int Research::LastVisiblePage(int plan) const
    {
        const auto& list = plans[size_t(plan)].pages;
        for (int i = int(list.size()) - 1; i >= 0; i--) if (PageVisible(list[size_t(i)])) return list[size_t(i)];
        return -1;
    }

    int Research::CurrentPage() const { return current >= 0 ? items[size_t(current)].page : -1; }
    int Research::CurrentPlan() const { int p = CurrentPage(); return p >= 0 ? pages[size_t(p)].plan : -1; }

    int Research::StepOpen(Plan& plan, int delta)
    {
        // 0x45C840: opening, the folder first, then what's left unrolls the visible pages from the last up (each to
        // its full height, 0x45D330); closing, the pages roll up from the first, then the folder. Returns what's left.
        if (plan.pages.empty()) return delta;
        auto page = [&](int p, int d) {
            Page& g = pages[size_t(p)];
            if (!d) return 0;
            g.height += d;
            if (d > 0 && g.height > g.full) { int left = g.height - g.full; g.height = g.full; return left; }
            if (d < 0 && g.height < 0) { int left = g.height; g.height = 0; return left; }
            return 0;
        };
        int left = 0;
        if (delta > 0)
        {
            plan.open += delta;
            if (plan.open > plan.full) { left = plan.open - plan.full; plan.open = plan.full; }
            for (int i = int(plan.pages.size()) - 1; i >= 0 && left; i--)
                if (PageVisible(plan.pages[size_t(i)])) left = page(plan.pages[size_t(i)], left);
            return left;
        }
        left = delta;
        for (size_t i = 0; i < plan.pages.size() && left; i++)
            if (PageVisible(plan.pages[i])) left = page(plan.pages[i], left);
        if (!left) return 0;
        plan.open += left;
        if (left > 0 && plan.open > plan.full) { int l = plan.open - plan.full; plan.open = plan.full; return l; }
        if (plan.open < 0) { int l = plan.open; plan.open = 0; return l; }
        return 0;
    }

    bool Research::AnimatePlans(int rate)
    {
        bool moving = false;
        for (int p = 0; p < int(plans.size()); p++)
        {
            if (!PlanShown(p)) continue;
            const int delta = p == selectedPlan ? rate : -rate;
            if (StepOpen(plans[size_t(p)], delta) != delta) moving = true;
        }
        return moving;
    }

    bool Research::ClickPlan(int plan)
    {
        if (plan != selectedPlan) { selectedPlan = plan; return true; }
        selectedPlan = -1;
        return false;
    }

    void Research::ShowPage(int page)
    {
        currentPage = page;
        if (page < 0 || size_t(page) >= pages.size()) return;
        for (int i : pages[size_t(page)].items) items[size_t(i)].fresh = 2; // 0x45BCB0
    }

    bool Research::Choose(int number)
    {
        if (number == selectedItem) return false;
        selectedItem = number;
        if (number >= 0 && size_t(number) < items.size()) SetCurrent(number);
        return true;
    }

    std::string Research::Duration(int hours, bool playingHuman, const TextTable& text)
    {
        const int h = uint16_t(hours);
        if (!h) return {};
        const int days = h / 24;
        if (playingHuman && days < 2)
            return std::to_string(h % 100) + text.Get(h > 1 ? 0x460 : 0x45F);
        return std::to_string(days % 10000) + text.Get(days > 1 ? 0x45E : 0x45D);
    }

    std::string Research::NameWithTime(const Item& item, bool playingHuman) const
    {
        if (item.kind == TriggerItem) return "Trigger";
        return text->Get(item.researchName) + " (" + Duration(item.remaining ? item.remaining : item.total, playingHuman, *text) + ")";
    }

    std::string Research::StatusText(const Item& item, bool playingHuman) const
    {
        if (item.kind == TriggerItem) return "Trigger";
        if (item.state == Done || item.state == Obsolete) return text->Get(item.researchedText);
        return text->Get(item.researchingText) + " <<" + Duration(item.remaining ? item.remaining : item.total, playingHuman, *text) + ">>";
    }

    const Research::Item* Research::ItemFor(int type) const
    {
        auto it = byType.find(type);
        return it == byType.end() ? nullptr : &items[size_t(it->second)];
    }

    bool Research::AnyAvailable() const
    {
        for (const Item& item : items) if (item.state == Available) return true;
        return false;
    }

    bool Research::TryMakeAvailable(Item& item)
    {
        if (item.state != Locked) return false;
        for (int p : item.pre)
            if (p >= 0 && items[size_t(p)].state != Done && items[size_t(p)].state != Obsolete) return false;
        item.state = Available;
        item.fresh = 0;
        if (notifications && local && (item.remaining || item.total))
        {
            Heard(0xB8);
            if (listener.event) listener.event(5, item.name); // ResearchAvailable
        }
        if (!item.total) Complete(item);
        return true;
    }

    void Research::Complete(Item& item)
    {
        item.state = Done;
        if (item.pre[0] >= 0)
        {
            Item& first = items[size_t(item.pre[0])];
            // "%s Makes %s Obsolete": the same kind and, for units, the same unit table +0x00 (the type itself)
            if (first.kind == item.kind && (item.kind != UnitItem || first.type == item.type)) first.state = Obsolete;
        }
        if (notifications && local)
        {
            Heard(0xB9);
            if (listener.event) listener.event(4, item.name); // ResearchCompleted
        }
        else if (!local && listener.otherSideCompleted) listener.otherSideCompleted(item.type);
    }

    void Research::CompleteTrigger(Item& item)
    {
        if (item.state != Locked) return;
        item.state = Done;
        Heard(0xB9);
    }

    void Research::Heard(int command)
    {
        // 0x458E50: 0xB8 marks +0x6C (what it's for isn't known); 0xB9, something done, resets the idle count and
        // has the current item dropped at the end of the hour
        if (command == 0xB8) flag6C = 1;
        if (command == 0xB9) { idle = 0; flag6D = 1; }
    }

    bool Research::UnlockPass()
    {
        bool changed = false;
        for (Item& item : items)
            if (item.kind != TriggerItem && TryMakeAvailable(item)) changed = true;
        return changed;
    }

    void Research::SetCurrent(int number)
    {
        if (number < 0 || size_t(number) >= items.size() || number == current) return;
        current = number;
        Item& item = items[size_t(number)];
        if (!item.remaining) item.remaining = item.total; // 0x45B850
        idle = 0;
    }

    namespace
    {
        // The plans (off_4CA600 Human: off_4CA4F8, off_4CA580; off_4CA740 Martian: off_4CA610, off_4CA6B0), each a list
        // of the side's research globals (0x588C40 Human, 0x588F20 Martian, four bytes an item), as item numbers
        const std::vector<std::vector<int>> HumanPlans = {
            { 65, 72, 28, 20, 40, 36, 69, 11, 55, 29, 61, 75, 68, 37, 78, 70, 62, 12, 30, 44, 71, 38, 41, 77, 42, 66, 82, 76, 79, 26, 73, 27, 74 },
            { 72, 15, 46, 69, 65, 17, 28, 58, 61, 75, 11, 73, 26, 55, 70, 77, 74, 16, 47, 26, 59, 82, 44, 68, 66, 78, 12, 27, 60, 79, 71 },
        };
        const std::vector<std::vector<int>> MartianPlans = {
            { 37, 32, 33, 16, 29, 69, 8, 34, 6, 24, 65, 72, 26, 75, 61, 30, 70, 58, 79, 60, 35, 73, 66, 27, 77, 7, 25, 80, 62, 31, 76, 68, 21, 36, 28, 78, 71, 74 },
            { 37, 9, 72, 75, 65, 8, 34, 69, 12, 51, 10, 39, 6, 58, 79, 73, 35, 21, 77, 60, 13, 52, 11, 40, 74, 78, 66, 7, 36, 14, 53, 68, 80, 22, 76 },
        };
        int humanPlan = -1, martianPlan = -1;   // dword_4CA1A8 / dword_4CA364: chosen once, for the whole run
    }

    int Research::ChooseFromPlan()
    {
        const auto& plans = human ? HumanPlans : MartianPlans;
        int& chosen = human ? humanPlan : martianPlan;
        if (chosen == -1 && !plans.empty()) chosen = random::Rand() % int(plans.size());
        if (chosen >= 0)
            for (int number : plans[size_t(chosen)])
                if (number >= 0 && size_t(number) < items.size() && items[size_t(number)].state == Available)
                {
                    SetCurrent(number);
                    return number;
                }
        return ChooseForAi();
    }

    int Research::ChooseForAi()
    {
        const int count = int(items.size());
        auto usable = [&](const Item& i) { return i.state == Available && i.kind != TriggerItem; };
        int pick = -1;
        for (int n = 0; n < count && pick < 0; n++)
        {
            const Item& i = items[size_t(n)];
            if (!usable(i)) continue;
            for (int p : i.pre) // 0x45B8B0
                if (p >= 0 && size_t(p) < items.size() && items[size_t(p)].kind == TriggerItem) { pick = n; break; }
        }
        if (pick < 0)
        {
            if (count == 0) return -1;
            int n = random::Rand() % count;
            for (int left = count; left > 0; left--)
            {
                if (usable(items[size_t(n)])) { pick = n; break; }
                n = (n + 1) % count;
            }
            if (pick < 0) return -1;
        }
        SetCurrent(pick);
        return pick;
    }

    void Research::Advance(int hours)
    {
        flag6C = flag6D = 0;
        if (current >= 0)
        {
            Item& item = items[size_t(current)]; // 0x45BB00
            if (item.state == Available && item.remaining)
            {
                item.remaining -= hours;
                if (item.remaining <= 0) { item.remaining = 0; Complete(item); }
            }
        }
        else idle++;
        while (UnlockPass()) {} // "WResearch::AdvanceResearch - Additional research pass"
        if (flag6D) current = -1;
        if (local && idle > 0x150)
        {
            if (AnyAvailable() && listener.event) listener.event(6, {}); // ResearchAlert
            idle = 0;
        }
    }

    void Research::UpdateTriggers(const std::set<int>& encountered)
    {
        for (const TriggerRule& rule : TriggerRules(human))
            for (int type : rule.types)
                if (encountered.count(type)) { CompleteTrigger(items[size_t(rule.trigger)]); break; }
        UnlockPass();
        for (const Improvement& imp : Improvements(human))
        {
            const Item* item = ItemFor(imp.type);
            if (item && item->state == Done) multipliers[imp.type] = imp.value;
        }
        if (!human)
        {
            const Item* metals = ItemFor(0x28C);
            const Item* subterrain = ItemFor(0x28D);
            if (metals && metals->state == Done) metalsDetection = true;
            if (subterrain && subterrain->state == Done) subterrainDetection = true;
        }
    }

    float Research::Multiplier(int type) const
    {
        auto it = multipliers.find(type);
        return it == multipliers.end() ? 1.0f : it->second;
    }

    void Research::CompleteAll()
    {
        const bool was = notifications;
        notifications = false;
        for (Item& item : items)
        {
            if (item.state == Locked) { item.state = Available; item.fresh = 0; } // 0x45BB30
            Complete(item);
        }
        notifications = was;
    }

    bool Research::Load(std::span<const uint8_t> data, size_t& pos)
    {
        auto u32 = [&](uint32_t& v) {
            if (pos + 4 > data.size()) return false;
            v = uint32_t(data[pos]) | uint32_t(data[pos + 1]) << 8 | uint32_t(data[pos + 2]) << 16 | uint32_t(data[pos + 3]) << 24;
            pos += 4;
            return true;
        };
        auto u8 = [&](uint8_t& v) { if (pos >= data.size()) return false; v = data[pos++]; return true; };
        uint32_t a = 0, b = 0, c = 0, cur = 0, count = 0;
        if (!u32(a) || !u32(b) || !u32(c) || !u8(flag6C) || !u8(flag6D) || !u32(cur) || !u32(count)) return false;
        selectedPlan = int32_t(a);
        currentPage = int32_t(b);
        selectedItem = int32_t(c);
        current = int32_t(cur) >= 0 && cur < items.size() ? int(cur) : -1;
        for (const Plan& plan : plans)
        {
            if (!u32(count)) return false;
            for (int p : plan.pages)
            {
                Page& page = pages[size_t(p)];
                if (!u8(page.fresh) || !u32(count)) return false;
                for (int i : page.items)
                {
                    Item& item = items[size_t(i)];
                    uint32_t state = 0, fresh = 0, remaining = 0;
                    if (!u32(state) || !u32(fresh) || !u32(remaining)) return false;
                    item.state = int(state);
                    item.fresh = int(fresh);
                    item.remaining = int(remaining);
                }
            }
        }
        notifications = false;
        for (Item& item : items) if (item.kind != TriggerItem) TryMakeAvailable(item);
        notifications = true;
        return true;
    }

    void Research::Save(std::vector<uint8_t>& out) const
    {
        auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out.push_back(uint8_t(v >> (8 * i))); };
        const char* tag = Tag(human);
        out.insert(out.end(), tag, tag + 4);
        u32(uint32_t(selectedPlan));
        u32(uint32_t(currentPage));
        u32(uint32_t(selectedItem));
        out.push_back(flag6C);
        out.push_back(flag6D);
        u32(uint32_t(current));
        u32(uint32_t(plans.size()));
        for (const Plan& plan : plans)
        {
            u32(uint32_t(plan.pages.size()));
            for (int p : plan.pages)
            {
                const Page& page = pages[size_t(p)];
                out.push_back(page.fresh);
                u32(uint32_t(page.items.size()));
                for (int i : page.items)
                {
                    const Item& item = items[size_t(i)];
                    u32(uint32_t(item.state));
                    u32(uint32_t(item.fresh));
                    u32(uint32_t(item.remaining));
                }
            }
        }
    }
}
