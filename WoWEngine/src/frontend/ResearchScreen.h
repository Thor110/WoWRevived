// The research screen (0x45E670, opened by WResearch::Open 0x458150 from the war map's 0x7D: R or the Research
// button): a full-screen view, backdrop GType 0x85 (HU_RSCH / MR_RSCH) in palette group 0x2EF (hr), and in Human
// games the overlays 0x86 HU_RSCHT, 0x87 RES_LAMP and 0x88 RES_WIRE drawn over the tree (0x45E930). The exit button
// (0x42FA20, GType 0x7C RES_EXIT / MR_EXIT, command 0xBD) is at (0x1AF, 0x1A4) Human, (0x1B7, 0x1BD) Martian; Esc
// does the same (0x45E9E0).
//
// The tree (docs/RESEARCH.md "The screen"): the plans as folders down the left, laid out from (32, 30) Human or
// (4, 10) Martian (0x459200 / 0x45C9A0), each with its visible pages as tabs below it while it's open. A folder
// clicked (0xBE) opens and the others close, a step of [Research] "Human Open Rate" (20) / "Martian Open Rate"
// (10) a tick (0xBC, 0x458D90); a tab clicked (0xBF) turns to that page and shows its detail (0x45D380); an
// available item there clicked (0x84) becomes the research (sound 0x132).
#pragma once
#include "core/ObjOjd.h"
#include "core/Research.h"
#include "frontend/Screen.h"

#include <map>
#include <utility>

namespace wow
{
    class ResearchScreen : public Screen
    {
    public:
        static constexpr int Id = 0x7D;
        static constexpr int CloseCommand = 0xBD;

        struct Colours
        {
            int current = 25;       // [Colours] "Research Current" (0x4B9400)
            int normal = 26;        // "Research Normal" (0x4B9404)
            int fresh = 27;         // "Research New" (0x4B9408)
            int heading = 17;       // "Research Heading" (0x4B940C)
            int proposal = 26;      // "Research Proposal" (0x4B9410)
            int detail = 26;        // "Research Detail" (0x4B9414)
            int specification = 26; // "Research Specification" (0x4B9418)
            int enhancement = 26;   // "Research Enhancement" (0x4B941C)
            int menuItems = 4, glowFirst = 4, glowLast = 9; // [Colours] "Menu Items", "Menu Glow First" / "Last"
        };

        ResearchScreen(Resources& res, Research& research, std::map<int, ObjOjd::ResearchPageInfo> pageInfo,
                       Colours colours, int openRate);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        void OnMouseMove(int x, int y) override;
        int CursorAt(int, int) const override { return 0; }
        int TipOwner(int x, int y) const override;
        std::string TipText(int owner, int level) const override;
        // the speech playing for the page (dword_589138): the front end starts it when asked (the page's speech, 30
        // ticks after the page shows, or at once on a click on the blueprint) and fades the last one out over 30
        // ticks when the page goes (0x45E4A0 -> 0x464EB0)
        int Speech() const { return speechSound; }
        void SetSpeech(int handle) { speechSound = handle; }
        int SpeechId() const { return speechId; }                 // (update 265) the speech's sound id, queued or playing
        void SetSpeechId(int id) { speechId = id; }
        int TakeSpeechStart() { return std::exchange(speechStart, 0); }
        bool TakeSpeechFade() { return std::exchange(speechFade, false); }

    private:
        struct Hotspot { Rect rect; int command; int param; };
        void Layout();                     // 0x459200
        void BuildDetail();                // 0x45D3F0 / 0x45DD50
        int SpriteWidth(int gtype) const;
        int SpriteHeight(int gtype) const;
        void Activate(size_t hotspot);     // a hotspot's command (0x45ECB0: a click, Enter or Space)
        void PageGoes();                   // 0x45E4A0: the speech faded, the delay dropped
        int PageSpeech() const;            // the shown page's speech sound (0 for none)
        std::string MakesAvailable(int item) const; // 0x8114

        Research& research;
        std::map<int, ObjOjd::ResearchPageInfo> pageInfo;
        Colours colours;
        int openRate;
        bool animating = false;            // 0xBC re-posting itself
        std::vector<std::pair<int, int>> planPos, pagePos; // where Layout put them
        std::vector<Hotspot> hotspots;     // the plans' (0xBE) and pages' (0xBF)
        int hover = -1;                    // the hotspot under the mouse (its focus, +0x44 / +0x40)
        Rect exitRect;
        bool exitHeld = false;
        // the page's detail: its lines and the proposals (available items) that can be clicked
        struct Line { std::string text; int x = 0, y = 0, w = 0, h = 0, level = 0, item = -1; };
        std::vector<Line> detail;          // on the screen, wrapped and placed
        Rect blueprint, proposals;         // the blueprint window and the proposals list (empty without one)
        int speechSound = 0;
        int speechId = 0;
        int speechStart = 0;               // a speech to start (an SFX id)
        bool speechFade = false;
        int speechDelay = -1;              // ticks until the page's speech starts
        // the blueprint window's timer (0x45F3D0, 0x8129 every 2 ticks): the focused proposal's level steps between
        // "Menu Glow First" and "Last" and back; one the focus leaves is left at "Menu Items" (0x8128)
        int focusedLine = -1, glowingLine = -1, glowLevel = 4, glowStep = 1, glowTicks = 0;
    };
}
