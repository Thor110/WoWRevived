#include "frontend/CdPlayer.h"

#include <algorithm>
#include <cstdio>

namespace wow
{
    namespace
    {
        constexpr int CmdTrack = 0x803F;
        constexpr int CmdTransport = 0x80D3;
        enum Transport { PrevTrack = 0, Play = 1, Stop = 2, NextTrack = 3, Pause = 5 };
        constexpr int TrackText[6] = { 0x2F9, 0x2F9, 0x4C5, 0x4C6, 0x4C7, 0x4C8 }; // vanilla 0x4B8B38
        constexpr int ButtonFont = 0x73, CreditsFont = 0x72;
    }

    CdPlayer::CdPlayer(Resources& r, MenuColours c, int previous, Music& m, const GameFiles& f)
        : Screen(Id, r, Backdrop, PaletteGroup), colours(c), music(m), files(f)
    {
        SetPrevious(previous);
        int y = 35;
        Button heading;
        heading.plate = 0xA5; heading.glass = 0xA6; heading.font = ButtonFont; heading.level = colours.heading;
        heading.text = res.String(0x4B6); heading.focusable = false; heading.rect = { 33, y, 0, 0 };
        y += buttons[Place(heading)].rect.h;

        for (int t = FirstTrack; t <= LastTrack; t++)
        {
            Button b;
            b.plate = 0xA4; b.glass = 0xA7; b.font = ButtonFont; b.level = colours.staticText;
            b.text = res.String(0x4BE + (t - FirstTrack)); b.command = { CmdTrack, t }; b.rect = { 33, y, 0, 0 };
            y += buttons[Place(b)].rect.h;
        }
        Button back;
        back.plate = 0xA4; back.glass = 0xA8; back.font = ButtonFont; back.level = colours.staticText;
        back.text = res.String(0x40F); back.command = { previous, 0 }; back.rect = { 33, y, 0, 0 };
        y += buttons[Place(back)].rect.h + 0x14;
        status = SpriteRect(0xAA, 47, y);

        auto transport = [&](int sprite, int param, int x) {
            Button b;
            b.kind = Button::Kind::Graphic; b.plate = sprite; b.command = { CmdTransport, param }; b.rect = { x, 291, 0, 0 };
            return Place(b);
        };
        size_t previousButton = transport(0xA1, PrevTrack, 54);
        int x = buttons[previousButton].rect.w + 0x41;
        playButton = transport(0x9F, Play, x);
        stopButton = transport(0x9E, Stop, x);
        x += buttons[stopButton].rect.w + 0x0B;
        size_t pause = transport(0xA0, Pause, x);
        x += buttons[pause].rect.w + 0x0B;
        transport(0xA2, NextTrack, x);

        for (int t = FirstTrack; t <= LastTrack && !anyMusic; t++) anyMusic = TrackFile(t).has_value();
        playing = music.Playing() && !music.Finished();
        Refresh();
    }

    std::optional<std::filesystem::path> CdPlayer::TrackFile(int t) const
    {
        char name[64];
        // the same folder rule as WoWRevived's _inmm: "-Martian" whenever Human.cd is present (UseMartianAssets)
        std::snprintf(name, sizeof(name), "Music-%s\\%02d Track%02d.wav", res.Human() ? "Martian" : "Human", t, t);
        return files.Resolve(name);
    }

    void CdPlayer::Select(int t, bool play)
    {
        track = std::clamp(t, FirstTrack, LastTrack);
        if (play)
        {
            auto file = TrackFile(track);
            playing = file && music.Play(*file);
        }
        Refresh();
    }

    void CdPlayer::Refresh()
    {
        // play and stop share a place: whichever doesn't apply is hidden (0x4081D0, command 0x80A3)
        buttons[playButton].visible = !playing;
        buttons[stopButton].visible = playing;

        const std::string& text = res.String(TrackText[track]);
        size_t lineEnd = text.find('\n');
        title = text.substr(0, lineEnd);
        credits = lineEnd == std::string::npos ? std::string() : text.substr(lineEnd + 1);
    }

    bool CdPlayer::Handle(const Command& command)
    {
        if (command.id == CmdTrack) { Select(command.param, true); return true; }
        if (command.id != CmdTransport) return false;
        switch (command.param)
        {
        case PrevTrack: Select(track - 1, playing); break;
        case NextTrack: Select(track + 1, playing); break;
        case Play: Select(track, true); break;
        case Stop: music.Stop(); playing = false; Refresh(); break;
        case Pause: if (playing) music.SetPaused(!music.Paused()); break;
        default: break;
        }
        return true;
    }

    void CdPlayer::Update()
    {
        // like a CD, carry on with the next track when one finishes
        if (playing && music.Finished())
        {
            if (track < LastTrack) Select(track + 1, true);
            else { music.Stop(); playing = false; Refresh(); }
        }
    }

    void CdPlayer::Draw(Canvas& canvas) const
    {
        canvas.DrawSprite(0xA9 + track, 0, 295, 26);
        Screen::Draw(canvas);

        canvas.DrawSprite(0xAA, 0, status.x, status.y, 0, Canvas::Blend::Translucent);
        std::string line;
        if (!anyMusic) line = res.String(0x4CC);
        else
        {
            uint32_t seconds = playing ? music.PositionMs() / 1000 : 0;
            char text[32];
            std::snprintf(text, sizeof(text), "[%d]<%02u:%02u>", track - 1, seconds / 60, seconds % 60);
            line = text;
        }
        canvas.DrawString(ButtonFont, line, status, TextCentreX | TextCentreY, colours.staticText);

        canvas.DrawString(ButtonFont, title, { 320, 360, 0, 0 }, TextCentreX | TextCentreY, colours.heading, colours.lineSpacing);
        canvas.DrawString(CreditsFont, canvas.Wrap(CreditsFont, credits, 500), { 320, 416, 0, 0 }, TextCentreX | TextCentreY,
            colours.staticText, colours.lineSpacing);
    }
}
