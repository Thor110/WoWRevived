#include "frontend/Briefing.h"
#include "warmap/WarMapData.h"

#include <cstdio>

namespace wow
{
    namespace
    {
        constexpr int Backdrop = 0x91;
        constexpr int BriefingPalette = 0x2EE;   // hb
        constexpr int MapShadeSlot = 0x15;    // 0x4A2100: 0x40B500(0x15, 0)
        constexpr int MapBoxX = 354, MapBoxW = 255, MapBoxY = 95, MapBoxH = 285;

        // 0x41B440: the calendar as "hh:mmap dd mmm yyyy" (12-hour clock, 0 shown as 12)
        std::string CalendarText(const uint16_t dt[6])
        {
            static constexpr const char* Months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            int hour = dt[2] % 12;
            if (hour == 0) hour = 12;
            char text[32];
            std::snprintf(text, sizeof(text), "%02d:%02d%s %02d %s %04d", hour, dt[1] % 100, dt[2] >= 12 ? "pm" : "am",
                          (dt[3] + 1) % 100, Months[dt[4] % 12], dt[5] % 10000);
            return text;
        }
    }

    BattleScreen::BattleScreen(int id, Resources& res, BattleColours colours)
        : Screen(id, res, Backdrop, BriefingPalette), colours(colours)
    {
        const bool human = res.Human();
        left = human ? 55 : 85;
        right = human ? 285 : 276;
        centre = left + (right - left) / 2;
        y = human ? 170 : 72;
        const Font* font = res.GetFont(TextFont);
        lineHeight = (font ? font->Height() : 0) + 2;
    }

    void BattleScreen::Add(std::string text, int x, int flags, int level)
    {
        lines.push_back({ std::move(text), x, y, flags, level });
    }

    void BattleScreen::AddPair(int label, std::string value)
    {
        Add(res.String(label), left, 0, colours.detail);
        Add(std::move(value), right, TextRight, colours.detail);
    }

    void BattleScreen::Draw(Canvas& canvas) const
    {
        for (const Line& l : lines)
        {
            std::string text = l.wrap ? canvas.Wrap(TextFont, l.text, right - left) : l.text;
            canvas.DrawString(TextFont, text, { l.x, l.y, 0, 0 }, l.flags, l.level);
        }
        if (mapSector < 1) return;
        char name[32];
        std::snprintf(name, sizeof(name), res.Human() ? "maps\\hmin%02d.spr" : "maps\\mmin%02d.spr", mapSector);
        const IndexedImage* map = res.GetFileImage(name);
        const ShadeTable* shade = res.GetShade(MapShadeSlot);
        if (!map || !shade) return;
        canvas.DrawImage(*map, shade->Level(0), MapBoxX + (MapBoxW - int(map->width)) / 2, MapBoxY + (MapBoxH - int(map->height)) / 2);
    }

    Briefing::Briefing(Resources& res, BattleColours colours, const BriefingInfo& info, int delay)
        : BattleScreen(Id, res, colours), ticksLeft(delay)
    {
        Add(res.String(info.heading) + "\n" + res.String(warmap::NameText(info.sector)), centre, TextCentreX, colours.sectorName);
        y += lineHeight * 2 + 16;
        Add(res.String(0x4A0), centre, TextCentreX, colours.heading);   // "Local Conditions:"
        y += lineHeight + 16;
        Add(res.String(0x4A1), left, 0, colours.detail);                // "Date/Time:"
        if (res.Human())
        {
            std::string when = CalendarText(info.dateTime);
            Add(when.substr(0, 7), right, TextRight, colours.detail);
            y += lineHeight;
            Add(when.substr(8), right, TextRight, colours.detail);
        }
        else
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%07.2f", double(info.hours) * (1.0 / 24.0)); // dbl_4B0F48
            Add(text, right, TextRight, colours.detail);
        }
        y += lineHeight * 2;
        if (info.units || info.facilities)
        {
            Add(res.String(0x4A4), centre, TextCentreX, colours.heading); // "Forces Present:"
            y += lineHeight + 16;
            char text[32];
            if (info.units)
            {
                std::snprintf(text, sizeof(text), "%d (%d)", info.units, info.vehicles);
                AddPair(0x4A6, text);                                     // "Units (Vehicles):"
                y += lineHeight;
            }
            if (info.facilities)
            {
                std::snprintf(text, sizeof(text), "%d", info.facilities);
                AddPair(0x4A7, text);                                     // "Facilities:"
                y += lineHeight;
            }
        }
        SetMap(info.sector);
    }

    void Briefing::Update()
    {
        if (!done && --ticksLeft <= 0) { done = true; sent = Command{ 0x71, 0 }; }
    }

    // The Briefing posts its commands to itself, so they take the Fullscreen View's way (0x4065E0): the fade out
    // and, 5 ticks later, on to the world (+0x84). Sent from Update so Escape fades like any other key.
    std::optional<Command> Briefing::Leave()
    {
        if (!done) { done = true; sent = Command{ 0x98, 0 }; }
        return std::nullopt;
    }

    std::optional<Command> Briefing::OnKey(int) { return Leave(); }
    std::optional<Command> Briefing::OnMouseButton(int, int, bool down) { return down ? Leave() : std::nullopt; }
    std::optional<Command> Briefing::OnRightButton(int, int, bool down) { return down ? Leave() : std::nullopt; }

    Debriefing::Debriefing(Resources& res, BattleColours colours, const DebriefingInfo& info, int delay)
        : BattleScreen(Id, res, colours), ticksLeft(delay)
    {
        int heading;
        if (!info.fought && !info.forfeit) { heading = 0x4A8; sound = 0x18E; } // "RETURNING TO WAR ROOMS"
        else if (info.forfeit || !info.survivors) { heading = 0x4AA; sound = 0x190; } // "DEFEAT"
        else { heading = 0x4A9; sound = 0x18F; }                       // "VICTORY!"
        Add(res.String(heading), centre, TextCentreX, colours.sectorName);
        y += lineHeight + 16;
        if (info.fought)
        {
            lines.push_back({ res.String(info.survivors ? 0x4AB : 0x4AC), left, y, 0, colours.detail, true });
            y += lineHeight * 3;
            auto section = [&](int title, int units, int vehicles, int facilities)
            {
                Add(res.String(title), centre, TextCentreX, colours.heading);
                y += lineHeight + 16;
                char text[32];
                std::snprintf(text, sizeof(text), "%d (%d)", units, vehicles);
                AddPair(0x4A6, text);
                y += lineHeight;
                std::snprintf(text, sizeof(text), "%d", facilities);
                AddPair(0x4A7, text);
                y += lineHeight * 2;
            };
            section(0x4AD, info.lostUnits, info.lostVehicles, info.lostFacilities);       // "Forces lost:"
            section(0x4AE, info.killedUnits, info.killedVehicles, info.killedFacilities); // "Enemy forces destroyed:"
        }
        SetMap(info.sector);
    }

    void Debriefing::Update()
    {
        if (leaveIn > 0 && --leaveIn == 0) { sent = Command{ 0x99, 0 }; return; }
        // 0x49B830: the 0x8017 is posted, "Debriefing Delay" on, by a draw after the first, not as the screen is
        // made: the count starts an update later
        if (!drawn) { drawn = true; return; }
        if (!done && --ticksLeft <= 0) { done = true; sent = Command{ 0x8017, 0 }; }
    }

    void Debriefing::Leave()
    {
        if (done) return;
        done = true;
        leaveIn = 5;
    }

    std::optional<Command> Debriefing::OnKey(int) { Leave(); return std::nullopt; }
    std::optional<Command> Debriefing::OnMouseButton(int, int, bool down) { if (down) Leave(); return std::nullopt; }
    std::optional<Command> Debriefing::OnRightButton(int, int, bool down) { if (down) Leave(); return std::nullopt; }
}
