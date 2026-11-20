#include "frontend/MovieScreen.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

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
        if (movie && movie->SeparateAudio())
            SDL_Log("Movie: finished at %.2f s; %d frames decoded, %d shown, slowest %.1f ms; the sound ran dry on %d updates",
                clock, decoded, shownCount, slowestDecode * 1000.0, underruns);
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
        if (movie->SeparateAudio()) { UpdateSeparate(); return; }
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

    // A movie with its sound on its own (the .mp4s): the sound is kept a second ahead whatever the pictures do, so
    // it never runs dry while a big frame decodes; the pictures follow the clock, skipping any whose time has gone.
    void MovieScreen::UpdateSeparate()
    {
        if (audio && audio->Available() && !audioEnded && clock > 0.1 && audio->Queued() <= 0.0) underruns++;
        while (audio && audio->Available() && !audioEnded && audio->Queued() < SoundAhead)
        {
            sound.clear();
            bool more = false;
            try { more = movie->ReadAudio(sound, 0.25); }
            catch (const std::exception& e) { SDL_Log("Movie: %s", e.what()); }
            audio->Push(sound);
            if (!more) audioEnded = true;
        }
        for (int n = 0; n < MostFramesAnUpdate && !ended && (queue.empty() || queue.back().time <= clock); n++)
        {
            bool more = false;
            uint64_t start = SDL_GetTicksNS();
            try { more = movie->Next(sound); }
            catch (const std::exception& e) { SDL_Log("Movie: %s", e.what()); }
            slowestDecode = std::max(slowestDecode, double(SDL_GetTicksNS() - start) / 1e9);
            if (!more) { ended = true; break; }
            decoded++;
            // a frame already behind the clock is only worth keeping as the latest
            if (!queue.empty() && queue.back().time <= clock) queue.pop_back();
            queue.push_back({ movie->Time(), movie->Frame() });
        }
        while (!queue.empty() && queue.front().time <= clock)
        {
            shown = std::move(queue.front().pixels);
            queue.pop_front();
            shownCount++;
        }
        // done when the pictures are, and the sound has played out (or there's none)
        bool soundDone = !audio || !audio->Available() || (audioEnded && audio->Queued() <= 0.0);
        if (ended && queue.empty() && soundDone) Finish();
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
