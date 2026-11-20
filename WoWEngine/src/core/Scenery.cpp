#include "core/Scenery.h"
#include "core/Bytes.h"

namespace wow
{
    std::vector<SceneryObject> LoadScenery(std::span<const uint8_t> data)
    {
        ByteReader r(data);
        r.U32();
        if (r.U32() != 0x4C4F4D42) throw FormatError("scenery: no BMOL list");
        uint32_t count = r.U32();
        std::vector<SceneryObject> out;
        out.reserve(count);
        for (uint32_t i = 0; i < count; i++)
        {
            SceneryObject o;
            o.type = int32_t(r.U32());
            o.x = int32_t(r.U32());
            o.y = int32_t(r.U32());
            if (o.type >= 0xCF && o.type <= 0xD4) o.extra[0] = int32_t(r.U32());
            else if (o.type >= 445 && o.type <= 447) for (int& e : o.extra) e = int32_t(r.U32());
            out.push_back(o);
        }
        return out;
    }
}
