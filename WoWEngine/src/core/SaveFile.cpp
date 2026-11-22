#include "core/SaveFile.h"
#include "core/Ffuh.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace wow
{
    namespace
    {
        constexpr const char* ListTags[6] = { "HUNI", "AUNI", "HCON", "ACON", "HRES", "ARES" };

        uint32_t Get32(const std::vector<uint8_t>& b, size_t at)
        {
            if (at + 4 > b.size()) return 0;
            return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
        }
        void Put32(std::vector<uint8_t>& b, size_t at, uint32_t v)
        {
            if (at + 4 > b.size()) return;
            for (int i = 0; i < 4; i++) b[at + size_t(i)] = uint8_t(v >> (8 * i));
        }

        struct Reader
        {
            std::span<const uint8_t> d;
            size_t p = 0;
            void Need(size_t n) const { if (p + n > d.size()) throw FormatError("save: ends early"); }
            uint32_t U32() { Need(4); uint32_t v = uint32_t(d[p]) | uint32_t(d[p + 1]) << 8 | uint32_t(d[p + 2]) << 16 | uint32_t(d[p + 3]) << 24; p += 4; return v; }
            uint16_t U16() { Need(2); uint16_t v = uint16_t(d[p] | d[p + 1] << 8); p += 2; return v; }
            uint8_t U8() { Need(1); return d[p++]; }
            float F32() { uint32_t b = U32(); float f; std::memcpy(&f, &b, 4); return f; }
            bool At(const char* tag, size_t at) const { return at + 4 <= d.size() && std::memcmp(&d[at], tag, 4) == 0; }
            void Tag(const char* tag)
            {
                if (!At(tag, p)) throw FormatError(std::string("save: expected ") + tag + " at " + std::to_string(p));
                p += 4;
            }
            size_t Find(const char* tag, size_t from) const
            {
                for (size_t i = from; i + 4 <= d.size(); i++) if (std::memcmp(&d[i], tag, 4) == 0) return i;
                return std::string::npos;
            }
            // count objects of a list from p up to end: each starts `pre` bytes before a SELO that has its WMOB 16
            // bytes on (a battle map unit's SELO doesn't)
            std::vector<SaveObject> Objects(int list, uint32_t count, size_t end, size_t pre)
            {
                std::vector<size_t> begins;
                for (size_t i = p; i + 20 <= end; i++)
                    if (std::memcmp(&d[i], "SELO", 4) == 0 && std::memcmp(&d[i + 16], "WMOB", 4) == 0) begins.push_back(i - pre);
                if (begins.size() != count || (count && begins[0] != p))
                    throw FormatError("save: a list of " + std::to_string(count) + " objects at " + std::to_string(p) + " doesn't read");
                std::vector<SaveObject> objects;
                for (size_t k = 0; k < begins.size(); k++)
                {
                    size_t b = k + 1 < begins.size() ? begins[k + 1] : end;
                    SaveObject o;
                    o.list = list;
                    o.bytes.assign(d.begin() + std::ptrdiff_t(begins[k]), d.begin() + std::ptrdiff_t(b));
                    objects.push_back(std::move(o));
                }
                p = end;
                return objects;
            }
        };

        struct Writer
        {
            std::vector<uint8_t> o;
            void U32(uint32_t v) { for (int i = 0; i < 4; i++) o.push_back(uint8_t(v >> (8 * i))); }
            void U16(uint16_t v) { o.push_back(uint8_t(v)); o.push_back(uint8_t(v >> 8)); }
            void U8(uint8_t v) { o.push_back(v); }
            void F32(float f) { uint32_t b; std::memcpy(&b, &f, 4); U32(b); }
            void Tag(const char* t) { o.insert(o.end(), t, t + 4); }
            void Bytes(const std::vector<uint8_t>& b) { o.insert(o.end(), b.begin(), b.end()); }
            void List(const std::vector<SaveObject>& objects)
            {
                Tag("WMOB");
                U32(uint32_t(objects.size()));
                for (const SaveObject& ob : objects) Bytes(ob.bytes);
            }
        };
    }

    int SaveObject::Type() const { return int(Get32(bytes, 0)); }
    int SaveObject::Id() const { return int(Get32(bytes, SeloAt() + 4)); }
    int SaveObject::X() const { return int(int32_t(Get32(bytes, SeloAt() + 8))); }
    int SaveObject::Y() const { return int(int32_t(Get32(bytes, SeloAt() + 12))); }
    int SaveObject::Sector() const { return int(Get32(bytes, SeloAt() + 20)); }
    float SaveObject::Value(int i) const
    {
        if (!Kind2()) return 0;
        uint32_t b = Get32(bytes, 4 + size_t(i) * 4);
        float f;
        std::memcpy(&f, &b, 4);
        return f;
    }
    void SaveObject::SetPosition(int x, int y) { Put32(bytes, SeloAt() + 8, uint32_t(x)); Put32(bytes, SeloAt() + 12, uint32_t(y)); }
    static size_t LastBatp(const std::vector<uint8_t>& b)
    {
        size_t at = std::string::npos;
        for (size_t i = 16; i + 4 <= b.size(); i++)
            if (std::memcmp(&b[i], "BATP", 4) == 0) at = i;
        return at;
    }
    bool SaveObject::BattlePosition(float& x, float& y) const
    {
        const size_t at = LastBatp(bytes);
        if (at == std::string::npos) return false;
        uint32_t a = Get32(bytes, at - 16), b = Get32(bytes, at - 12);
        std::memcpy(&x, &a, 4);
        std::memcpy(&y, &b, 4);
        return true;
    }
    std::vector<std::pair<float, float>> SaveObject::BattlePositions() const
    {
        std::vector<std::pair<float, float>> out;
        for (size_t i = 0; i + 16 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "BAMO", 4) == 0)
            {
                uint32_t a = Get32(bytes, i + 8), b = Get32(bytes, i + 12);
                float x, y;
                std::memcpy(&x, &a, 4);
                std::memcpy(&y, &b, 4);
                out.push_back({ x, y });
            }
        return out;
    }

    std::vector<int> SaveObject::BattleHealths() const
    {
        std::vector<int> out;
        for (size_t i = 0; i + 24 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "BAMO", 4) == 0) out.push_back(int(Get32(bytes, i + 20)));
        return out;
    }

    void SaveObject::SetBattleHealth(size_t unit, int health)
    {
        size_t n = 0;
        for (size_t i = 0; i + 24 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "BAMO", 4) == 0 && n++ == unit) { Put32(bytes, i + 20, uint32_t(health)); return; }
    }

    // A vehicle of the object destroyed in a battle: it goes from the object's list of battle units (+0x4C; what
    // 0x421F90 counts for the battle report and the war map's pips show). A vehicle's entry is 52 bytes: its type,
    // SELO with its id and place, BAMO with five values, BATP, VEHI; the count after BMOL goes down by one.
    bool SaveObject::RemoveBattleUnit(size_t unit)
    {
        size_t n = 0;
        for (size_t i = 20; i + 32 <= bytes.size(); i++)
        {
            if (std::memcmp(&bytes[i], "BAMO", 4) != 0 || n++ != unit) continue;
            if (std::memcmp(&bytes[i - 16], "SELO", 4) != 0 || std::memcmp(&bytes[i + 24], "BATPVEHI", 8) != 0) return false;
            size_t list = std::string::npos;
            for (size_t j = 0; j + 8 <= i; j++) if (std::memcmp(&bytes[j], "BMOL", 4) == 0) list = j;
            if (list == std::string::npos || Get32(bytes, list + 4) == 0) return false;
            Put32(bytes, list + 4, Get32(bytes, list + 4) - 1);
            bytes.erase(bytes.begin() + std::ptrdiff_t(i - 20), bytes.begin() + std::ptrdiff_t(i + 32));
            return true;
        }
        return false;
    }

    SaveObject SaveObject::MakeUnit(int type, int list, int sector, int vehicles, int health, bool factory)
    {
        SaveObject o;
        o.list = list;
        std::vector<uint8_t>& b = o.bytes;
        auto word = [&](uint32_t v) { for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i))); };
        auto tag = [&](const char* t) { b.insert(b.end(), t, t + 4); };
        word(uint32_t(type));
        tag("SELO"); word(0); word(0); word(0);
        tag("WMOB"); word(uint32_t(sector));
        tag("BMOL"); word(uint32_t(vehicles));
        for (int v = 0; v < vehicles; v++)
        {
            word(uint32_t(type));
            tag("SELO"); word(0); word(0xCDCDCDCDu); word(0xCDCDCDCDu);
            tag("BAMO"); word(0); word(0); word(0); word(0); word(uint32_t(health));
            tag("BATP"); tag("VEHI");
        }
        word(factory ? 1 : 0);
        if (factory) word(0);
        word(0);
        tag("VEHU");
        return o;
    }

    size_t SaveObject::BattleUnitCount() const
    {
        size_t list = std::string::npos;
        for (size_t j = 0; j + 8 <= bytes.size(); j++) if (std::memcmp(&bytes[j], "BMOL", 4) == 0) { list = j; break; }
        return list == std::string::npos ? 0 : Get32(bytes, list + 4);
    }

    // A vehicle's entry is 52 bytes (RemoveBattleUnit): the last one is copied onto the end of the list
    bool SaveObject::DuplicateLastVehicle()
    {
        size_t list = std::string::npos, last = std::string::npos;
        for (size_t j = 0; j + 8 <= bytes.size(); j++) if (std::memcmp(&bytes[j], "BMOL", 4) == 0) { list = j; break; }
        if (list == std::string::npos) return false;
        for (size_t i = 20; i + 32 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "BAMO", 4) == 0 && std::memcmp(&bytes[i - 16], "SELO", 4) == 0 &&
                std::memcmp(&bytes[i + 24], "BATPVEHI", 8) == 0) last = i - 20;
        if (last == std::string::npos) return false;
        const std::vector<uint8_t> entry(bytes.begin() + std::ptrdiff_t(last), bytes.begin() + std::ptrdiff_t(last + 52));
        bytes.insert(bytes.begin() + std::ptrdiff_t(last + 52), entry.begin(), entry.end());
        Put32(bytes, list + 4, Get32(bytes, list + 4) + 1);
        return true;
    }

    void SaveObject::SetBattlePosition(float x, float y)
    {
        const size_t at = LastBatp(bytes);
        if (at == std::string::npos) return;
        uint32_t a, b;
        std::memcpy(&a, &x, 4);
        std::memcpy(&b, &y, 4);
        Put32(bytes, at - 16, a);
        Put32(bytes, at - 12, b);
    }

    void SaveObject::SetType(int type)
    {
        const uint32_t old = Get32(bytes, 0);
        Put32(bytes, 0, uint32_t(type));
        for (size_t i = 4; i + 8 <= bytes.size(); i++)
            if (Get32(bytes, i) == old && std::memcmp(&bytes[i + 4], "SELO", 4) == 0) Put32(bytes, i, uint32_t(type));
    }

    void SaveObject::SetValue(int i, float v)
    {
        if (!Kind2()) return;
        uint32_t b;
        std::memcpy(&b, &v, 4);
        Put32(bytes, 4 + size_t(i) * 4, b);
    }
    void SaveObject::SetSector(int sector) { Put32(bytes, SeloAt() + 20, uint32_t(sector)); }
    std::vector<size_t> SaveObject::IdOffsets() const
    {
        std::vector<size_t> at;
        for (size_t i = 0; i + 8 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "SELO", 4) == 0) at.push_back(i + 4);
        return at;
    }

    int SaveSector::Owner() const { return int(Get32(rest, 0)); }
    void SaveSector::SetOwner(int owner) { Put32(rest, 0, uint32_t(owner)); }

    namespace
    {
        // The Damage Layer's two blocks, as 0x405100 writes them after "DMGL", the width, the height and +0x18 (the
        // packed stream's size): for each, its size (vt+0x2C) and then the block through vt+0x14, which writes the
        // size again before the bytes. So: 0x410, 0x410, the FFUH header and counts (0x410 bytes); n, n, the stream
        // (n bytes). The engine's saves before update 178 had each size once; they are still read.
        struct DamageBlocks { size_t header = 0, stream = 0, streamSize = 0, end = 0; };
        bool FindDamage(const std::vector<uint8_t>& rest, DamageBlocks& b)
        {
            if (rest.size() < 20 || std::memcmp(&rest[4], "DMGL", 4) != 0) return false;
            b = {};
            if (Get32(rest, 8) + Get32(rest, 12) == 0) { b.end = 20; return true; }
            size_t at = 20;
            auto block = [&](size_t& start, size_t& size) {
                if (at + 8 > rest.size()) return false;
                size = Get32(rest, at);
                at += 4;
                // the size again (the game's), unless the block starts here (the engine's old saves)
                const bool twice = Get32(rest, at) == size && !(at + 4 <= rest.size() && std::memcmp(&rest[at], "FFUH", 4) == 0);
                if (twice) at += 4;
                start = at;
                at += size;
                return at <= rest.size();
            };
            size_t headerSize = 0;
            if (!block(b.header, headerSize) || headerSize != 0x410) return false;
            if (!block(b.stream, b.streamSize)) return false;
            b.end = at;
            return true;
        }
        // where the DMGL after the owner ends (0 if it isn't there)
        size_t DamageEnd(const std::vector<uint8_t>& rest)
        {
            DamageBlocks b;
            return FindDamage(rest, b) ? b.end : 0;
        }
    }

    bool SaveSector::Damage(int& width, int& height, std::vector<uint8_t>& grid) const
    {
        DamageBlocks b;
        if (!FindDamage(rest, b)) return false;
        width = int(Get32(rest, 8));
        height = int(Get32(rest, 12));
        if (width + height == 0) return false;
        std::vector<uint8_t> packed(rest.begin() + std::ptrdiff_t(b.header), rest.begin() + std::ptrdiff_t(b.header + 0x410));
        packed.insert(packed.end(), rest.begin() + std::ptrdiff_t(b.stream), rest.begin() + std::ptrdiff_t(b.stream + b.streamSize));
        // 0 bits: one byte repeated, the stream's (0x4155B0 sets +0x20 there, 0x4158E0 fills with it)
        if (packed.size() > 0x410 && Get32(packed, 12) == 0)
        {
            grid.assign(size_t(Get32(packed, 4)), packed[0x410]);
            return grid.size() == size_t(width) * size_t(height);
        }
        try { grid = ffuh::Decompress(packed); }
        catch (const std::exception&) { return false; }
        return grid.size() == size_t(width) * size_t(height);
    }

    void SaveSector::SetDamage(int width, int height, const std::vector<uint8_t>& grid)
    {
        const size_t end = DamageEnd(rest);
        if (!end) return;
        std::vector<uint8_t> chunk(16, 0);
        std::memcpy(&chunk[0], "DMGL", 4);
        Put32(chunk, 4, uint32_t(width));
        Put32(chunk, 8, uint32_t(height));
        if (width + height != 0)
        {
            const std::vector<uint8_t> packed = ffuh::Compress(grid);
            const uint32_t size = Get32(packed, 8);
            Put32(chunk, 12, size);
            auto word = [&](uint32_t v) { const size_t at = chunk.size(); chunk.resize(at + 4); Put32(chunk, at, v); };
            word(0x410);
            word(0x410);
            chunk.insert(chunk.end(), packed.begin(), packed.begin() + 0x410);
            word(size);
            word(size);
            chunk.insert(chunk.end(), packed.begin() + 0x410, packed.begin() + 0x410 + std::ptrdiff_t(size));
        }
        rest.erase(rest.begin() + 4, rest.begin() + std::ptrdiff_t(end));
        rest.insert(rest.begin() + 4, chunk.begin(), chunk.end());
    }

    SaveFile SaveFile::Parse(std::span<const uint8_t> data)
    {
        Reader r{ data };
        SaveFile s;
        s.version = r.U32();
        r.Tag("GAME");
        uint32_t length = r.U32();
        r.Need(length);
        s.game.assign(data.begin() + std::ptrdiff_t(r.p), data.begin() + std::ptrdiff_t(r.p + length));
        r.p += length;
        r.Tag("WMAP");
        for (uint32_t& w : s.warMap) w = r.U32();
        r.Tag("CALN");
        s.hours = r.F32();
        r.Tag("DtTm");
        for (uint16_t& v : s.dateTime) v = r.U16();
        size_t mpad = r.Find("MPAD", r.p);
        if (mpad == std::string::npos) throw FormatError("save: no MPAD");
        s.research.assign(data.begin() + std::ptrdiff_t(r.p), data.begin() + std::ptrdiff_t(mpad));
        r.p = mpad;
        r.Tag("MPAD");
        for (uint32_t n = r.U32(); n > 0; n--)
        {
            SaveMessage m;
            uint32_t len = r.U32();
            r.Need(len);
            size_t textLength = 0;
            while (textLength < len && data[r.p + textLength]) textLength++;
            m.text.assign(reinterpret_cast<const char*>(&data[r.p]), textLength);
            r.p += len;
            m.life = r.U32();
            m.state = r.U32();
            m.hour = r.U32();
            s.messages.push_back(std::move(m));
        }
        // the routes: where the last one's objects end is where the pods' two u32s are, just before sector 0
        uint32_t routeCount = r.U32();
        size_t sector0At = r.Find("DMGL", r.p);
        if (sector0At == std::string::npos || sector0At < r.p + 12) throw FormatError("save: no sector 0");
        sector0At -= 4;
        for (uint32_t k = 0; k < routeCount; k++)
        {
            SaveRoute route;
            route.origin = r.U32();
            route.destination = r.U32();
            route.flag5F = r.U32();
            route.flag5E = r.U32();
            route.field44 = r.U32();
            uint32_t n = r.U32();
            if (n > 64) throw FormatError("save: a route of " + std::to_string(n) + " sectors");
            route.added = r.U32();
            for (uint32_t i = 0; i < n; i++) route.path.push_back(r.U32());
            route.field50 = r.U8();
            // the route's units (WarMapRouteEntryList 0x427F70): u32 count, then each entry: its object as a list
            // writes one (u32 type and the object's own save), then the entry's eight values (0x42B250). The
            // engine's saves before update 137 had "WMOB", the count and the objects alone; still read.
            const bool legacy = r.At("WMOB", r.p);
            if (legacy) r.p += 4;
            uint32_t count = r.U32();
            size_t end = sector0At - 8;
            if (k + 1 < routeCount)
            {
                end = std::string::npos;
                if (legacy)
                {
                    // the next route's WMOB list (one with no SELO 16 bytes before it), then back over its header
                    for (size_t q = r.Find("WMOB", r.p); q != std::string::npos && end == std::string::npos; q = r.Find("WMOB", q + 4))
                    {
                        if (q >= 16 && std::memcmp(&data[q - 16], "SELO", 4) == 0) continue;
                        for (uint32_t m = 0; m <= 64 && end == std::string::npos; m++)
                        {
                            size_t h = q - 29 - 4 * size_t(m);
                            if (q < 29 + 4 * size_t(m) || h < r.p) break;
                            Reader peek{ data, h + 20 };
                            if (peek.U32() == m) end = h;
                        }
                    }
                }
                else
                {
                    // the next route's header: the first place after this one's units that reads as one (sectors
                    // 1-30, the flags 0 or 1, the 5, a path from its origin to its destination, a count, and a
                    // unit's type and SELO after it)
                    auto sector = [](uint32_t v) { return v >= 1 && v <= 30; };
                    for (size_t h = r.p + 4; h + 37 <= sector0At && end == std::string::npos; h++)
                    {
                        Reader peek{ data, h };
                        const uint32_t origin = peek.U32(), destination = peek.U32(), f5F = peek.U32(), f5E = peek.U32(), five = peek.U32();
                        const uint32_t m = peek.U32();
                        if (!sector(origin) || !sector(destination) || f5F > 1 || f5E > 1 || five != 5 || m < 1 || m > 64) continue;
                        if (h + 28 + 4 * size_t(m) + 1 + 4 + 8 > sector0At) continue;
                        peek.U32();
                        bool good = true;
                        uint32_t first = 0, last = 0;
                        for (uint32_t i = 0; i < m && good; i++)
                        {
                            last = peek.U32();
                            if (i == 0) first = last;
                            good = sector(last);
                        }
                        if (!good || first != origin || last != destination || peek.U8() > 1) continue;
                        const uint32_t units = peek.U32();
                        if (units < 1 || units > 4096 || !r.At("SELO", peek.p + 4)) continue;
                        end = h;
                    }
                }
                if (end == std::string::npos) throw FormatError("save: a route's end not found");
            }
            route.objects = r.Objects(0, count, end, 4);
            // each object is followed by its entry's eight values (0x42B150 after the object's own load); taken
            // off the object's bytes when every one of them reads as an entry of this route
            {
                auto f32 = [](const std::vector<uint8_t>& b, size_t at) { float f; std::memcpy(&f, &b[at], 4); return f; };
                std::vector<SaveRouteEntry> entries;
                for (const SaveObject& o : route.objects)
                {
                    if (o.bytes.size() < 32 + 24) break;
                    const size_t at = o.bytes.size() - 32;
                    SaveRouteEntry e;
                    e.segment = int32_t(Get32(o.bytes, at));
                    e.index = int32_t(Get32(o.bytes, at + 4));
                    e.speed = f32(o.bytes, at + 8);
                    e.remaining = f32(o.bytes, at + 12);
                    e.hoursLeft = f32(o.bytes, at + 16);
                    e.segmentLeft = f32(o.bytes, at + 20);
                    e.segmentLength = f32(o.bytes, at + 24);
                    e.started = Get32(o.bytes, at + 28);
                    if (e.started > 1 || e.segment < 0 || uint32_t(e.segment) >= n || e.index < 0 || e.index > 4096 ||
                        !(e.segmentLength >= 0.0f) || !(e.speed >= 0.0f)) break;
                    entries.push_back(e);
                }
                if (!legacy && entries.size() == route.objects.size())
                {
                    for (SaveObject& o : route.objects) o.bytes.resize(o.bytes.size() - 32);
                    route.entries = std::move(entries);
                }
            }
            s.routes.push_back(std::move(route));
        }
        s.podHours = r.U32();
        s.pods = r.U32();
        if (r.p != sector0At) throw FormatError("save: sector 0 isn't where it should be");
        size_t first = r.Find("SECT", r.p);
        if (first == std::string::npos) throw FormatError("save: no sectors");
        s.sector0.rest.assign(data.begin() + std::ptrdiff_t(r.p), data.begin() + std::ptrdiff_t(first));
        r.p = first;
        for (int sector = 1; sector <= 30; sector++)
        {
            SaveSector& sec = s.sectors[size_t(sector - 1)];
            r.Tag("SECT");
            for (int l = 0; l < 6; l++)
            {
                r.Tag(ListTags[l]);
                r.Tag("WMOB");
                uint32_t count = r.U32();
                size_t end;
                if (l < 5)
                {
                    end = std::string::npos;
                    for (size_t q = r.Find(ListTags[l + 1], r.p); q != std::string::npos; q = r.Find(ListTags[l + 1], q + 4))
                        if (r.At("WMOB", q + 4)) { end = q; break; }
                }
                else
                {
                    end = r.Find("DMGL", r.p);
                    if (end != std::string::npos) end -= 4;
                }
                if (end == std::string::npos || end < r.p) throw FormatError("save: sector " + std::to_string(sector) + " doesn't read");
                sec.lists[size_t(l)] = r.Objects(l, count, end, l >= 2 ? 12 : 4);
            }
            size_t next = sector < 30 ? r.Find("SECT", r.p) : data.size();
            if (next == std::string::npos) throw FormatError("save: sector " + std::to_string(sector + 1) + " missing");
            sec.rest.assign(data.begin() + std::ptrdiff_t(r.p), data.begin() + std::ptrdiff_t(next));
            r.p = next;
        }
        return s;
    }

    std::vector<uint8_t> SaveFile::Write() const
    {
        Writer w;
        w.U32(version);
        w.Tag("GAME");
        w.U32(uint32_t(game.size()));
        w.Bytes(game);
        w.Tag("WMAP");
        for (uint32_t v : warMap) w.U32(v);
        w.Tag("CALN");
        w.F32(hours);
        w.Tag("DtTm");
        for (uint16_t v : dateTime) w.U16(v);
        w.Bytes(research);
        w.Tag("MPAD");
        w.U32(uint32_t(messages.size()));
        for (const SaveMessage& m : messages)
        {
            w.U32(uint32_t(m.text.size() + 1));
            w.o.insert(w.o.end(), m.text.begin(), m.text.end());
            w.U8(0);
            w.U32(m.life);
            w.U32(m.state);
            w.U32(m.hour);
        }
        w.U32(uint32_t(routes.size()));
        for (const SaveRoute& route : routes)
        {
            w.U32(route.origin);
            w.U32(route.destination);
            w.U32(route.flag5F);
            w.U32(route.flag5E);
            w.U32(route.field44);
            w.U32(uint32_t(route.path.size()));
            w.U32(route.added);
            for (uint32_t sector : route.path) w.U32(sector);
            w.U8(route.field50);
            if (route.entries.size() != route.objects.size()) w.List(route.objects);   // as it was read (before update 137)
            else
            {
                // 0x427F70: the count, then each entry: its object, then its own values (0x42B250)
                w.U32(uint32_t(route.objects.size()));
                for (size_t i = 0; i < route.objects.size(); i++)
                {
                    const SaveRouteEntry& e = route.entries[i];
                    w.Bytes(route.objects[i].bytes);
                    w.U32(uint32_t(e.segment));
                    w.U32(uint32_t(e.index));
                    w.F32(e.speed);
                    w.F32(e.remaining);
                    w.F32(e.hoursLeft);
                    w.F32(e.segmentLeft);
                    w.F32(e.segmentLength);
                    w.U32(e.started);
                }
            }
        }
        w.U32(podHours);
        w.U32(pods);
        w.Bytes(sector0.rest);
        for (const SaveSector& sec : sectors)
        {
            w.Tag("SECT");
            for (int l = 0; l < 6; l++)
            {
                w.Tag(ListTags[l]);
                w.List(sec.lists[size_t(l)]);
            }
            w.Bytes(sec.rest);
        }
        return std::move(w.o);
    }

    std::string SaveFile::Name() const
    {
        size_t n = 0;
        while (n < game.size() && n < 36 && game[n]) n++;
        return std::string(reinterpret_cast<const char*>(game.data()), n);
    }

    void SaveFile::SetName(const std::string& latin1)
    {
        if (game.size() < 36) game.resize(0x28, 0);
        for (size_t i = 0; i < 36; i++) game[i] = i < latin1.size() ? uint8_t(latin1[i]) : 0;
    }

    void SaveFile::MarkIds(const std::vector<uint8_t>& bytes, std::vector<bool>& used)
    {
        for (size_t i = 0; i + 8 <= bytes.size(); i++)
            if (std::memcmp(&bytes[i], "SELO", 4) == 0)
            {
                uint32_t id = Get32(bytes, i + 4);
                if (id < used.size()) used[id] = true;
            }
    }

    std::vector<bool> SaveFile::UsedIds() const
    {
        // every SELO id anywhere: the objects, the routes' units, and what each sector carries (its own data can
        // hold battle map objects too)
        std::vector<bool> used(32768, false);
        for (const SaveRoute& route : routes) for (const SaveObject& o : route.objects) MarkIds(o.bytes, used);
        for (const SaveSector& sec : sectors)
        {
            for (const auto& list : sec.lists) for (const SaveObject& o : list) MarkIds(o.bytes, used);
            MarkIds(sec.rest, used);
        }
        MarkIds(sector0.rest, used);
        return used;
    }

    int SaveFile::TakeId(std::vector<bool>& used)
    {
        for (size_t i = 0; i < used.size(); i++)
            if (!used[i]) { used[i] = true; return int(i); }
        return -1;
    }
}
