// WoWRevived's .mp4 movies through Media Foundation's source reader (Windows only). The picture comes as NV12 (the
// decoder's own output, turned into RGB here only at the size shown) or, failing that, as 32-bit RGB; the sound as
// 16-bit PCM from a second reader of its own, so it never waits on the pictures.
//
// The decoders settle their output format only once they've seen the stream: the H.264 decoder pads the frame and
// sets its stride, and the AAC decoder may decide a 22050 Hz track is HE-AAC and give 44100 Hz. They say so with
// MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED on a sample, so the first sample of each is read while opening and the
// formats are taken after it (and again whenever they change). Everything is logged to help with files that don't
// play.
#include "platform/Movie.h"
#include "platform/Nv12.h"

// windows.h's min and max macros would swallow std::min and std::max
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cstring>

namespace wow
{
    namespace
    {
        template <typename T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

        class Mp4Movie final : public Movie
        {
        public:
            ~Mp4Movie() override
            {
                SDL_Log("Movie: %d frames read, %d with no picture", framesRead, framesFailed);
                Release(pending);
                Release(reader);
                Release(audioReader);
                if (started) MFShutdown();
                if (comStarted) CoUninitialize();
            }

            bool Open(const std::filesystem::path& path, std::string& error)
            {
                HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                comStarted = SUCCEEDED(hr);
                if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) { error = "Media Foundation isn't available"; return false; }
                started = true;

                // Two readers on the file: one for the picture and one for the sound, so the sound can be kept well
                // ahead of what's heard however long the pictures take to decode
                reader = OpenReader(path, true);
                if (!reader) { error = "can't open it"; return false; }
                reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
                reader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

                IMFMediaType* type = nullptr;
                for (const GUID& subtype : { MFVideoFormat_NV12, MFVideoFormat_RGB32 })
                {
                    MFCreateMediaType(&type);
                    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                    type->SetGUID(MF_MT_SUBTYPE, subtype);
                    hr = reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type);
                    Release(type);
                    if (SUCCEEDED(hr)) { nv12 = subtype == MFVideoFormat_NV12; break; }
                    SDL_Log("Movie: the decoder won't give %s (0x%08lX)", subtype == MFVideoFormat_NV12 ? "NV12" : "RGB32", static_cast<unsigned long>(hr));
                }
                if (FAILED(hr)) { error = "its video can't be decoded"; return false; }
                if (!ReadVideoType()) { error = "no video"; return false; }

                // the first frame, which settles the decoder's format
                if (!ReadFrameSample(pending, pendingTime)) { error = "no video frames"; return false; }
                // shown at most 640 x 480 (the game's screen), keeping its shape
                double scale = (std::min)({ 1.0, 640.0 / sourceWidth, 480.0 / sourceHeight });
                width = (std::max)(1, int(sourceWidth * scale));
                height = (std::max)(1, int(sourceHeight * scale));
                frame.assign(size_t(width) * size_t(height), 0);
                SDL_Log("Movie: picture %dx%d shown at %dx%d", sourceWidth, sourceHeight, width, height);

                // the sound as 16-bit PCM, if there is any
                audioReader = OpenReader(path, false);
                if (audioReader)
                {
                    audioReader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
                    hr = audioReader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
                    MFCreateMediaType(&type);
                    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                    type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                    if (SUCCEEDED(hr)) hr = audioReader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type);
                    Release(type);
                    if (FAILED(hr)) SDL_Log("Movie: no sound (0x%08lX)", static_cast<unsigned long>(hr));
                    else if (ReadAudioType())
                    {
                        // the first sound, which settles the decoder's format (the sample rate can change here)
                        firstAudio = true;
                        ReadAudioSamples(pendingAudio, 0.0);
                        firstAudio = false;
                    }
                }
                if (rate <= 0 || channels <= 0)
                {
                    rate = channels = 0;
                    audioDone = true;
                    Release(audioReader);
                }
                SDL_Log("Movie: sound %d Hz, %d channels", rate, channels);
                return true;
            }

            int Width() const override { return width; }
            int Height() const override { return height; }
            int AudioRate() const override { return rate; }
            int AudioChannels() const override { return channels; }
            double Time() const override { return time; }
            const std::vector<uint32_t>& Frame() const override { return frame; }
            bool SeparateAudio() const override { return true; }

            bool Next(std::vector<int16_t>&) override
            {
                IMFSample* sample = nullptr;
                double stamp = 0;
                if (pending) { sample = pending; stamp = pendingTime; pending = nullptr; }
                else if (!ReadFrameSample(sample, stamp)) return false;
                time = stamp;
                framesRead++;
                if (!CopyFrame(sample)) framesFailed++;
                Release(sample);
                return true;
            }

            bool ReadAudio(std::vector<int16_t>& audio, double seconds) override
            {
                if (!pendingAudio.empty())
                {
                    audio.insert(audio.end(), pendingAudio.begin(), pendingAudio.end());
                    pendingAudio.clear();
                }
                if (!audioDone) ReadAudioSamples(audio, seconds);
                return !audioDone || !audio.empty();
            }

        private:
            static IMFSourceReader* OpenReader(const std::filesystem::path& path, bool videoProcessing)
            {
                IMFAttributes* attributes = nullptr;
                MFCreateAttributes(&attributes, 1);
                if (attributes && videoProcessing) attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
                IMFSourceReader* r = nullptr;
                HRESULT hr = MFCreateSourceReaderFromURL(path.wstring().c_str(), attributes, &r);
                Release(attributes);
                if (FAILED(hr)) SDL_Log("Movie: can't open %s (0x%08lX)", path.string().c_str(), static_cast<unsigned long>(hr));
                return SUCCEEDED(hr) ? r : nullptr;
            }

            // The picture's format as the decoder gives it now: the frame's size (padding included), its stride,
            // where the picture is in it, and the colour matrix.
            bool ReadVideoType()
            {
                IMFMediaType* current = nullptr;
                if (FAILED(reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current))) return false;
                UINT32 w = 0, h = 0;
                MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &w, &h);
                stride = int32_t(MFGetAttributeUINT32(current, MF_MT_DEFAULT_STRIDE, UINT32(nv12 ? w : w * 4)));
                UINT32 matrix = MFGetAttributeUINT32(current, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown);
                bt709 = matrix == MFVideoTransferMatrix_BT709 || (matrix != MFVideoTransferMatrix_BT601 && h >= 720);
                frameWidth = int(w);
                frameHeight = int(h);
                left = top = 0;
                int pictureW = int(w), pictureH = int(h);
                MFVideoArea area{};
                UINT32 got = 0;
                if (SUCCEEDED(current->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), &got)) &&
                    got == sizeof(area) && area.Area.cx > 0 && area.Area.cy > 0 &&
                    area.OffsetX.value + area.Area.cx <= LONG(w) && area.OffsetY.value + area.Area.cy <= LONG(h))
                {
                    left = area.OffsetX.value;
                    top = area.OffsetY.value;
                    pictureW = int(area.Area.cx);
                    pictureH = int(area.Area.cy);
                }
                Release(current);
                // the shown size is fixed once the movie is open; a later change only moves where the picture is
                if (!sourceWidth) { sourceWidth = pictureW; sourceHeight = pictureH; }
                else { sourceWidth = (std::min)(pictureW, frameWidth - int(left)); sourceHeight = (std::min)(pictureH, frameHeight - int(top)); }
                SDL_Log("Movie: video %s, frame %ux%u, stride %d, picture %dx%d at %ld,%ld, %s", nv12 ? "NV12" : "RGB32", w, h, int(stride),
                    pictureW, pictureH, static_cast<long>(left), static_cast<long>(top), bt709 ? "BT.709" : "BT.601");
                return w > 0 && h > 0;
            }

            // The next frame's sample, following any change of format on the way. False at the end (or on an error).
            bool ReadFrameSample(IMFSample*& sample, double& stamp)
            {
                sample = nullptr;
                for (int tries = 0; tries < 1000; tries++)
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG at = 0;
                    HRESULT hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &stream, &flags, &at, &sample);
                    if (FAILED(hr)) { SDL_Log("Movie: reading a frame failed (0x%08lX)", static_cast<unsigned long>(hr)); Release(sample); return false; }
                    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) ReadVideoType();
                    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { Release(sample); return false; }
                    if (flags & MF_SOURCE_READERF_ERROR) { SDL_Log("Movie: the video stream failed"); Release(sample); return false; }
                    if (sample) { stamp = double(at) / 1e7; return true; }
                }
                SDL_Log("Movie: no frame came");
                return false;
            }

            // The sample's picture into frame; false if there was nothing to copy.
            bool CopyFrame(IMFSample* sample)
            {
                // first: the top row; pitch: from one row to the next (negative for a bottom-up RGB image). Each
                // shown pixel takes the source pixel it lands on.
                auto copy = [&](const BYTE* first, LONG pitch) {
                    for (int y = 0; y < height; y++)
                    {
                        int sy = int(top) + int(int64_t(y) * sourceHeight / height);
                        uint32_t* out = frame.data() + size_t(y) * size_t(width);
                        if (!nv12)
                        {
                            const uint32_t* in = reinterpret_cast<const uint32_t*>(first + LONG(sy) * pitch);
                            for (int x = 0; x < width; x++) out[x] = in[left + int(int64_t(x) * sourceWidth / width)] & 0xFFFFFF;
                            continue;
                        }
                        // NV12: the Y plane, then the U and V samples interleaved at half size, below the full
                        // (padded) frame height
                        const BYTE* luma = first + LONG(sy) * pitch;
                        const BYTE* chroma = first + LONG(frameHeight) * pitch + LONG(sy / 2) * pitch;
                        Nv12Row(luma, chroma, int(left), sourceWidth, out, width, bt709);
                    }
                };
                IMFMediaBuffer* buffer = nullptr;
                if (SUCCEEDED(sample->GetBufferByIndex(0, &buffer)))
                {
                    // a 2D buffer says where its top row is and which way the rows go
                    IMF2DBuffer* twoD = nullptr;
                    bool copied = false;
                    if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&twoD))))
                    {
                        BYTE* first = nullptr;
                        LONG pitch = 0;
                        if (SUCCEEDED(twoD->Lock2D(&first, &pitch)))
                        {
                            copy(first, pitch);
                            copied = true;
                            twoD->Unlock2D();
                        }
                        Release(twoD);
                    }
                    Release(buffer);
                    if (copied) return true;
                }
                if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return false;
                // otherwise the media type's stride (negative: bottom-up)
                bool copied = false;
                BYTE* data = nullptr;
                DWORD length = 0;
                if (SUCCEEDED(buffer->Lock(&data, nullptr, &length)))
                {
                    LONG pitch = stride;
                    LONG rows = nv12 ? LONG(frameHeight) * 3 / 2 : LONG(frameHeight);
                    LONG span = (stride < 0 ? -stride : stride) * rows;
                    if (DWORD(span) <= length)
                    {
                        copy(stride < 0 && !nv12 ? data + span + stride : data, pitch);
                        copied = true;
                    }
                    else if (framesFailed == 0) SDL_Log("Movie: a frame's buffer is %lu bytes, not the %ld expected", static_cast<unsigned long>(length), static_cast<long>(span));
                    buffer->Unlock();
                }
                Release(buffer);
                return copied;
            }

            bool ReadAudioType()
            {
                IMFMediaType* current = nullptr;
                if (FAILED(audioReader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &current))) return false;
                int newRate = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
                int newChannels = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_NUM_CHANNELS, 0));
                UINT32 bits = MFGetAttributeUINT32(current, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);
                GUID subtype{};
                current->GetGUID(MF_MT_SUBTYPE, &subtype);
                Release(current);
                SDL_Log("Movie: sound %s %d Hz, %d channels, %u bits", subtype == MFAudioFormat_PCM ? "PCM" : "not PCM", newRate, newChannels, bits);
                if (subtype != MFAudioFormat_PCM || bits != 16 || newRate <= 0 || newChannels <= 0)
                {
                    // not what was asked for: no sound rather than noise
                    rate = channels = 0;
                    audioDone = true;
                    return false;
                }
                if (!firstAudio && (rate || channels) && (newRate != rate || newChannels != channels))
                {
                    // the sound stream was opened at the old format; going on would play it at the wrong speed
                    SDL_Log("Movie: the sound's format changed part way through; the rest is left out");
                    audioDone = true;
                    return false;
                }
                rate = newRate;
                channels = newChannels;
                return true;
            }

            // Appends at least `seconds` of sound (at least one sample's worth), whole sample frames only.
            void ReadAudioSamples(std::vector<int16_t>& audio, double seconds)
            {
                const size_t start = audio.size();
                for (int got = 0; !audioDone; )
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG at = 0;
                    IMFSample* sound = nullptr;
                    HRESULT hr = audioReader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &stream, &flags, &at, &sound);
                    if (FAILED(hr)) { SDL_Log("Movie: reading the sound failed (0x%08lX)", static_cast<unsigned long>(hr)); audioDone = true; Release(sound); break; }
                    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) { if (!ReadAudioType()) { Release(sound); break; } }
                    if (flags & (MF_SOURCE_READERF_ENDOFSTREAM | MF_SOURCE_READERF_ERROR)) { audioDone = true; Release(sound); break; }
                    if (!sound) continue;
                    IMFMediaBuffer* pcm = nullptr;
                    if (SUCCEEDED(sound->ConvertToContiguousBuffer(&pcm)))
                    {
                        BYTE* data = nullptr;
                        DWORD length = 0;
                        if (SUCCEEDED(pcm->Lock(&data, nullptr, &length)))
                        {
                            size_t frameBytes = sizeof(int16_t) * size_t(channels);
                            size_t count = (length / frameBytes) * size_t(channels);
                            size_t at2 = audio.size();
                            audio.resize(at2 + count);
                            std::memcpy(audio.data() + at2, data, count * sizeof(int16_t));
                            pcm->Unlock();
                        }
                        Release(pcm);
                    }
                    Release(sound);
                    got++;
                    if (got > 0 && audio.size() - start >= size_t(seconds * rate) * size_t(channels)) break;
                }
            }

            IMFSourceReader* reader = nullptr;       // the picture
            IMFSourceReader* audioReader = nullptr;  // the sound
            bool started = false, comStarted = false;
            int width = 0, height = 0, rate = 0, channels = 0; // the picture as shown; the sound
            int32_t stride = 0;
            LONG left = 0, top = 0;                  // where the picture is in the decoded frame
            int frameWidth = 0, frameHeight = 0;     // the decoded frame, padding included (NV12's colour plane starts below it)
            int sourceWidth = 0, sourceHeight = 0;   // the picture in the decoded frame
            bool nv12 = false, bt709 = true;
            double time = 0;
            bool audioDone = false, firstAudio = false;
            IMFSample* pending = nullptr;            // the first frame, read while opening
            double pendingTime = 0;
            std::vector<int16_t> pendingAudio;       // the first sound, likewise
            std::vector<uint32_t> frame;
            int framesRead = 0, framesFailed = 0;
        };
    }

    std::unique_ptr<Movie> OpenMp4(const std::filesystem::path& path, std::string& error)
    {
        auto movie = std::make_unique<Mp4Movie>();
        if (!movie->Open(path, error)) return nullptr;
        return movie;
    }
}
