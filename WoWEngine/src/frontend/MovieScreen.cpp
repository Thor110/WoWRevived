#include "frontend/MovieScreen.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>

#include <algorithm>

namespace wow
{
    MovieScreen::MovieScreen(Resources& r, std::unique_ptr<Movie> m, Command after, float volume)
        : Screen(Id, r, 0, 0), movie(std::move(m)), next(after)
    {
        if (movie && movie->AudioRate() > 0)
        {
            audio = std::make_unique<MovieAudio>(movie->AudioRate(), movie->AudioChannels());
            audio->SetVolume(volume);
        }
        if (movie) shownWidth = movie->Width(), shownHeight = movie->Height();
    }

    void MovieScreen::Finish()
    {
        if (finished) return;
        finished = true;
        audio.reset();
        sent = next;
    }

    void MovieScreen::Update()
    {
        if (finished || !movie) { Finish(); return; }
        // The clock moves on a game update (a 30th of a second) at a time, and catches up with the sound if the
        // sound has got ahead, so the picture keeps time with it.
        clock += 1.0 / 30.0;
        if (audio && audio->Available()) clock = std::max(clock, audio->Played());
        // keep the queue topped up
        auto wanted = [&]() {
            if (ended || queue.size() >= 64) return false;
            if (audio && audio->Available()) return audio->Queued() < Ahead || queue.empty();
            return queue.size() < Most;
        };
        while (wanted())
        {
            sound.clear();
            bool more = false;
            try { more = movie->Next(sound); }
            catch (const std::exception& e) { SDL_Log("Movie: %s", e.what()); }
            if (!more) { ended = true; break; }
            if (audio) audio->Push(sound);
            queue.push_back({ movie->Time(), movie->Frame() });
        }
        // show the latest frame whose time has come
        while (!queue.empty() && queue.front().time <= clock)
        {
            shown = std::move(queue.front().pixels);
            queue.pop_front();
        }
        if (ended && queue.empty()) Finish();
    }

    void MovieScreen::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        if (shown.empty() || shownWidth <= 0 || shownHeight <= 0) return;
        // the game's own movies are 640 wide and drawn as they are; a larger picture (WoWRevived's .mp4s) is
        // scaled down to fit, keeping its shape
        double scale = std::min({ 1.0, double(canvas.Width()) / shownWidth, double(canvas.Height()) / shownHeight });
        int w = std::max(1, int(shownWidth * scale)), h = std::max(1, int(shownHeight * scale));
        int x0 = (canvas.Width() - w) / 2, y0 = (canvas.Height() - h) / 2;
        uint32_t* out = canvas.Pixels();
        for (int y = 0; y < h; y++)
        {
            int sy = std::min(shownHeight - 1, int(y / scale));
            const uint32_t* in = shown.data() + size_t(sy) * size_t(shownWidth);
            uint32_t* row = out + size_t(y0 + y) * size_t(canvas.Width()) + size_t(x0);
            for (int x = 0; x < w; x++) row[x] = in[std::min(shownWidth - 1, int(x / scale))];
        }
    }

    std::optional<Command> MovieScreen::OnKey(int sdlKey)
    {
        if (sdlKey == SDLK_ESCAPE) Finish();
        return std::nullopt;
    }
}
