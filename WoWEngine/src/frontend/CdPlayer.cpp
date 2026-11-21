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

        // each with its tool tip (0x407E33-0x408003): 0x4BC Previous Track, 0x4B9 Play, 0x4BA Stop, 0x4BD Pause,
        // 0x4BB Next Track
        auto transport = [&](int sprite, int param, int x, int tooltip) {
            Button b;
            b.kind = Button::Kind::Graphic; b.plate = sprite; b.command = { CmdTransport, param }; b.rect = { x, 291, 0, 0 };
            b.tooltip = tooltip;
            return Place(b);
        };
        size_t previousButton = transport(0xA1, PrevTrack, 54, 0x4BC);
        int x = buttons[previousButton].rect.w + 0x41;
        playButton = transport(0x9F, Play, x, 0x4B9);
        stopButton = transport(0x9E, Stop, x, 0x4BA);
        x += buttons[stopButton].rect.w + 0x0B;
        size_t pause = transport(0xA0, Pause, x, 0x4BD);
        x += buttons[pause].rect.w + 0x0B;
        transport(0xA2, NextTrack, x, 0x4BB);

        for (int t = FirstTrack; t <= LastTrack && !anyMusic; t++) anyMusic = TrackFile(t).has_value();
        playing = music.Playing() && !music.Finished();
        Refresh();
    }

    std::optional<std::filesystem::path> CdPlayer::TrackFile(int t) const
    {
        char name[64];
        // the side's own disc's music: Human.cd means the Human disc is in (0x408B70 checks for Human.cd when the
        // side flag 0x4B84C4 is set, Martian.cd otherwise), so Music-Human
        std::snprintf(name, sizeof(name), "Music-%s\\%02d Track%02d.wav", res.Human() ? "Human" : "Martian", t, t);
        return files.Resolve(name);
    }

    void CdPlayer::Select(int t, bool play)
    {
        track = std::clamp(t, FirstTrack, LastTrack);
        if (!play && music.Paused()) music.Stop(); // a paused track doesn't carry over to another
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
        // Next / Prev (0x413220 / 0x4134B0, from 0x4081D0) wrap round the ends: past the last, the first music track
        // (2); before the first, the last; and a wrap stops play (+0xC4 0, 0x4083BF / 0x408351)
        case PrevTrack:
        case NextTrack:
        {
            const int to = command.param == NextTrack ? track + 1 : track - 1;
            if (to > LastTrack || to < FirstTrack)
            {
                music.Stop();
                music.SetPaused(false);
                playing = false;
                Select(to > LastTrack ? FirstTrack : LastTrack, false);
            }
            else Select(to, playing);
            break;
        }
        // Play (0x408328) resumes a paused track, otherwise starts it; Pause (0x40833B) pauses the CD and counts
        // as not playing, so Play comes back in Stop's place
        case Play:
            if (music.Paused()) { music.SetPaused(false); playing = true; Refresh(); }
            else Select(track, true);
            break;
        case Stop: music.Stop(); music.SetPaused(false); playing = false; Refresh(); break;
        case Pause: if (playing) { music.SetPaused(true); playing = false; Refresh(); } break;
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
            uint32_t seconds = playing || music.Paused() ? music.PositionMs() / 1000 : 0;
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
