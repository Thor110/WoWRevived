// The file formats and the game's numbers (src/core), on data made here: no game files are needed.
#include "Test.h"
#include "core/Wav.h"
#include <cstdlib>
#include "core/Bytes.h"
#include "core/EngineText.h"
#include "core/Ffuh.h"
#include "core/Palette.h"
#include "core/Random.h"
#include "core/ShadeTable.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace wow;

namespace
{
    void Put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
    void Put32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i))); }
}

// ---------------------------------------------------------------- Bytes

TEST("bytes: little-endian reads")
{
    const std::vector<uint8_t> data = { 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 'a', 'b', 0, 'x' };
    ByteReader r(data);
    CHECK_EQ(int(r.U8()), 0x12);
    CHECK_EQ(int(r.U16()), 0x1234);
    CHECK_EQ(r.U32(), 0x12345678u);
    CHECK_EQ(r.Name(4), std::string("ab"));     // a fixed field, cut at its first 0
    CHECK_EQ(r.Remaining(), size_t(0));
}

TEST("bytes: reading past the end throws")
{
    const std::vector<uint8_t> data = { 1, 2, 3 };
    ByteReader r(data);
    CHECK_THROWS(r.U32());
    ByteReader s(data);
    s.Skip(2);
    CHECK_THROWS(s.U16());
}

// ---------------------------------------------------------------- FFUH

TEST("ffuh: compress then decompress gives the data back")
{
    std::vector<uint8_t> text;
    for (int i = 0; i < 5000; i++) text.push_back(uint8_t("the war of the worlds "[i % 22]));
    std::vector<uint8_t> noise;
    random::Seed(7);
    for (int i = 0; i < 4096; i++) noise.push_back(uint8_t(random::Rand()));
    std::vector<uint8_t> two = { 1, 2, 1, 1, 2, 2, 2, 1 };
    for (const auto* data : { &text, &noise, &two })
    {
        const std::vector<uint8_t> packed = ffuh::Compress(*data);
        CHECK(ffuh::IsCompressed(packed));
        CHECK_EQ(ffuh::Decompress(packed), *data);
    }
}

TEST("ffuh: text packs smaller than it was")
{
    std::vector<uint8_t> text;
    for (int i = 0; i < 5000; i++) text.push_back(uint8_t("aaaaabbbc"[i % 9]));
    CHECK(ffuh::Compress(text).size() < text.size());
}

TEST("ffuh: data that isn't compressed comes back as it is")
{
    const std::vector<uint8_t> plain = { 'K', 'A', 'T', '!', 1, 2, 3 };
    CHECK(!ffuh::IsCompressed(plain));
    CHECK_EQ(ffuh::Decompress(plain), plain);
}

TEST("ffuh: damaged data throws")
{
    std::vector<uint8_t> text(3000);
    for (size_t i = 0; i < text.size(); i++) text[i] = uint8_t(i * 7);
    std::vector<uint8_t> packed = ffuh::Compress(text);
    packed.resize(packed.size() / 2);       // cut short
    CHECK_THROWS(ffuh::Decompress(packed));
}

// ---------------------------------------------------------------- the game's random numbers

TEST("random: MSVC rand() after srand(1), as the war map seeds it")
{
    random::Seed(1);
    const int expected[] = { 41, 18467, 6334, 26500, 19169, 15724, 11478, 29358, 26962, 24464 };
    for (int e : expected) CHECK_EQ(random::Rand(), e);
}

TEST("random: the table and Random(n) stay in 0 to n")
{
    random::Seed(1);
    random::FillTable();
    CHECK_EQ(random::GameTable().values[0], 41);
    for (int i = 0; i < 3000; i++)
    {
        const int v = random::Random(5);
        CHECK(v >= 0 && v <= 5);
    }
    CHECK_EQ(random::Random(-1), 0);        // the game divides by 0 there; the engine gives 0
}

// ---------------------------------------------------------------- sprites, palettes, shade tables

TEST("sprite: run-length rows decode, index 0 see-through")
{
    // 4 x 2, one frame: row 0 = 3 x colour 5 then 1 x colour 0; row 1 = 2 x 0, 2 x 9
    std::vector<uint8_t> s;
    Put16(s, 4); Put16(s, 2); Put16(s, 1);
    Put32(s, 10);                       // the frame, from the sprite's start
    Put32(s, 8); Put32(s, 12);          // the rows, from the frame's start
    s.insert(s.end(), { 5, 3, 0, 1 });
    s.insert(s.end(), { 0, 2, 9, 2 });
    Sprite sprite(s);
    CHECK_EQ(int(sprite.Info().width), 4);
    CHECK_EQ(sprite.FrameCount(), 1);
    const IndexedImage img = sprite.Decode();
    const std::vector<uint8_t> want = { 5, 5, 5, 0, 0, 0, 9, 9 };
    CHECK_EQ(img.pixels, want);
}

TEST("sprite: a damaged header throws")
{
    std::vector<uint8_t> s = { 4, 0, 2 };
    CHECK_THROWS(Sprite(s));
}

TEST("palette: 6-bit colours, the blend table when the file has it")
{
    std::vector<uint8_t> pal(Palette::FileSize, 0);
    pal[3] = 63; pal[4] = 32; pal[5] = 1;      // colour 1
    pal[Palette::BlendOffset + 5] = 77;
    const Palette p = Palette::Load(pal);
    CHECK_EQ(int(p.colours[1].r), 252);
    CHECK_EQ(int(p.colours[1].g), 128);
    CHECK_EQ(int(p.colours[1].b), 4);
    CHECK_EQ(p.blend.size(), size_t(65536));
    CHECK_EQ(int(p.blend[5]), 77);
    const std::vector<uint8_t> small(Palette::BlendOffset, 0);
    CHECK(Palette::Load(small).blend.empty());
}

TEST("shade table: RGB565 levels to 24-bit colour")
{
    std::vector<uint8_t> shh = { 2 };
    for (int level = 0; level < 2; level++)
        for (int i = 0; i < 256; i++) Put16(shh, level == 0 ? 0xF800 : 0x07E0);    // red, then green
    const ShadeTable t = ShadeTable::LoadShh(shh);
    CHECK_EQ(t.Levels(), 2);
    CHECK_EQ(t.Level(0)[10], 0xFF0000u);
    CHECK_EQ(t.Level(1)[10], 0x00FF00u);
    CHECK_EQ(t.Level(99)[0], 0x00FF00u);    // clamped to the last level
    CHECK_THROWS(ShadeTable::LoadShh(std::vector<uint8_t>{ 0 }));
}

// ---------------------------------------------------------------- TEXT.ojd

namespace
{
    void TextRecord(std::vector<uint8_t>& b, int type, uint16_t a, const std::string& text)
    {
        b.insert(b.end(), { 0xFF, 0, 0 });
        Put16(b, uint16_t(type));
        Put16(b, a);
        Put16(b, uint16_t(text.size() + 1));
        b.insert(b.end(), text.begin(), text.end());
        b.push_back(0);
    }
    void TextPair(std::vector<uint8_t>& b, int type, uint16_t a, uint16_t c)
    {
        b.insert(b.end(), { 0xFF, 0, 0 });
        Put16(b, uint16_t(type)); Put16(b, a); Put16(b, c);
    }
}

TEST("text: each side's strings, copies, speech and escapes")
{
    std::vector<uint8_t> t;
    TextRecord(t, 2, 3, "North Western Highlands");
    TextRecord(t, 1, 10, "Human only");
    TextRecord(t, 0, 10, "Martian only");
    TextRecord(t, 2, 11, "two\\nlines");
    TextPair(t, 3, 12, 3);           // 12 is 3's text
    TextPair(t, 4, 3, 456);          // 3's speech is sound 456
    t.push_back(0);                  // the files end with a 0
    const TextTable human = TextTable::Load(t, true), martian = TextTable::Load(t, false);
    CHECK_EQ(human.Get(10), std::string("Human only"));
    CHECK_EQ(martian.Get(10), std::string("Martian only"));
    CHECK_EQ(human.Get(11), std::string("two\nlines"));
    CHECK_EQ(human.Get(12), std::string("North Western Highlands"));
    CHECK_EQ(human.Extra(3), 456);
    CHECK(!human.Has(99));
}

TEST("text: an unknown record type throws")
{
    std::vector<uint8_t> t;
    TextPair(t, 9, 1, 1);
    CHECK_THROWS(TextTable::Load(t, true));
}

TEST("engine text: every string has all five languages, in Latin-1")
{
    for (const enginetext::Entry& e : enginetext::Table)
        for (int l = 0; l < enginetext::Languages; l++)
        {
            REQUIRE(e.text[l] != nullptr);
            CHECK(std::string(e.text[l]).size() > 0);
            CHECK(enginetext::Latin1(e.text[l]).find('?') == std::string::npos || std::string(e.text[l]).find('?') != std::string::npos);
        }
    CHECK_EQ(enginetext::Latin1("Größe"), std::string("Gr\xF6\xDF" "e"));
    CHECK(enginetext::Find(enginetext::MenuScale) != nullptr);
    CHECK(enginetext::Find(enginetext::MenuScale + enginetext::TipOffset) != nullptr);
}

namespace
{
    // a WAV: the fmt chunk, then the data
    std::vector<uint8_t> MakeWav(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits, const std::vector<uint8_t>& samples)
    {
        std::vector<uint8_t> w;
        auto tag = [&](const char* t) { w.insert(w.end(), t, t + 4); };
        auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) w.push_back(uint8_t(v >> (8 * i))); };
        auto u16 = [&](uint16_t v) { w.push_back(uint8_t(v)); w.push_back(uint8_t(v >> 8)); };
        tag("RIFF"); u32(uint32_t(4 + 8 + 16 + 8 + samples.size())); tag("WAVE");
        tag("LIST"); u32(4); tag("INFO");                      // a chunk to step over
        tag("fmt "); u32(16); u16(format); u16(channels); u32(rate); u32(rate * channels * bits / 8); u16(uint16_t(channels * bits / 8)); u16(bits);
        tag("data"); u32(uint32_t(samples.size()));
        w.insert(w.end(), samples.begin(), samples.end());
        return w;
    }
}

TEST("wav: 16-bit mono at the sample's rate is read as it is")
{
    const std::vector<uint8_t> s = { 0x00, 0x10, 0x00, 0xF0, 0xFF, 0x7F };   // 4096, -4096, 32767
    std::string error;
    auto pcm = DecodeWav(MakeWav(1, 1, 22050, 16, s), 22050, error);
    REQUIRE(pcm.has_value());
    REQUIRE(pcm->size() == size_t(3));
    CHECK_EQ(int((*pcm)[0]), 4096);
    CHECK_EQ(int((*pcm)[1]), -4096);
    CHECK_EQ(int((*pcm)[2]), 32767);
}

TEST("wav: stereo mixed down, 44100 made 22050, 8-bit read; a file that isn't one says why")
{
    // two frames of stereo 16-bit: (1000, 3000) and (-1000, -3000) at 44100 -> one frame of 2000 at 22050
    const std::vector<uint8_t> s = { 0xE8, 0x03, 0xB8, 0x0B, 0x18, 0xFC, 0x48, 0xF4 };
    std::string error;
    auto pcm = DecodeWav(MakeWav(1, 2, 44100, 16, s), 22050, error);
    REQUIRE(pcm.has_value());
    REQUIRE(pcm->size() == size_t(1));
    CHECK(std::abs(int((*pcm)[0]) - 2000) <= 1);
    // 8-bit is unsigned: 128 is silence, 255 nearly the top
    pcm = DecodeWav(MakeWav(1, 1, 22050, 8, { 128, 255 }), 22050, error);
    REQUIRE(pcm.has_value());
    CHECK_EQ(int((*pcm)[0]), 0);
    CHECK((*pcm)[1] > 32000);
    CHECK(!DecodeWav(std::vector<uint8_t>{ 'R', 'I', 'F', 'F' }, 22050, error).has_value());
    CHECK(!error.empty());
    CHECK(!DecodeWav(MakeWav(2, 1, 22050, 4, { 1, 2 }), 22050, error).has_value());   // ADPCM isn't read
}

TEST("wav: a stereo sound keeps its two channels, and a mono file is doubled for one")
{
    // one stereo frame (1000, -1000) at the same rate
    std::string error;
    auto pcm = DecodeWav(MakeWav(1, 2, 22050, 16, { 0xE8, 0x03, 0x18, 0xFC }), 22050, error, 2);
    REQUIRE(pcm.has_value());
    REQUIRE(pcm->size() == size_t(2));
    CHECK_EQ(int((*pcm)[0]), 1000);
    CHECK_EQ(int((*pcm)[1]), -1000);
    pcm = DecodeWav(MakeWav(1, 1, 22050, 16, { 0xE8, 0x03 }), 22050, error, 2);
    REQUIRE(pcm.has_value());
    REQUIRE(pcm->size() == size_t(2));
    CHECK_EQ(int((*pcm)[0]), 1000);
    CHECK_EQ(int((*pcm)[1]), 1000);
}
