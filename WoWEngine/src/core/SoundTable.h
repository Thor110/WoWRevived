// SFX.ojd, the sound table (loader 0x463D10), and the sound libraries its samples come from.
//
// SFX.ojd is a list of records: 0xFF, a u16 line number, a u16 opcode, then the opcode's arguments. Sounds are
// numbered; opcode 1 starts one and the opcodes after it describe it, until the next 1. The game keeps them in
// a table of 0x70 byte entries at 0x58FE88.
//   0  u16 type        sets the type for the sounds that follow (0-5, see Type) and resets the volume to 1.0
//   1  u16 id          starts sound id
//   2  u16 n, name     adds a sample (n bytes, null terminated, including the null)
//   3  u16 n, name     ... only in Martian games
//   4  u16 n, name     ... only in Human games
//   5  f32             multiplies the sound's volume
//   6                  flag at +0x6C (used with type 5)
//   7                  loops: the sound plays until it's stopped
//   8  s16             +0x58
//   9  s16             +0x5C
//   10 s16             +0x64
//   11 s16 rate        sample rate for this sound's samples: 11 = 11025, 22 = 22050, else as given (default 22050)
//   13 s16 id          copies another sound's entry over this one
//   14 f32             the volume for the sounds that follow
//   15                 calls 0x466C30(1) on the sound
//   16                 ignored ("Ignoring constant")
//   17 u16 group       the samples that follow go in variation group (group - 1), up to 3 groups of 5
//   18                 positional: only plays when its map position is on screen (0x464EE0)
// Samples are looked up by name in SFX\SFX.WoW, or for type 5 (speech and music) in the side's VOX\HUMAN.WoW
// or VOX\MARTIAN.WoW. They're 16-bit mono PCM.
#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct SoundSample
    {
        std::string name;
        bool vox = false;   // from the side's VOX library rather than SFX\SFX.WoW
        int rate = 22050;
    };

    struct SoundDef
    {
        // The type picks the [Sound\Volume] setting that scales the sound (the array at 0x4CA858).
        enum Type { Ambient = 0, Engine = 1, Explosions = 2, Weapons = 3, Special = 4, Speech = 5 };

        int id = 0;
        int type = 0;
        float volume = 1.0f;
        int priority = 0xC0;     // 0x80 for speech
        bool loop = false;       // opcode 7
        bool positional = false; // opcode 18
        bool flag6 = false, flag15 = false;
        int field58 = 0, field5C = 0, field64 = 0;
        std::vector<SoundSample> groups[3];
    };

    class SoundTable
    {
    public:
        // Throws FormatError on a malformed file.
        static SoundTable Load(std::span<const uint8_t> data, bool human);

        const SoundDef* Find(int id) const;
        const std::map<int, SoundDef>& Sounds() const { return sounds; }

        static const char* TypeName(int type);

    private:
        std::map<int, SoundDef> sounds;
    };
}

#include "core/GameFiles.h"
#include <memory>
#include <optional>

namespace wow
{
    class Archive;

    // The three sound libraries (0x466750): SFX\SFX.WoW, VOX\HUMAN.WoW and VOX\MARTIAN.WoW. A sample is looked
    // up by name in that order, whichever folder SFX.ojd names, after a loose "SFX\<name>.snd" (or VOX\) that
    // replaces it (0x4660A0). Missing libraries are skipped.
    class SoundLibrary
    {
    public:
        explicit SoundLibrary(const GameFiles& files);
        ~SoundLibrary();

        // 16-bit mono PCM, or nothing if no library has it.
        std::optional<std::vector<int16_t>> Load(const SoundSample& sample);

    private:
        const GameFiles& files;
        std::unique_ptr<Archive> libraries[3];
    };
}
