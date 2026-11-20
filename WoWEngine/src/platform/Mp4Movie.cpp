// WoWRevived's .mp4 movies through Media Foundation's source reader (Windows only): the video as 32-bit RGB,
// the sound as 16-bit PCM, kept a little ahead of the picture.
#include "platform/Movie.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

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
                Release(reader);
                if (started) MFShutdown();
                if (comStarted) CoUninitialize();
            }

            bool Open(const std::filesystem::path& path, std::string& error)
            {
                HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                comStarted = SUCCEEDED(hr);
                if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) { error = "Media Foundation isn't available"; return false; }
                started = true;

                IMFAttributes* attributes = nullptr;
                MFCreateAttributes(&attributes, 1);
                if (attributes) attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
                hr = MFCreateSourceReaderFromURL(path.wstring().c_str(), attributes, &reader);
                Release(attributes);
                if (FAILED(hr) || !reader) { error = "can't open it"; return false; }

                // the picture as 32-bit RGB
                IMFMediaType* type = nullptr;
                MFCreateMediaType(&type);
                type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                hr = reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type);
                Release(type);
                if (FAILED(hr)) { error = "its video can't be decoded"; return false; }
                IMFMediaType* current = nullptr;
                if (FAILED(reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current))) { error = "no video"; return false; }
                UINT32 w = 0, h = 0;
                MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &w, &h);
                stride = int32_t(MFGetAttributeUINT32(current, MF_MT_DEFAULT_STRIDE, UINT32(w * 4)));
                // the decoder can pad the frame (1088 lines for 1080): the picture is the display aperture
                width = int(w);
                height = int(h);
                MFVideoArea area{};
                UINT32 got = 0;
                if (SUCCEEDED(current->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), &got)) &&
                    got == sizeof(area) && area.Area.cx > 0 && area.Area.cy > 0 &&
                    area.OffsetX.value + area.Area.cx <= LONG(w) && area.OffsetY.value + area.Area.cy <= LONG(h))
                {
                    left = area.OffsetX.value;
                    top = area.OffsetY.value;
                    width = int(area.Area.cx);
                    height = int(area.Area.cy);
                }
                frameHeight = int(h);
                Release(current);
                if (!w || !h) { error = "no picture size"; return false; }
                frame.assign(size_t(width) * size_t(height), 0);

                // the sound as 16-bit PCM, if there is any
                MFCreateMediaType(&type);
                type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                hr = reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type);
                Release(type);
                if (SUCCEEDED(hr) && SUCCEEDED(reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &current)))
                {
                    rate = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
                    channels = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_NUM_CHANNELS, 0));
                    Release(current);
                }
                if (rate <= 0 || channels <= 0)
                {
                    rate = channels = 0;
                    audioDone = true;
                    reader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), FALSE);
                }
                return true;
            }

            int Width() const override { return width; }
            int Height() const override { return height; }
            int AudioRate() const override { return rate; }
            int AudioChannels() const override { return channels; }
            double Time() const override { return time; }
            const std::vector<uint32_t>& Frame() const override { return frame; }

            bool Next(std::vector<int16_t>& audio) override
            {
                IMFSample* sample = nullptr;
                for (;;)
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG stamp = 0;
                    HRESULT hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &stream, &flags, &stamp, &sample);
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { Release(sample); return false; }
                    if (sample) { time = double(stamp) / 1e7; break; }
                }
                auto copy = [&](const BYTE* first, LONG pitch) {
                    // first: the top row; pitch: from one row to the next (negative for a bottom-up image)
                    for (int y = 0; y < height; y++)
                    {
                        const uint32_t* in = reinterpret_cast<const uint32_t*>(first + LONG(top + y) * pitch) + left;
                        uint32_t* out = frame.data() + size_t(y) * size_t(width);
                        for (int x = 0; x < width; x++) out[x] = in[x] & 0xFFFFFF;
                    }
                };
                IMFMediaBuffer* buffer = nullptr;
                bool copied = false;
                if (SUCCEEDED(sample->GetBufferByIndex(0, &buffer)))
                {
                    // a 2D buffer says where its top row is and which way the rows go
                    IMF2DBuffer* twoD = nullptr;
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
                }
                if (!copied && SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)))
                {
                    // otherwise the media type's stride (negative: bottom-up)
                    BYTE* data = nullptr;
                    DWORD length = 0;
                    if (SUCCEEDED(buffer->Lock(&data, nullptr, &length)))
                    {
                        LONG pitch = stride;
                        LONG span = (stride < 0 ? -stride : stride) * LONG(frameHeight);
                        if (DWORD(span) <= length)
                            copy(stride < 0 ? data + span + stride : data, pitch);
                        buffer->Unlock();
                    }
                    Release(buffer);
                }
                Release(sample);

                // the sound up to half a second past this frame
                while (!audioDone && audioTime <= time + 0.5)
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG stamp = 0;
                    IMFSample* sound = nullptr;
                    HRESULT hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &stream, &flags, &stamp, &sound);
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { audioDone = true; Release(sound); break; }
                    if (!sound) continue;
                    audioTime = double(stamp) / 1e7;
                    IMFMediaBuffer* pcm = nullptr;
                    if (SUCCEEDED(sound->ConvertToContiguousBuffer(&pcm)))
                    {
                        BYTE* data = nullptr;
                        DWORD length = 0;
                        if (SUCCEEDED(pcm->Lock(&data, nullptr, &length)))
                        {
                            size_t count = length / sizeof(int16_t);
                            size_t at = audio.size();
                            audio.resize(at + count);
                            std::memcpy(audio.data() + at, data, count * sizeof(int16_t));
                            pcm->Unlock();
                        }
                        Release(pcm);
                    }
                    Release(sound);
                }
                return true;
            }

        private:
            IMFSourceReader* reader = nullptr;
            bool started = false, comStarted = false;
            int width = 0, height = 0, rate = 0, channels = 0;
            int32_t stride = 0;
            LONG left = 0, top = 0;   // the display aperture in the decoded frame
            int frameHeight = 0;
            double time = 0, audioTime = 0;
            bool audioDone = false;
            std::vector<uint32_t> frame;
        };
    }

    std::unique_ptr<Movie> OpenMp4(const std::filesystem::path& path, std::string& error)
    {
        auto movie = std::make_unique<Mp4Movie>();
        if (!movie->Open(path, error)) return nullptr;
        return movie;
    }
}
