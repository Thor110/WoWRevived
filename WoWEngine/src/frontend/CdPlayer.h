// The CD Player (menu 0x803E): constructor 0x407990, message handler 0x4081D0, status 0x4088B0.
// Opened from the main menu with backdrop GType 0xAF (CD_SEP1.spr) and palette group 0x2F9 ("cd").
//
// Layout (from the constructor):
//   heading       plate 0xA5, glass 0xA6, string 0x4B6, font 0x73, at (33, 35)
//   track buttons plate 0xA4, glass 0xA7, strings 0x4BE-0x4C1, command 0x803F with the track (2-5), stacked below
//   back button   plate 0xA4, glass 0xA8, string 0x40F, sends the previous menu's id
//   status        sprite 0xAA at (47, 20 below the back button), "[track - 1]<mm:ss>", or 0x4CC with no music
//   transport     command 0x80D3 at y 291: previous 0xA1 (0) at x 54, then play 0x9F (1) and stop 0x9E (2)
//                 sharing x = width + 65, pause 0xA0 (5) and next 0xA2 (3) each 11 past the one before
//   picture       GType 0xA9 + track at (295, 26)
//   title / text  string from the table at 0x4B8B38 (tracks 2-5: 0x4C5-0x4C8): its first line centred at
//                 (320, 360) in font 0x73, the rest wrapped to 500 pixels in font 0x72 centred at (320, 416)
#pragma once
#include "core/GameFiles.h"
#include "frontend/Menu.h"
#include "platform/Music.h"

namespace wow
{
    class CdPlayer : public Screen
    {
    public:
        static constexpr int Id = 0x803E;
        static constexpr int Backdrop = 0xAF;
        static constexpr int PaletteGroup = 0x2F9;

        CdPlayer(Resources& res, MenuColours colours, int previous, Music& music, const GameFiles& files);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        bool Handle(const Command& command) override;

    private:
        static constexpr int FirstTrack = 2, LastTrack = 5;

        std::optional<std::filesystem::path> TrackFile(int track) const;
        void Select(int track, bool play);
        void Refresh();

        MenuColours colours;
        Music& music;
        const GameFiles& files;
        bool anyMusic = false;
        int track = FirstTrack;
        bool playing = false;
        size_t playButton = 0, stopButton = 0;
        Rect status;
        std::string title, credits;
    };
}
