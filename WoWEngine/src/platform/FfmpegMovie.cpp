// MODERN (update 249, docs/MODERN.md "The movies"): WoWRevived's .mp4 movies on Linux and macOS, through the ffmpeg
// program where it's installed (Windows plays them through Media Foundation, Mp4Movie.cpp). The engine asks ffprobe
// first whether the file can be read (its picture's size and frame rate, its sound's rate and channels); if ffprobe
// or ffmpeg isn't there, or can't read it, the .smk is played as before. The picture comes from one ffmpeg process
// (decoded and scaled to the size shown, BGRA, a frame at a time down a pipe) and the sound from another (16-bit,
// interleaved), so the sound is read on its own, as Media Foundation's is. Nothing of FFmpeg is linked or shipped.
#include "platform/Movie.h"

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace wow
{
    namespace
    {
        // a program's path: on the PATH, else where Homebrew and the like put them (a Mac app started from the Finder
        // has only /usr/bin:/bin:/usr/sbin:/sbin as its PATH, without /opt/homebrew/bin or /usr/local/bin)
        std::string Tool(const char* name)
        {
            std::string dirs = std::getenv("PATH") ? std::getenv("PATH") : "";
            dirs += ":/opt/homebrew/bin:/usr/local/bin:/opt/local/bin:/usr/bin";
            size_t start = 0;
            while (start <= dirs.size())
            {
                size_t end = dirs.find(':', start);
                if (end == std::string::npos) end = dirs.size();
                const std::string dir = dirs.substr(start, end - start);
                start = end + 1;
                if (dir.empty()) continue;
                const std::string full = dir + "/" + name;
                if (access(full.c_str(), X_OK) == 0) return full;
            }
            return "";
        }

        // a program run with its standard output to a pipe (no shell: the arguments as they are)
        class Process
        {
        public:
            bool Start(std::vector<std::string> args)
            {
                if (args.empty()) return false;
                args[0] = Tool(args[0].c_str());
                if (args[0].empty()) return false;
                int fds[2];
                if (pipe(fds) != 0) return false;
                posix_spawn_file_actions_t actions;
                posix_spawn_file_actions_init(&actions);
                posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
                posix_spawn_file_actions_addclose(&actions, fds[0]);
                posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
                std::vector<char*> argv;
                for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
                argv.push_back(nullptr);
                const int r = posix_spawn(&pid, argv[0], &actions, nullptr, argv.data(), environ);
                posix_spawn_file_actions_destroy(&actions);
                close(fds[1]);
                if (r != 0) { close(fds[0]); pid = -1; return false; }
                fd = fds[0];
                return true;
            }
            // up to n bytes; fewer only at the end
            size_t Read(uint8_t* to, size_t n)
            {
                size_t got = 0;
                while (got < n && fd >= 0)
                {
                    const ssize_t r = read(fd, to + got, n - got);
                    if (r < 0 && errno == EINTR) continue;
                    if (r <= 0) break;
                    got += size_t(r);
                }
                return got;
            }
            std::string ReadAll()
            {
                std::string out;
                uint8_t buffer[4096];
                for (size_t n; (n = Read(buffer, sizeof(buffer))) > 0;) out.append(reinterpret_cast<char*>(buffer), n);
                return out;
            }
            int Finish()
            {
                if (fd >= 0) { close(fd); fd = -1; }
                if (pid <= 0) return -1;
                int status = 0;
                waitpid(pid, &status, 0);
                pid = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            void Stop()
            {
                if (pid > 0) kill(pid, SIGTERM);
                Finish();
            }
            ~Process() { Stop(); }

        private:
            pid_t pid = -1;
            int fd = -1;
        };

        // ffprobe's answer for one stream: its fields, comma-separated, in the order asked
        std::vector<std::string> Probe(const std::string& path, const char* stream, const char* fields)
        {
            Process p;
            if (!p.Start({ "ffprobe", "-v", "error", "-select_streams", stream, "-show_entries", std::string("stream=") + fields, "-of", "csv=p=0", path })) return {};
            std::string text = p.ReadAll();
            if (p.Finish() != 0) return {};
            const size_t end = text.find('\n');
            if (end != std::string::npos) text.erase(end);
            std::vector<std::string> out;
            size_t start = 0;
            for (size_t comma; (comma = text.find(',', start)) != std::string::npos; start = comma + 1) out.push_back(text.substr(start, comma - start));
            if (!text.empty()) out.push_back(text.substr(start));
            return out;
        }

        class FfmpegMovie final : public Movie
        {
        public:
            bool Open(const std::filesystem::path& file, std::string& error)
            {
                path = file.string();
                const auto video = Probe(path, "v:0", "width,height,r_frame_rate");
                if (video.size() < 3) { error = "ffprobe isn't installed or can't read it"; return false; }
                const int sourceW = std::atoi(video[0].c_str()), sourceH = std::atoi(video[1].c_str());
                const size_t slash = video[2].find('/');
                const double num = std::atof(video[2].c_str()), den = slash == std::string::npos ? 1.0 : std::atof(video[2].c_str() + slash + 1);
                fps = den > 0 ? num / den : 0;
                if (sourceW <= 0 || sourceH <= 0 || fps <= 0) { error = "no picture ffprobe can read"; return false; }
                // shown at its own size up to 1920 x 1080, its shape kept (as Mp4Movie), even sizes for the scaler
                const double scale = std::min({ 1.0, 1920.0 / sourceW, 1080.0 / sourceH });
                width = std::max(2, int(sourceW * scale) & ~1);
                height = std::max(2, int(sourceH * scale) & ~1);
                const auto audio = Probe(path, "a:0", "sample_rate,channels");
                if (audio.size() >= 2)
                {
                    rate = std::atoi(audio[0].c_str());
                    channels = std::clamp(std::atoi(audio[1].c_str()), 1, 2);
                    if (rate <= 0) rate = channels = 0;
                }
                const std::string size = std::to_string(width) + "x" + std::to_string(height);
                if (!pictures.Start({ "ffmpeg", "-v", "error", "-nostdin", "-i", path, "-an", "-f", "rawvideo", "-pix_fmt", "bgra", "-s", size, "pipe:1" }))
                {
                    error = "ffmpeg isn't installed";
                    return false;
                }
                if (rate > 0 && !sound.Start({ "ffmpeg", "-v", "error", "-nostdin", "-i", path, "-vn", "-f", "s16le", "-ac", std::to_string(channels), "-ar", std::to_string(rate), "pipe:1" }))
                    rate = channels = 0;
                raw.resize(size_t(width) * size_t(height) * 4);
                frame.assign(size_t(width) * size_t(height), 0);
                SDL_Log("Movie: %dx%d at %.3f frames a second (shown %dx%d), sound %d Hz x %d, through ffmpeg", sourceW, sourceH, fps, width, height, rate, channels);
                return true;
            }

            int Width() const override { return width; }
            int Height() const override { return height; }
            int AudioRate() const override { return rate; }
            int AudioChannels() const override { return channels; }
            double Time() const override { return fps > 0 ? double(index) / fps : 0; }
            const std::vector<uint32_t>& Frame() const override { return frame; }
            bool SeparateAudio() const override { return true; }

            bool Next(std::vector<int16_t>&) override
            {
                if (done) return false;
                if (started) index++;
                started = true;
                if (pictures.Read(raw.data(), raw.size()) != raw.size()) { done = true; return false; }
                for (size_t i = 0; i < frame.size(); i++)
                    frame[i] = uint32_t(raw[i * 4 + 2]) << 16 | uint32_t(raw[i * 4 + 1]) << 8 | raw[i * 4];
                return true;
            }

            bool ReadAudio(std::vector<int16_t>& audio, double seconds) override
            {
                if (rate <= 0 || soundDone) return false;
                const size_t want = size_t(std::max(1.0, std::ceil(seconds * rate))) * size_t(channels);
                std::vector<uint8_t> bytes(want * 2);
                const size_t got = sound.Read(bytes.data(), bytes.size()) / 2;
                for (size_t i = 0; i < got; i++) audio.push_back(int16_t(bytes[i * 2] | bytes[i * 2 + 1] << 8));
                if (got < want) soundDone = true;
                return got > 0;
            }

        private:
            std::string path;
            Process pictures, sound;
            int width = 0, height = 0, rate = 0, channels = 0;
            double fps = 0;
            long index = 0;
            bool started = false, done = false, soundDone = false;
            std::vector<uint8_t> raw;
            std::vector<uint32_t> frame;
        };
    }

    std::unique_ptr<Movie> OpenMp4(const std::filesystem::path& path, std::string& error)
    {
        auto movie = std::make_unique<FfmpegMovie>();
        if (!movie->Open(path, error)) return nullptr;
        return movie;
    }
}
