#include "core/ObjOjd.h"
#include "core/Bytes.h"

#include <cstring>

namespace wow
{
    namespace
    {
        // Payload sizes of the fixed-size record types, worked out from the loader's switch (0x43785F).
        // -1 marks the variable-size types handled in SkipPayload.
        constexpr int FixedPayload[65] = {
            /* 0*/ 2, 2, -1, -1, -1, 4, 12, 2, -1, -1,
            /*10*/ 34, 6, 16, 10, 16, 2, -1, 2, 2, 4,
            /*20*/ 2, 2, 2, 2, 2, 4, 2, 8, 12, 22,
            /*30*/ 6, 2, 2, 2, 2, 4, 2, 6, 8, 2,
            /*40*/ 2, 2, 2, 4, 4, 12, 4, 12, 4, 2,
            /*50*/ 4, 6, 2, 2, 2, 2, 1, 2, 2, 2,
            /*60*/ 2, 2, 1, 8, 8,
        };

        void SkipPayload(ByteReader& r, int type)
        {
            if (type < 0 || type > 64) return; // the loader ignores unknown types, there is no payload to skip
            switch (type)
            {
            case 8:  // int16 index, uint16 len, name[len], 4 unused int16s, 6 int16s
            {
                r.Skip(2);
                uint16_t len = r.U16();
                r.Skip(len + 8 + 12);
                return;
            }
            case 9:  // int16 index, int16 value, then three times: uint16 len, name[len], 4 floats
                r.Skip(4);
                for (int i = 0; i < 3; i++) { uint16_t len = r.U16(); r.Skip(len + 16); }
                return;
            case 16: // uint16 len, data[len]
            {
                uint16_t len = r.U16();
                r.Skip(len);
                return;
            }
            default:
                r.Skip(size_t(FixedPayload[type]));
            }
        }
    }

    std::map<int, GType> ObjOjd::LoadGTypes(std::span<const uint8_t> data, bool human)
    {
        std::map<int, GType> table;
        ByteReader r(data);
        int id = 0;
        uint16_t gclass = 0;
        bool loadOnUse = true;
        int16_t hotX = 0, hotY = 0;

        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            switch (type)
            {
            case 0: id = int16_t(r.U16()); break;
            case 1: gclass = r.U16(); break;
            case 2: case 3: case 4:
            {
                uint16_t len = r.U16();
                std::string name = r.Name(len);
                uint16_t slot = r.U16();
                bool wanted = type == 2 || (type == 3 && human) || (type == 4 && !human);
                if (wanted) table[id] = GType{ gclass, slot, std::move(name), loadOnUse, hotX, hotY };
                loadOnUse = true;
                hotX = hotY = 0;
                break;
            }
            case 5: hotX = int16_t(r.U16()); hotY = int16_t(r.U16()); break;
            case 7: loadOnUse = r.U16() == 0; break; // the loader stores (value == 0) in its "load later" flag
            default: SkipPayload(r, type);
            }
        }
        return table;
    }

    std::map<int, int> ObjOjd::LoadUnitCounters(std::span<const uint8_t> data)
    {
        std::map<int, int> counters;
        ByteReader r(data);
        int unit = 0;
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            switch (type)
            {
            case 15: unit = int16_t(r.U16()); break;            // starts unit type (the table at 0x51A0F0, 0xB0 bytes each)
            case 23: counters[unit] = int16_t(r.U16()); break; // its war map counter frame (+0x2C, 0x437EF4)
            case 18: { int from = int16_t(r.U16()); if (counters.count(from)) counters[unit] = counters[from]; break; } // a template's
            case 2: case 3: case 4: { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); break; } // file registrations
            default: SkipPayload(r, type);
            }
        }
        return counters;
    }

    std::map<int, ObjOjd::UnitData> ObjOjd::LoadUnits(std::span<const uint8_t> data)
    {
        std::map<int, UnitData> units;
        ByteReader r(data);
        int unit = 0, category = 0;
        auto f32 = [&]() { uint32_t b = r.U32(); float f; std::memcpy(&f, &b, 4); return f; };
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            switch (type)
            {
            case 17: category = int16_t(r.U16()); break; // 0x437E9E: the class the next unit types get
            case 15: // 0x437D62: a unit type seen the first time takes the current class (+0x10)
            {
                unit = int16_t(r.U16());
                bool fresh = units.find(unit) == units.end();
                if (fresh) units[unit].category = category;
                if (!units[unit].baseType) units[unit].baseType = unit; // +0x00, when not set yet
                break;
            }
            case 18: // 0x437DEE: the whole entry copied from a template type, then its parts, emitters and
            {        // builders (+0x38, +0x3C / +0x42, +0x78-0x80) cleared
                int from = int16_t(r.U16());
                auto src = units.find(from);
                UnitData copy = src != units.end() ? src->second : UnitData{};
                for (int& b : copy.builders) b = 0;
                units[unit] = copy;
                break;
            }
            case 19: units[unit].upgrade = int16_t(r.U16()); r.Skip(2); break; // 0x437E4C: +0x88, +0x8C
            case 20: units[unit].upgrade = int16_t(r.U16()); break;            // 0x437E7A: +0x88, +0x8C = 0
            case 52: units[unit].maxPerSector = int(r.U16()); break;      // 0x438738
            case 54: units[unit].prerequisite = int16_t(r.U16()); break;  // 0x438776
            case 55: units[unit].battleUnits = int16_t(r.U16()); break; // 0x438794
            case 40: units[unit].researchName = int16_t(r.U16()); break;    // 0x438473
            case 41: units[unit].researchingText = int16_t(r.U16()); break; // 0x438494
            case 42: units[unit].researchedText = int16_t(r.U16()); break;  // 0x4384B5
            case 43: // 0x438529, scaled by the faction the type has so far
            {
                uint32_t b = r.U32();
                std::memcpy(&units[unit].buildDays, &b, 4);
                units[unit].buildFaction = units[unit].faction;
                break;
            }
            case 49: case 50: case 51: // 0x4386AE: one, two or three builders
                for (int i = 0; i < type - 48; i++) units[unit].builders[i] = int16_t(r.U16());
                break;
            case 26: units[unit].faction = int16_t(r.U16()); break;     // 0x437F51
            case 39: // 0x4383F2, scaled by the faction the type has so far
                units[unit].researchDays = int16_t(r.U16());
                units[unit].researchFaction = units[unit].faction;
                break;
            case 23: units[unit].counterFrame = int16_t(r.U16()); break;
            case 21: units[unit].nameText = int16_t(r.U16()); break;    // 0x437EB2
            case 36: units[unit].moveClass = int16_t(r.U16()); break; // 0x438332
            case 37: // 0x438351
                units[unit].field52 = int16_t(r.U16());
                units[unit].warMapSpeed = int16_t(r.U16());
                units[unit].field56 = int16_t(r.U16());
                break;
            case 38: // 0x438392: scales the speed and +0x56
            {
                float a = f32(), b = f32();
                units[unit].warMapSpeed = int(float(units[unit].warMapSpeed) * a);
                units[unit].field56 = int(float(units[unit].field56) * b);
                break;
            }
            case 45: for (float& v : units[unit].upkeep) v = f32(); break;      // 0x4385BA
            case 46: { float m = f32(); for (float& v : units[unit].upkeep) v *= m; break; }
            case 47: for (float& v : units[unit].activity) v = f32(); break;    // 0x438634
            case 48: { float m = f32(); for (float& v : units[unit].activity) v *= m; break; }
            case 2: case 3: case 4: { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); break; }
            default: SkipPayload(r, type);
            }
        }
        return units;
    }

    std::map<int, ObjOjd::ResearchPageInfo> ObjOjd::LoadResearchPages(std::span<const uint8_t> data)
    {
        std::map<int, ResearchPageInfo> pages;
        ByteReader r(data);
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            if (type == 63 || type == 64) // 0x4384D6
            {
                int page = int16_t(r.U16());
                ResearchPageInfo& p = pages[page];
                p.name = int16_t(r.U16());
                p.picture = int16_t(r.U16());
                p.speech = int16_t(r.U16());
            }
            else if (type >= 2 && type <= 4) { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); }
            else SkipPayload(r, type);
        }
        return pages;
    }

    int ObjOjd::ResearchHours(const UnitData& unit, bool playingHuman, const ResearchModifiers& m)
    {
        float factor = 1.0f; // flt_4AA518
        if (unit.researchFaction == 0) factor = playingHuman ? m.humanFriend : m.humanEnemy;
        else if (unit.researchFaction == 1) factor = playingHuman ? m.martianEnemy : m.martianFriend;
        return int16_t(int(float(unit.researchDays * 24) * factor));
    }

    float ObjOjd::BuildTable(const UnitData& unit, bool playingHuman, const ManufactureModifiers& m)
    {
        float factor = 1.0f;
        if (unit.buildFaction == 0) factor = playingHuman ? m.humanFriend : m.humanEnemy;
        else if (unit.buildFaction == 1) factor = playingHuman ? m.martianEnemy : m.martianFriend;
        return unit.buildDays * factor;
    }

    int ObjOjd::BuildHours(const UnitData& unit, bool playingHuman, const ManufactureModifiers& m)
    {
        double hours = double(BuildTable(unit, playingHuman, m)) * 24.0; // dbl_4AADD0
        if (unit.category == 1) hours *= unit.battleUnits; // fimul on the x87 value, no rounding between
        return int(hours);
    }

    std::map<int, ObjOjd::ArrowType> ObjOjd::LoadArrowTypes(std::span<const uint8_t> data)
    {
        std::map<int, ArrowType> types;
        ByteReader r(data);
        auto f32 = [&]() { uint32_t b = r.U32(); float f; std::memcpy(&f, &b, 4); return f; };
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            if (type == 14)
            {
                ArrowType& t = types[int16_t(r.U16())];
                t.gtype = int16_t(r.U16());
                t.a = int16_t(r.U16());
                t.b = int16_t(r.U16());
                t.c = int16_t(r.U16());
                t.d = int16_t(r.U16());
                t.f = f32();
            }
            else if (type == 2 || type == 3 || type == 4) { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); }
            else SkipPayload(r, type);
        }
        return types;
    }

    std::map<int, std::array<int, 6>> ObjOjd::LoadSectorDeposits(std::span<const uint8_t> data)
    {
        std::map<int, std::array<int, 6>> deposits;
        ByteReader r(data);
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            if (type == 8)
            {
                // 0x437750 case 8: the sector, its name (to the sector table's +0xAC), 4 int16s it skips, then the
                // deposits of the six resource kinds (+0x04)
                int sector = int16_t(r.U16());
                uint16_t len = r.U16();
                r.Skip(size_t(len) + 8);
                std::array<int, 6>& d = deposits[sector];
                for (int& v : d) v = int16_t(r.U16());
            }
            else if (type == 2 || type == 3 || type == 4) { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); }
            else SkipPayload(r, type);
        }
        return deposits;
    }
}
