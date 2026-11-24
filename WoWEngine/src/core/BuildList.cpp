#include "core/BuildList.h"

#include <cstring>

namespace wow
{
    namespace
    {
        uint32_t U32(const std::vector<uint8_t>& b, size_t at)
        {
            return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
        }
        void Put(std::vector<uint8_t>& out, uint32_t v) { for (int i = 0; i < 4; i++) out.push_back(uint8_t(v >> (8 * i))); }
    }

    static std::optional<ObjectTail> ReadAt(const std::vector<uint8_t>& b, size_t anchor, size_t end)
    {
        ObjectTail t;
        t.at = anchor;
        size_t p = anchor;
        auto need = [&](size_t n) { return p + n <= end; };
        if (!need(4)) return std::nullopt;
        const uint32_t has = U32(b, p);
        p += 4;
        if (has > 1) return std::nullopt;
        t.hasFactory = has != 0;
        if (t.hasFactory)
        {
            if (!need(4)) return std::nullopt;
            const uint32_t count = U32(b, p);
            p += 4;
            if (count > 32) return std::nullopt;
            if (count)
            {
                if (!need(1 + count * 32)) return std::nullopt;
                t.queue.busy = b[p++] != 0;
                for (uint32_t i = 0; i < count; i++)
                {
                    BuildEntry e;
                    e.type = int(U32(b, p));
                    e.target = U32(b, p + 4);
                    for (int k = 0; k < 4; k++) e.place[size_t(k)] = int32_t(U32(b, p + 8 + size_t(k) * 4));
                    e.repeat = int(U32(b, p + 24));
                    uint32_t f = U32(b, p + 28);
                    std::memcpy(&e.hours, &f, 4);
                    t.queue.entries.push_back(e);
                    p += 32;
                }
            }
        }
        if (!need(4)) return std::nullopt;
        t.last = U32(b, p);
        p += 4;
        if (p != end) return std::nullopt;
        t.end = end;
        return t;
    }

    std::optional<ObjectTail> ReadTail(const SaveObject& object)
    {
        const std::vector<uint8_t>& b = object.bytes;
        size_t end = b.size();
        if (end >= 4 && std::memcmp(&b[end - 4], "VEHU", 4) == 0) end -= 4;
        // after the last unit's closing tag
        size_t anchor = std::string::npos;
        for (size_t i = 0; i + 4 <= end; i++)
            if (std::memcmp(&b[i], "BATP", 4) == 0 || std::memcmp(&b[i], "VEHI", 4) == 0) anchor = i + 4;
        if (anchor != std::string::npos)
            if (auto t = ReadAt(b, anchor, end)) return t;
        // Some battle map objects write more after their tag (type 451 four more u32s), so failing that, the
        // shortest tail: no factory and the +0x3C
        if (end >= 8 && U32(b, end - 8) == 0) return ReadAt(b, end - 8, end);
        return std::nullopt;
    }

    void WriteTail(SaveObject& object, const ObjectTail& tail)
    {
        std::vector<uint8_t> out;
        Put(out, tail.hasFactory ? 1 : 0);
        if (tail.hasFactory)
        {
            Put(out, uint32_t(tail.queue.entries.size()));
            if (!tail.queue.entries.empty())
            {
                out.push_back(tail.queue.busy ? 1 : 0);
                for (const BuildEntry& e : tail.queue.entries)
                {
                    Put(out, uint32_t(e.type));
                    Put(out, e.target);
                    for (int32_t v : e.place) Put(out, uint32_t(v));
                    Put(out, uint32_t(e.repeat));
                    uint32_t f;
                    std::memcpy(&f, &e.hours, 4);
                    Put(out, f);
                }
            }
        }
        Put(out, tail.last);
        // where the tail is now: the object's bytes may have changed since it was read (a vehicle lost in a battle
        // takes 52 bytes out before it, SaveObject::RemoveBattleUnit), and the old place would be past the end
        // (update 256, a tester's crash in the war map's production after a battle)
        std::vector<uint8_t>& b = object.bytes;
        size_t at = tail.at, end = tail.end;
        if (const auto now = ReadTail(object)) { at = now->at; end = now->end; }
        if (at > end || end > b.size()) return;     // (nowhere it can go: left as it is)
        b.erase(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(end));
        b.insert(b.begin() + std::ptrdiff_t(at), out.begin(), out.end());
    }
}
