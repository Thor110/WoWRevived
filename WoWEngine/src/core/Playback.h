// The game's playback recordings (Win95DeskTop, 0x42D2D0 / 0x42D520): a self-running demo made of recorded input.
// Every input message the window gets goes through Record, which, while recording, appends it to "Playback.1" as
// four u32s: the frames since the recording began (the game's frame counter, dword_4D2558), the message, wParam and
// lParam. While playing back, Pump (once a frame) opens "playback.N" (N from 1 up to [Demo] "Playback Files", then
// 1 again, a file each time playback is started) and hands back each message whose frame has come; real input is
// dropped meanwhile; at the file's end playback stops. The messages are the window's own: 0x100 / 0x101 a key down
// / up (wParam the virtual key), 0x102 a character, 0x200 the mouse moved, 0x201 / 0x202 and 0x204 / 0x205 the
// left and right buttons (lParam x | y << 16).
// In the retail game neither is ever switched on (PLAN.md 4.6); the engine's switches are --record and --playback.
#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace wow
{
    class Playback
    {
    public:
        struct Message { uint32_t msg = 0, wParam = 0, lParam = 0; };

        ~Playback() { Close(); }
        // where "Playback.1" is written (the game's folder, its working directory), and how a "playback.N" is found
        void SetFiles(std::filesystem::path folder, std::function<std::optional<std::filesystem::path>(const std::string&)> find, int count)
        {
            root = std::move(folder);
            resolve = std::move(find);
            files = count < 1 ? 1 : count;
        }
        void SetRecording(bool on) { recording = on; }      // byte_4EA3F4
        void SetPlaying(bool on) { playing = on; }          // byte_4EA3E4 (0x808F to the world sets it)
        bool Recording() const { return recording; }
        bool Playing() const { return playing; }

        // 0x42D520. False while playing back: the message isn't to be acted on. Otherwise it is, and while recording
        // it's written first (the file opened at the first one, which is where its frames count from).
        bool Record(uint32_t frame, const Message& m)
        {
            if (playing) return false;
            if (recording && !file)
            {
                file = std::fopen((root / "Playback.1").string().c_str(), "wb");
                start = frame;
            }
            else if (!recording) Close();
            if (file)
            {
                const uint32_t record[4] = { frame - start, m.msg, m.wParam, m.lParam };
                std::fwrite(record, 4, 4, file);
                std::fflush(file);
            }
            return true;
        }

        // 0x42D2D0, once a frame: the messages due
        std::vector<Message> Pump(uint32_t frame)
        {
            std::vector<Message> due;
            if (!playing) { if (file && !recording) Close(); return due; }
            if (!file)
            {
                const std::string name = "playback." + std::to_string(next);
                if (++next > files) next = 1;
                std::optional<std::filesystem::path> path = resolve ? resolve(name) : std::optional<std::filesystem::path>(root / name);
                file = path ? std::fopen(path->string().c_str(), "rb") : nullptr;
                start = frame;
                pending = false;
                if (!file) { playing = false; return due; }     // (the game goes on asking each frame; nothing comes)
            }
            const uint32_t elapsed = frame - start;
            for (;;)
            {
                if (!pending)
                {
                    uint32_t record[4];
                    if (std::fread(record, 4, 4, file) != 4) { playing = false; Close(); break; }   // the end: off
                    tick = record[0];
                    held = { record[1], record[2], record[3] };
                    pending = true;
                }
                if (tick > elapsed) break;
                due.push_back(held);
                pending = false;
            }
            return due;
        }

    private:
        void Close() { if (file) std::fclose(file); file = nullptr; }

        std::filesystem::path root;
        std::function<std::optional<std::filesystem::path>(const std::string&)> resolve;
        std::FILE* file = nullptr;
        bool recording = false, playing = false, pending = false;
        int files = 4, next = 1;        // dword_4BAAD8, dword_4BAAD4
        uint32_t start = 0, tick = 0;   // dword_4EA3EC, dword_4EA414
        Message held;                   // dword_4EA418, dword_4EA378, dword_4EA34C
    };
}
