// WResearch (0x457FA0-0x45BCB0): one side's research, its plans, pages and items, as the war map keeps two of them
// (the local player's at dword_4E9B88, the other side's at dword_4E9B8C).
//
// The trees are fixed (core/ResearchTrees.h, built by 0x459540 / 0x45A680); OBJ.ojd gives each item its name
// (record 21) and time (record 39, see ObjOjd::ResearchHours). Items:
//   kind     0 unit, 1 building, 2 enhancement, 3 trigger (an item standing for something seen in battle)
//   state    0 locked, 1 available (the one being researched is also 1), 2 done, 3 obsolete (done, superseded)
//   fresh    +0x50: 2 when made, 0 once it becomes available, 2 again once looked at on the screen (0x45BCB0)
// An item becomes available (0x45B920) once every prerequisite is done or obsolete; then, with notifications on
// (byte_588BA8) and it belonging to the local player, the research hears 0xB8 and the war map ResearchAvailable
// (event 5) with its name, unless it takes no time, and one that takes no time is done at once. The current item
// counts down every game hour (0x45BB00) and is done at 0 (0x45BB90): its first prerequisite becomes obsolete if
// it's the same kind and, for units, the same type (+0x00 of the unit table, the type itself, so never in
// practice); then with notifications on the local side's research hears 0xB9 (no item current any more, the idle
// count reset) and the war map ResearchCompleted (event 4); the other side's tells its player (dword_4E9B78, 0xB9
// with the type) instead, with notifications on or off.
//
// Every game hour (0x458BD0, the war map's 0x802D handler for both sides): the current item goes on, or the idle
// count goes up; unlock passes run until nothing changes; a 0xB9 heard on the way clears the current item; and the
// local player's research, idle more than 336 hours with something available, raises ResearchAlert (event 6).
//
// Saved (HRSH / MRSH, Human first whatever the side, 0x4585D0): u32 selected plan, u32 page, u32 selected item (all
// -1 at the start), u8 +0x6C, u8 +0x6D, i32 the current item's number or -1, u32 the plan count, then each plan:
// u32 its page count, each page: u8 fresh (+0x60, 1 when made), u32 its item count, each item: u32 state, fresh and
// hours left. Triggers belong to no page, so they aren't saved: UpdateTriggers works them out again. Loading reads
// as many plans, pages and items as the tree has, whatever the counts say, then checks every item again with
// notifications off (0x4584C0).
#pragma once
#include "core/ObjOjd.h"
#include "core/TextOjd.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    class Research
    {
    public:
        enum Kind { UnitItem = 0, BuildingItem = 1, EnhancementItem = 2, TriggerItem = 3 };
        enum State { Locked = 0, Available = 1, Done = 2, Obsolete = 3 };

        struct Item
        {
            int number = 0;            // +0x20
            int kind = 0;              // +0x40
            int type = 0;              // +0x44: the unit type (0 for a trigger)
            int page = -1;             // +0x28 (-1 for a trigger)
            int total = 0;             // +0x2C, hours
            std::array<int, 4> pre{ -1, -1, -1, -1 }; // +0x30-0x3C
            int state = Locked;        // +0x4C
            int fresh = 2;             // +0x50
            int remaining = 0;         // +0x54, hours
            std::string name;          // +0x0C: "Trigger" or the type's name
            int researchName = 0, researchingText = 0, researchedText = 0; // the type's records 40-42
        };
        // A page (0x45CB70): its number in the category table, its plan, +0x60, its items; on the screen how far
        // it's unrolled (+0x44, up to +0x48: 0x18 Human, 0x14 Martian)
        struct Page { int number = 0; int plan = 0; uint8_t fresh = 1; std::vector<int> items; int height = 0, full = 0; };
        // A plan (0x45BFA0): its name, its pages; on the screen how far its folder is open (+0x3C, up to +0x40:
        // 0x16 Human, 0x26 Martian)
        struct Plan { int text = 0; std::vector<int> pages; int open = 0, full = 0; };

        // What the research tells the rest of the game: war map events 4 / 5 / 6 (0x41E5B0, with the item's name),
        // and for the other side a finished item's type to that side's player (0xB9 to dword_4E9B78)
        struct Listener
        {
            std::function<void(int event, const std::string& name)> event;
            std::function<void(int type)> otherSideCompleted;
        };

        // human: which tree; local: the local player's (+0x88)
        Research(bool human, bool local, const std::map<int, ObjOjd::UnitData>& units, const TextTable& text,
                 const ObjOjd::ResearchModifiers& modifiers, bool playingHuman, Listener listener);

        bool Human() const { return human; }
        bool Local() const { return local; }
        const std::vector<Item>& Items() const { return items; }
        const std::vector<Plan>& Plans() const { return plans; }
        const std::vector<Page>& Pages() const { return pages; }
        const Item* Current() const { return current >= 0 ? &items[size_t(current)] : nullptr; }
        int CurrentNumber() const { return current; }
        int Idle() const { return idle; }
        // the item for a unit type (the unit table's +0x04, dword_51A0F4), or nothing
        const Item* ItemFor(int type) const;
        bool AnyAvailable() const;

        // The research screen's side of it:
        bool PageVisible(int page) const;  // 0x45CCF0: any of its items available or done
        bool PageHasNew(int page) const;   // 0x45CD30: any item not yet looked at (fresh 0 or 1)
        bool PlanActive(int plan) const;   // 0x45C140: any page visible
        int OpenPlan() const;              // 0x458100: Human, the last plan; Martian, the last active one (-1)
        bool PlanShown(int plan) const { return PlanActive(plan) || plan == OpenPlan(); }
        int LastVisiblePage(int plan) const; // 0x45C110
        int CurrentPlan() const;           // the plan the current research is in, -1 for none
        int CurrentPage() const;           // the page the current research is on, -1 for none
        int SelectedPlan() const { return selectedPlan; }
        int ShownPage() const { return currentPage; }
        // 0x458D90: a step of the folders' opening and closing: the selected plan opens by rate, every other shown
        // one closes by it; true while anything still moves
        bool AnimatePlans(int rate);
        void SettlePlans(int rate) { while (AnimatePlans(rate)) {} }
        // 0xBE, a plan's folder clicked: it's selected, or unselected if it was (true if it opened)
        bool ClickPlan(int plan);
        // 0xBF / 0x458D10: the page shown (-1 for none); its items marked as looked at (0x45D380 -> 0x45CD70)
        void ShowPage(int page);
        // 0x84: the item chosen to research (false if it was already the selected one)
        bool Choose(int number);

        // 0x41BDA0: a time in hours as the game shows it: "<n>day(s)" (Martian "cycle(s)", 0x45D / 0x45E) from two
        // days up, "<n>hr(s)" (0x45F / 0x460) below that in Human games; Martian games always count days, so less
        // than one is "0cycle". Nothing for 0.
        static std::string Duration(int hours, bool playingHuman, const TextTable& text);
        // 0x45B6D0: "<research name> (<time left, or the whole time>)"; "Trigger" for a trigger
        std::string NameWithTime(const Item& item, bool playingHuman) const;
        // 0x45B750: done (or obsolete), the type's done text; otherwise "<researching text> <<<time>>>"
        std::string StatusText(const Item& item, bool playingHuman) const;

        // 0x458A00: research this item (an available one, as the screen offers them)
        void SetCurrent(int number);
        // 0x458BD0
        void Advance(int hours);
        // 0x45A1C0 / 0x45B320: the triggers from the types met in battle (the unit table's +0xAC, set by the
        // battle map), an unlock pass, then the improvement multipliers of the finished enhancements
        void UpdateTriggers(const std::set<int>& encountered);
        // 0x458460, the cheat: everything made available and done, with notifications off
        void CompleteAll();

        // 0x4584C0 after the tag; false if the data runs out
        bool Load(std::span<const uint8_t> data, size_t& pos);
        // the tag (HRSH / MRSH) and 0x4585D0
        void Save(std::vector<uint8_t>& out) const;
        static const char* Tag(bool human) { return human ? "HRSH" : "MRSH"; }

        // The improvement multipliers (0x4CA180-0x4CA1A4 Human, 0x4CA348-0x4CA360 Martian; 1.0 until the enhancement
        // is done, then the [Tweak] value) and the Martian detection upgrades (byte_589130 / 589134), for the
        // battle map
        struct Improvement { int type; const char* setting; float value; };
        static const std::vector<Improvement>& Improvements(bool human);
        float Multiplier(int type) const;
        bool MetalsDetection() const { return metalsDetection; }
        bool SubterrainDetection() const { return subterrainDetection; }

        // byte_588BA8: notifications (off while a tree is built, loaded or cheated through)
        static bool notifications;

    private:
        int StepOpen(Plan& plan, int delta); // 0x45C840
        bool TryMakeAvailable(Item& item); // 0x45B920
        void Complete(Item& item);         // 0x45BB90
        void CompleteTrigger(Item& item);  // 0x45BC70
        bool UnlockPass();                 // 0x458CC0
        void Heard(int command);           // 0x458E50: 0xB8 / 0xB9

        bool human, local;
        const TextTable* text;
        Listener listener;
        std::vector<Item> items;
        std::vector<Plan> plans;
        std::vector<Page> pages;
        std::map<int, int> byType;
        int selectedPlan = -1, currentPage = -1, selectedItem = -1; // +0x58, +0x5C, +0x60
        int idle = 0;                      // +0x64
        uint8_t flag6C = 0, flag6D = 0;    // +0x6C (0xB8), +0x6D (0xB9)
        int current = -1;                  // +0x8C
        std::map<int, float> multipliers;
        bool metalsDetection = false, subterrainDetection = false;
    };
}
