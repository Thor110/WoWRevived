// The battle map's flying objects (0x487730 and its classes: WAnimEffect, WExplosion, the smoke, the mortars'
// shells): their update (0x489050 / 0x487890), their emitters (0x47F050), their step (0x47F770), and the smoke a
// damaged object gives off (0x48AEF0 / 0x48ADD0). See docs/BATTLEMAP.md "Flying objects and emitters".
#include "battlemap/BattleMap.h"
#include "core/Random.h"
#include <algorithm>
#include <cmath>

namespace wow
{
    namespace
    {
        // 0x43FE90: a heading (4096 a turn) and a length to x, y
        void Polar(int angle, float length, float& x, float& y)
        {
            const double a = double(angle) * 0.000244140625 * 6.283185307;
            x = float(std::cos(a) * length);
            y = float(std::sin(a) * length);
        }
    }

    // 0x43A670, each update of the world (0x43A980, after the fog's decay) with "Show Smoke & particles" on: "Water
    // twinkles per frame" times, a point rand() % (the world's width less 40) + 20 by rand() % (its height less 40)
    // + 20; at its vertex (rounded, kept on the map) an ATM byte of 1-3 (water, flat) makes, unless rand() is under
    // (the byte less 1) x 10000, a glint, type 0x3CA + rand() % 3 (LE_1-LE_3): there at height 0, started, its
    // renderer's +0x26 8, its frame step (+0xC0) 100, moving 0.5 by 0.5 (vt+0x80)
    void BattleMap::WaterTwinkles()
    {
        if (!setup.showSmoke || setup.waterTwinkles <= 0) return;
        const int w = land.WorldWidth() - 40, h = land.WorldHeight() - 40;
        if (w <= 0 || h <= 0) return;
        const int64_t cell = land.Cell(), half = land.HalfCell();
        for (int n = 0; n < setup.waterTwinkles; n++)
        {
            const int x = random::Rand() % w + 20;
            const int y = random::Rand() % h + 20;
            const int vx = std::clamp(int((int64_t(int32_t(uint32_t(x) << 16)) + half) / cell), 0, land.Width() - 1);
            const int vy = std::clamp(int((int64_t(int32_t(uint32_t(y) << 16)) + half) / cell), 0, land.Height() - 1);
            const int atm = land.At(vx, vy).atm;
            if (atm <= 0 || atm >= 4) continue;
            if (random::Rand() < (atm - 1) * 10000) continue;
            const int i = SpawnEffect(0x3CA + random::Rand() % 3, float(x), float(y), 0.0f, 0.5f, 0.5f, 0.0f, 0.0f);
            if (i < 0) continue;
            effects[size_t(i)].frameStep = 100;
            effects[size_t(i)].depth = 8;
            for (ViewObject& o : objects)
                if (o.effect == i && o.light < 0) o.key = y + 8;
        }
    }

    // The factory's class for each flying type (0x446B30)
    BattleMap::EffectKind BattleMap::KindOf(int t)
    {
        if (t >= 0x16F && t <= 0x173) return EffectKind::Smoke;
        if ((t >= 0x18B && t <= 0x196) || t == 713 || t == 714 || t == 1002) return EffectKind::Shell;
        if (t >= 0x393 && t <= 0x395) return EffectKind::Shell;     // the gas clouds (WGasCloud 0x488CC0, a shell's class)
        if (t == 0x17B) return EffectKind::Plain;
        return EffectKind::Animated;
    }

    bool BattleMap::IsGas(int type) { return type >= 0x393 && type <= 0x395; }

    // The gas clouds (WGasCloud, 0x488CC0 on the mortar shell 0x488E80; types 0x393-0x395, Black Dust): started
    // (0x488D20) with +0xC2 240, 480 or 720 updates; each update (0x488D70), after the shell's (0x488F00): at or under
    // 10.0 high, over water (0x47FFE0) it's gone (vt+0x78), else it's put at 10.0 and its velocity x 0.65, the
    // upward one turned round; a bounce under 1 (|ftol| under 0.25) stops it. Every 15th of +0xC2 its puff (the
    // type's +0x98: 501-503) goes off (0x488070) at its point, ftol(its height), with its firer (+0xC4) and its own
    // type as the weapon. True when it's gone.
    bool BattleMap::GasTick(size_t index)
    {
        Effect& e = effects[index];
        if (e.lift <= 10.0f)
        {
            const int vx = std::clamp(int((int64_t(int32_t(uint32_t(int(e.x)) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
            const int vy = std::clamp(int((int64_t(int32_t(uint32_t(int(e.y)) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            if (terrain >= 1 && terrain <= 3) return true;
            e.vz = float(double(-e.vz) * 0.65);
            e.vx = float(double(e.vx) * 0.65);
            e.vy = float(double(e.vy) * 0.65);
            e.lift = 10.0f;
            if (double(std::abs(int(e.vz))) < 0.25) e.vx = e.vy = e.vz = 0;
        }
        if (e.count % 15 == 0)
            if (auto t = setup.units->find(e.type); t != setup.units->end())
                cascades.push_back({ t->second.field98, int(e.x), int(e.y), int(e.lift), e.group, e.attacker, e.type });
        return false;
    }

    // An object made (0x446B30), given its velocity (vt+0x80), its point and height (vt+0x40) and its gravity (+0x94),
    // and added (vt+0x24): with a sprite (its type's +0x28), a view object at the point raised by the height.
    // *Provisional*: one with no sprite is made only if its type has emitters (it'd otherwise do nothing).
    int BattleMap::SpawnEffect(int type, float x, float y, float lift, float vx, float vy, float vz, float g)
    {
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return -1;
        Effect e;
        e.type = type;
        e.kind = KindOf(type);
        e.x = x;
        e.y = y;
        e.lift = lift;
        e.vx = vx;
        e.vy = vy;
        e.vz = vz;
        e.g = g;
        e.alive = true;
        const int gtype = u->second.field28;
        const GType* gt = gtype ? res.FindGType(gtype) : nullptr;
        const Sprite* sprite = gtype ? res.GetSprite(gtype) : nullptr;
        const IndexedImage* image = gtype ? res.GetFrame(gtype, 0) : nullptr;
        const bool drawn = gt && sprite && image;
        if (!drawn && u->second.emitters.empty()) return -1;
        if (drawn)
        {
            e.gtype = gtype;
            e.frames = sprite->FrameCount();
            e.depth = image->width / 2 + 64;
        }
        // a dead one's slot taken again (its view object's gone), else a new one; one made this update waits for
        // the next to move
        e.born = ticks;
        int index = -1;
        for (size_t i = 0; i < effects.size(); i++)
            if (!effects[i].alive) { index = int(i); break; }
        if (index < 0) { effects.push_back(e); index = int(effects.size()) - 1; }
        else effects[size_t(index)] = e;
        if (drawn)
        {
            const int ix = int(x), iy = int(y);
            objects.push_back({ iy + e.depth, ix, land.ProjectY(iy, land.HeightAt(ix, iy) + int(lift)), nullptr, nullptr, gt->paletteSlot,
                                gtype, -1, -1, 0, index });
            std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
        }
        return index;
    }

    // Each update, for each flying object made before it: the smoke first rises (+0x84 less -1.5, 0x48DD30); an
    // animated one (0x489050, with a renderer) moves its frame on (0x40EF20), gone past the last; then (0x487890)
    // its emitters (0x47F050), its step (vt+0x54, 0x47F770: unless +0xB8 bit 10, the point moved by the velocity and
    // the height kept level over the ground's change (ftol(+0x80 - +0x84) less the ground at the new point, plus the
    // height and the upward speed), then the upward speed plus the gravity) and, under 0.0 high, gone (0x8057). A
    // shell then lands (0x488F00, ShellLands).
    // *Provisional*: the parent's vt+0x9C check (0x487890) isn't in; an effect's frames under 3 never end.
    void BattleMap::UpdateEffects()
    {
        bool moved = false;
        for (size_t i = 0; i < effects.size(); i++)
        {
            if (!effects[i].alive || effects[i].born == ticks) continue;
            if (effects[i].kind == EffectKind::Smoke) effects[i].lift = float(double(effects[i].lift) - -1.5);
            bool gone = false;
            if (effects[i].gtype && effects[i].kind != EffectKind::Shell && effects[i].kind != EffectKind::Plain && effects[i].frames >= 3)
            {
                effects[i].counter += effects[i].frameStep;
                gone = effects[i].counter > (effects[i].frames - 1) << 8;
            }
            if (!gone)
            {
                Emit(i);
                Effect& e = effects[i];
                const int before = land.HeightAt(int(e.x), int(e.y));
                e.x += e.vx;
                e.y += e.vy;
                const int after = land.HeightAt(int(e.x), int(e.y));
                e.lift = float(before - after) + e.lift + e.vz;
                e.vz += e.g;
                gone = e.lift < 0.0f;
                moved |= e.vx != 0 || e.vy != 0 || e.vz != 0 || e.kind == EffectKind::Smoke;
                if (e.kind == EffectKind::Shell) gone = ShellLands(i) || gone;
                if (IsGas(e.type) && GasTick(i)) gone = true;
                // *Provisional*: one with nothing to draw goes once its emitters are spent (in the game it stays,
                // 0x489050 with no renderer only taking its step); the cascading ones (0x253-0x255, no sprite) not
                // before their 11th update has set off the next (0x48CDE0)
                const bool cascading = e.type >= 0x253 && e.type <= 0x255 && e.age <= 11;
                gone = gone || (!e.gtype && e.emitDone && !cascading);
            }
            Effect& e = effects[i];
            // 0x48CDE0 (the update of 0x253-0x255): at its 11th update (+0xD0) the next smaller (0x255 -> 0x254 ->
            // 0x253 -> 0x252) goes off (0x488070) at its point, y then x each moved Random(8) - 4.0, at ftol(its
            // height), with its +0xCC and +0xC4
            if (e.type >= 0x253 && e.type <= 0x255 && e.age++ == 11)
            {
                const int dy = random::Random(8), dx = random::Random(8);
                cascades.push_back({ e.type - 1, int(float(dx) + e.x - 4.0f), int(float(dy) + e.y - 4.0f), int(e.lift), e.group, e.attacker });
            }
            if (gone)
            {
                e.alive = false;
                objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.effect == int(i); }), objects.end());
                continue;
            }
            if (!e.gtype) continue;
            for (ViewObject& o : objects)
                if (o.effect == int(i))
                {
                    o.x = int(e.x);
                    o.y = land.ProjectY(int(e.y), land.HeightAt(int(e.x), int(e.y)) + int(e.lift));
                    o.key = int(e.y) + e.depth;
                }
        }
        if (moved) std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
        // the shells' landings, made once the list is walked (they add effects)
        std::vector<Landing> now;
        now.swap(landings);
        for (const Landing& l : now) ImpactAt(l.size, l.x, l.y, l.z, l.group, l.attacker, l.weapon);
        std::vector<Cascade> next;
        next.swap(cascades);
        for (const Cascade& c : next) Explode(c.type, c.x, c.y, c.z, c.firer, c.attacker, c.weapon);
    }

    // 0x47F050, with "Show Smoke & particles" on and a list: for each entry (its type's emitter, record 29: +4 the
    // type, +8 the spread, +0xA the limit, +0xC / +0x10 the slowest and fastest, +0x14 the gravity, +0x18 the rate),
    // its fraction goes up by the rate and the whole part n is taken off; n of the type are made, each: rand() %
    // spread as the angle up and a speed rand() x 0.000030601626 x (fastest - slowest) + slowest, split by 0x43FE90
    // into across and up; across along rand() & 0xFFF; at the object's point and height with that velocity and the
    // gravity. Then the entry's count goes up, and past a limit (not 0) the list is freed.
    void BattleMap::Emit(size_t index)
    {
        if (!setup.showSmoke || effects[index].emitDone) return;
        auto t = setup.units->find(effects[index].type);
        if (t == setup.units->end() || t->second.emitters.empty()) return;
        const auto& list = t->second.emitters;
        if (effects[index].acc.size() != list.size())
        {
            effects[index].acc.assign(list.size(), 0.0f);
            effects[index].emitted.assign(list.size(), 0);
        }
        for (size_t i = 0; i < list.size(); i++)
        {
            const auto& m = list[i];
            float& acc = effects[index].acc[i];
            acc += m.f[3];
            const int whole = int(acc);
            acc -= float(whole);
            for (int k = 0; k < whole; k++)
            {
                const int spread = int16_t(m.b);
                const int up = spread != 0 ? random::Rand() % spread : 0;
                const int r = random::Rand();
                const float speed = float(double(r) * double(0.000030601626f) * double(m.f[1] - m.f[0]) + double(m.f[0]));
                float across = 0, vz = 0;
                Polar(up, speed, across, vz);
                const int dir = random::Rand() & 0xFFF;
                float vx = 0, vy = 0;
                Polar(dir, across, vx, vy);
                const Effect& e = effects[index];
                SpawnEffect(m.a, e.x, e.y, e.lift, vx, vy, vz, m.f[2]);
            }
            Effect& e = effects[index];
            e.emitted[i]++;
            if (m.c != 0 && e.emitted[i] > int16_t(m.c))
            {
                e.emitDone = true;
                return;
            }
        }
    }

    // dword_4B84C8 (30) x the word / 20 (0x66666667, >> 3)
    static int Ticks(int word) { return int((int64_t(word * 30) * 0x66666667LL >> 32) >> 3) + ((word * 30) < 0 ? 1 : 0); }

    // A light part made at a point (z its height): with "Show lights" on (0x48DE80: otherwise it goes; a parent of
    // category 0-3 also rolls against the hour, done for the scenery's in LoadScenery), raised 16 (+0xA0 less -16).
    // As it's added (0x48DDC0): with any of record 13's times (+4, +8, +0xC) it fades: the intensity 0, the count
    // Ticks(a), the step the float / that, phase 0, and it joins the updated list; then 0x47EE10 makes its renderer
    // (vt+0x28, 0x48E050), which sets the intensity to the float again. Its range is its type's +0x44 word.
    int BattleMap::AddLight(int type, int x, int y, int z)
    {
        if (!setup.showLights) return -1;
        auto u = setup.units->find(type);
        if (u == setup.units->end()) return -1;
        ObjOjd::BattleTables::Thirteen t;
        if (setup.lights)
            if (auto it = setup.lights->find(type); it != setup.lights->end()) t = it->second;
        Light l{ x, y, z + 16, int(int16_t(u->second.hitPoints)), t.f };
        l.type = type;
        if (t.a != 0 || t.b != 0 || t.c != 0)
        {
            l.fading = true;
            l.count = Ticks(t.a);
            l.step = t.f / float(l.count);
            l.phase = 0;
        }
        l.current = t.f;
        int index = -1;
        for (size_t i = 0; i < lights.size(); i++)
            if (!lights[i].alive) { index = int(i); break; }
        if (index < 0) { lights.push_back(l); index = int(lights.size()) - 1; }
        else lights[size_t(index)] = l;
        objects.push_back({ y + (-32 - l.range), x, y, nullptr, nullptr, 0, 0, index });
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
        return index;
    }

    // 0x48E100, each update of a fading light: with the count under 0 the phase moves on: from 0 (rising) the step 0
    // and the count Ticks(b), phase 1 (holding); from 1 the count Ticks(c), the step -(intensity / that), phase 2
    // (falling); from 2 it's gone (0x8057). Then its renderer takes the intensity (0x4A2840), the intensity goes up
    // by the step and the count down by 1; the objects in its reach get 0x8066 (lit again).
    // With record 13's a, b, c of 1, 1, 16 (338, a rocket's hit) it shows 0.35, 0.70, then 1.05 for three updates, then
    // falls to 0 over 25.
    // *Provisional*: 0x8066 (the IOB objects lit by it) isn't in.
    void BattleMap::UpdateLights()
    {
        for (size_t i = 0; i < lights.size(); i++)
        {
            Light& l = lights[i];
            if (!l.alive || !l.fading) continue;
            if (l.count < 0)
            {
                ObjOjd::BattleTables::Thirteen t;
                if (setup.lights)
                    if (auto it = setup.lights->find(l.type); it != setup.lights->end()) t = it->second;
                if (l.phase == 0) { l.step = 0; l.count = Ticks(t.b); l.phase = 1; }
                else if (l.phase == 1) { l.phase = 2; l.count = Ticks(t.c); l.step = -(l.current / float(l.count)); }
                else
                {
                    l.alive = false;
                    objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.light == int(i); }), objects.end());
                    continue;
                }
            }
            l.intensity = l.current;
            l.current += l.step;
            l.count--;
        }
    }

    // 0x48AEF0, each update of an object (0x4967E0, after its look round): every 15th update (dword_4D2558 % 15),
    // with "Show Smoke & particles" on (and vt+0x9C not set): the level ftol(5 - its health fraction (vt+0xF4,
    // 0x48ADA0: +0xC4 / 0x47DA20) x 5), at most 4; above 0, if rand() is under 0xCCC a puff of 1000 (0x48ADD0), then
    // a vehicle (+0xB8 bit 0) puffs 0x16F + level - 1, anything else every one of 0x16F to that.
    // *Provisional*: +0xC0 bit 3's 0x80C1 to the parent's +0x48 +0x6C and vt+0x9C aren't in.
    void BattleMap::Smoulder(size_t index)
    {
        if (ticks % 15 != 0 || !setup.showSmoke) return;
        const Unit& u = units[index];
        const int max = MaxHealth(u.type);
        if (max == 0) return;
        const float fraction = float(u.health) / float(max);
        int level = int(double(fraction) * -5.0 + double(5.0f));
        if (level <= 0) return;
        level = std::min(level, 4);
        const int top = 0x16F + level - 1;
        if (random::Rand() < 0xCCC) Puff(1000, u.type, u.x, u.y, u.drive.lift, u.drive.radius);
        if (u.vehicle) Puff(top, u.type, u.x, u.y, u.drive.lift, u.drive.radius);
        else
            for (int t = 0x16F; t <= top; t++) Puff(t, u.type, units[index].x, units[index].y, units[index].drive.lift, units[index].drive.radius);
    }

    // 0x48ADD0: the type made at a point r = +0xAC / 3 + 1 about it (rand() % 2r - r each way, as floats on its
    // point, ftol'd) and rand() % r over its height plus its +0x41 byte x 1.414, standing still
    void BattleMap::Puff(int type, int ownType, float px, float py, float lift, int radius)
    {
        const int r = radius / 3 + 1;
        const float fr = float(r);
        const int x = int(float(random::Rand() % (2 * r)) + px - fr);
        const int y = int(float(random::Rand() % (2 * r)) + py - fr);
        auto t = setup.units->find(ownType);
        const int size = t != setup.units->end() ? t->second.size41 : 0;
        const int z = int(double(float(random::Rand() % r) + lift) - double(size) * -1.414);
        SpawnEffect(type, float(x), float(y), float(z));
    }

    // WScenery's update (vtable 0x4AE5B0 +0x10) is 0x48AEF0 too: a damaged scenery object (not a vehicle) puffs every
    // level up to its own
    void BattleMap::SmoulderProp(size_t index)
    {
        if (ticks % 15 != 0 || !setup.showSmoke) return;
        const Prop& p = props[index];
        const int max = MaxHealth(p.type);
        if (max == 0) return;
        const float fraction = float(p.health) / float(max);
        int level = int(double(fraction) * -5.0 + double(5.0f));
        if (level <= 0) return;
        level = std::min(level, 4);
        const int top = 0x16F + level - 1;
        auto t = setup.units->find(p.type);
        const int radius = t != setup.units->end() ? int16_t(t->second.hitPoints) : 0;
        if (random::Rand() < 0xCCC) Puff(1000, p.type, props[index].x, props[index].y, props[index].z, radius);
        for (int k = 0x16F; k <= top; k++) Puff(k, p.type, props[index].x, props[index].y, props[index].z, radius);
    }
}
