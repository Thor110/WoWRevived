#include "core/CivilWar.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cstring>

namespace wow
{
    namespace
    {
        bool InScotland(int sector) { return std::find(CivilWarScotland.begin(), CivilWarScotland.end(), sector) != CivilWarScotland.end(); }

        // a save list's side (0 for HUNI / HCON / HRES, 1 for AUNI / ACON / ARES) and the list of a kind for a side
        int ListSide(int list) { return list % 2; }
        int ListFor(int list, int side) { return list - list % 2 + side; }

        // the research blob split into its HRSH and MRSH blocks (each with its tag)
        void SplitResearch(const std::vector<uint8_t>& blob, std::vector<uint8_t>& human, std::vector<uint8_t>& martian)
        {
            if (blob.size() < 8 || std::memcmp(blob.data(), "HRSH", 4) != 0) throw FormatError("civil war: the start file's research doesn't begin with HRSH");
            size_t m = std::string::npos;
            for (size_t i = 4; i + 4 <= blob.size(); i++)
                if (std::memcmp(&blob[i], "MRSH", 4) == 0) { m = i; break; }
            if (m == std::string::npos) throw FormatError("civil war: the start file's research has no MRSH");
            human.assign(blob.begin(), blob.begin() + std::ptrdiff_t(m));
            martian.assign(blob.begin() + std::ptrdiff_t(m), blob.end());
        }

        // an object copied into another sector and side: its place moved by the two sectors' waypoints' difference
        // (unplaced objects, at 0, 0, stay there), new ids
        SaveObject Moved(const SaveObject& o, int sector, int side, int dx, int dy, std::vector<bool>& used)
        {
            SaveObject c = o;
            c.list = ListFor(o.list, side);
            c.SetSector(sector);
            if (o.X() != 0 || o.Y() != 0) c.SetPosition(o.X() + dx, o.Y() + dy);
            c.Renumber(used);
            // the command building as it is anywhere but its own home: London's Command Post (278) is a Command
            // Post (58) elsewhere, the Grampians' Communications Centre (830) one (120) elsewhere
            // (core/Deployment.h, HumanHq / MartianHq; WarMap::HasCommandPost)
            if (sector != 30) c.Retype(278, 58);
            if (sector != 3) c.Retype(830, 120);
            return c;
        }
    }

    SaveFile MakeCivilWarStart(const SaveFile& start, Campaign campaign)
    {
        // the start file is the player's faction's: its H lists (even) the player's faction, its A lists (odd) the
        // other faction, which a civil war doesn't have
        const bool human = campaign == Campaign::HumanCivilWar;
        const int playerCode = OwnerCode(human, 0), otherCode = OwnerCode(human, 1);
        SaveFile s = start;
        s.campaign = campaign;
        s.messages.clear();
        s.pods = 0;
        s.podHours = 0;
        std::vector<uint8_t> humanBlock, martianBlock;
        SplitResearch(start.research, humanBlock, martianBlock);
        const std::vector<uint8_t>& block = human ? humanBlock : martianBlock;
        s.research = block;                                     // the player's side's, then the other's
        s.research.insert(s.research.end(), block.begin(), block.end());
        // the player's home, and the home whose objects it is given a copy of
        const int home = human ? CivilWarNorthHome : CivilWarSouthHome;
        const int from = human ? CivilWarSouthHome : CivilWarNorthHome;

        for (int sector = 1; sector <= 30; sector++)
        {
            SaveSector& sec = s.Sector(sector);
            std::array<std::vector<SaveObject>, 6> lists;
            for (auto& list : sec.lists)
                for (SaveObject& o : list)
                {
                    if (ListSide(o.list) == 1) continue;                 // the other faction out
                    // the Humans: Scotland stays the player's, the rest is London's; the Martians: all of it the
                    // other side's (the player's comes from the copy below)
                    const bool players = human && InScotland(sector);
                    o.list = ListFor(o.list, players ? 0 : 1);
                    lists[size_t(o.list)].push_back(o);
                }
            sec.lists = std::move(lists);
            const int owner = sec.Owner();
            if (owner == playerCode) sec.SetOwner(human && InScotland(sector) ? playerCode : otherCode);
            else if (owner == otherCode) sec.SetOwner(2);
        }

        // the player's home: a copy of the other home's objects (the start file's own, the player's faction's),
        // moved by the difference between their middle and the home's place
        std::vector<bool> used = s.UsedIds();
        const SaveSector& source = start.Sector(from);
        int sx = 0, sy = 0, n = 0;
        for (const auto& list : source.lists)
            for (const SaveObject& o : list)
                if (ListSide(o.list) == 0 && (o.X() != 0 || o.Y() != 0)) { sx += o.X(); sy += o.Y(); n++; }
        if (n) { sx /= n; sy /= n; }
        // where the copy goes: the place of the Martians' base in the Grampian Mountains (Human.000, Martian.000), or
        // London's route waypoint (warmap/WarMapData.h, sector 30)
        const int tx = home == CivilWarNorthHome ? 468 : 1111, ty = home == CivilWarNorthHome ? 540 : 2034;
        for (const auto& list : source.lists)
            for (const SaveObject& o : list)
            {
                if (ListSide(o.list) != 0) continue;
                SaveObject c = Moved(o, home, 0, tx - sx, ty - sy, used);
                s.Sector(home).lists[size_t(c.list)].push_back(std::move(c));
            }
        s.Sector(home).SetOwner(playerCode);
        s.warMap[2] = uint32_t(home);       // the view starts on the player's home
        return s;
    }
}
