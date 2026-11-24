// Building on the battle map (docs/BATTLEMAP.md "Building on the battle map"): what the battle map asks of the war
// map's factories (WBuildList 0x45F770-0x462EE0, kept by the war map's objects, which stay alive behind the battle).
// The war map (warmap/WarMap) answers it; the battle map holds it through its Setup.
#pragma once
#include "core/Deployment.h"

#include <optional>
#include <string>

namespace wow
{
    class Canvas;

    class BuildHost
    {
    public:
        virtual ~BuildHost() = default;

        // the war map object's factory (its +0x54): whether it has one, how many entries it holds (+0x128) and
        // whether it's busy (+0x12C)
        struct Factory { bool exists = false; int queued = 0; bool busy = false; };
        virtual Factory FactoryOf(int ref) const = 0;
        // 0x813F (0x47DF74): Suspend / Resume (0x45FEF0, 0x45FE60)
        virtual void ToggleSuspend(int ref) = 0;

        // The Build List window opened in battle mode (0x47DFA3: the factory's +0x20 cleared, then its vt+0x14).
        // height: the panels' space's (the Build List is centred in it as WoWRevived's launcher placed it)
        virtual void OpenBattleWindow(int ref, int height = 480) = 0;
        virtual void CloseWindow() = 0;
        virtual bool WindowOpen() const = 0;
        virtual int WindowRef() const = 0;
        // in the panels' space
        virtual void DrawWindow(Canvas& canvas) const = 0;
        virtual int WindowHit(int x, int y) const = 0;      // -1 off it
        virtual void WindowHover(int x, int y) = 0;
        virtual std::string WindowTip(int hit) const = 0;
        // A press or a release on the window. In battle mode a construction's item (category 2 or 3) gives the
        // type to place (0x460D70 -> 0x42E790); a vehicle's is ordered at once (packet 0x17). A slot's cancel of a
        // construction under way gives its object, which the battle map kills (0x462120 in battle mode: 0x80A0).
        struct Click { bool taken = false; int placeType = 0; int killRef = -1; };
        virtual Click WindowClick(int x, int y, bool down) = 0;

        // The placement's order (packet 0x16, 0x4601C0 -> 0x460240 -> 0x461C80 / 0x4620B0 -> 0x43CD10): the
        // entry added to the open window's factory with its place, the construction made in the sector at once
        // at (x0, y0) (a fence's second end x1, y1); its battle group, or nothing if the factory refused it.
        virtual std::optional<BattleGroup> PlaceConstruction(int type, int x0, int y0, int x1, int y1) = 0;
        // a construction's state (+0x7C / +0x78) now, for the battle's object of it
        virtual float Progress(int ref) const = 0;
    };
}
