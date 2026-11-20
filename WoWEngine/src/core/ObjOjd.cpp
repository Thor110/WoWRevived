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
        int unit = 0;
        auto f32 = [&]() { uint32_t b = r.U32(); float f; std::memcpy(&f, &b, 4); return f; };
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5) throw FormatError("OBJ.ojd: record header cut short");
            r.Skip(3);
            int type = int16_t(r.U16());
            switch (type)
            {
            case 15: unit = int16_t(r.U16()); units[unit]; break;
            case 23: units[unit].counterFrame = int16_t(r.U16()); break;
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
            case 2: case 3: case 4: { uint16_t len = r.U16(); r.Skip(size_t(len) + 2); break; }
            default: SkipPayload(r, type);
            }
        }
        return units;
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
