// WoWRevived's .mp4 movies through Media Foundation's source reader (Windows only): the video as 32-bit RGB,
// the sound as 16-bit PCM from a second reader of its own, so it never waits on the pictures.
#include "platform/Movie.h"
#include "platform/Nv12.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

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
                // ahead of what's heard however long the pictures take to decode (one reader would have to decode
                // the pictures in between to get at the sound, and would keep them all in memory meanwhile)
                reader = OpenReader(path, true);
                if (!reader) { error = "can't open it"; return false; }
                reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
                reader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

                // The picture as the decoder gives it (NV12), turned into RGB here only at the size it's shown:
                // Media Foundation's own converter does every pixel of a full-HD frame in software, far slower than
                // the movie plays. RGB32 through that converter is the fallback.
                IMFMediaType* type = nullptr;
                for (const GUID& subtype : { MFVideoFormat_NV12, MFVideoFormat_RGB32 })
                {
                    MFCreateMediaType(&type);
                    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                    type->SetGUID(MF_MT_SUBTYPE, subtype);
                    hr = reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type);
                    Release(type);
                    if (SUCCEEDED(hr)) { nv12 = subtype == MFVideoFormat_NV12; break; }
                }
                if (FAILED(hr)) { error = "its video can't be decoded"; return false; }
                IMFMediaType* current = nullptr;
                if (FAILED(reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current))) { error = "no video"; return false; }
                UINT32 w = 0, h = 0;
                MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &w, &h);
                stride = int32_t(MFGetAttributeUINT32(current, MF_MT_DEFAULT_STRIDE, UINT32(nv12 ? w : w * 4)));
                // HD pictures are BT.709 unless the file says otherwise
                UINT32 matrix = MFGetAttributeUINT32(current, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown);
                bt709 = matrix == MFVideoTransferMatrix_BT709 || (matrix != MFVideoTransferMatrix_BT601 && h >= 720);
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
                sourceWidth = width;
                sourceHeight = height;
                // shown at most 640 x 480 (the game's screen), keeping its shape
                double scale = std::min({ 1.0, 640.0 / width, 480.0 / height });
                width = std::max(1, int(width * scale));
                height = std::max(1, int(height * scale));
                frame.assign(size_t(width) * size_t(height), 0);

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
                    if (SUCCEEDED(hr) && SUCCEEDED(audioReader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &current)))
                    {
                        rate = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
                        channels = int(MFGetAttributeUINT32(current, MF_MT_AUDIO_NUM_CHANNELS, 0));
                        UINT32 bits = MFGetAttributeUINT32(current, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);
                        Release(current);
                        if (bits != 16) rate = channels = 0; // not what was asked for: no sound rather than noise
                    }
                }
                if (rate <= 0 || channels <= 0)
                {
                    rate = channels = 0;
                    audioDone = true;
                    Release(audioReader);
                }
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
                for (;;)
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG stamp = 0;
                    HRESULT hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &stream, &flags, &stamp, &sample);
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { Release(sample); return false; }
                    if (sample) { time = double(stamp) / 1e7; break; }
                }
                // first: the top row; pitch: from one row to the next (negative for a bottom-up RGB image). Each shown
                // pixel takes the source pixel it lands on.
                auto copy = [&](const BYTE* first, LONG pitch) {
                    for (int y = 0; y < height; y++)
                    {
                        int sy = top + int(int64_t(y) * sourceHeight / height);
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
                        Nv12Row(luma, chroma, left, sourceWidth, out, width, bt709);
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
                        LONG rows = nv12 ? LONG(frameHeight) * 3 / 2 : LONG(frameHeight);
                        LONG span = (stride < 0 ? -stride : stride) * rows;
                        if (DWORD(span) <= length)
                            copy(stride < 0 && !nv12 ? data + span + stride : data, pitch);
                        buffer->Unlock();
                    }
                    Release(buffer);
                }
                Release(sample);
                return true;
            }

            // Appends at least `seconds` of sound (whole samples as the decoder gives them); false once there's no more.
            bool ReadAudio(std::vector<int16_t>& audio, double seconds) override
            {
                if (audioDone || !audioReader) return false;
                const size_t want = audio.size() + size_t(seconds * rate) * size_t(channels);
                while (audio.size() < want)
                {
                    DWORD stream = 0, flags = 0;
                    LONGLONG stamp = 0;
                    IMFSample* sound = nullptr;
                    HRESULT hr = audioReader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &stream, &flags, &stamp, &sound);
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { audioDone = true; Release(sound); break; }
                    if (!sound) continue;
                    IMFMediaBuffer* pcm = nullptr;
                    if (SUCCEEDED(sound->ConvertToContiguousBuffer(&pcm)))
                    {
                        BYTE* data = nullptr;
                        DWORD length = 0;
                        if (SUCCEEDED(pcm->Lock(&data, nullptr, &length)))
                        {
                            // whole sample frames only, so the channels never slip
                            size_t frameBytes = sizeof(int16_t) * size_t(channels);
                            size_t count = (length / frameBytes) * size_t(channels);
                            size_t at = audio.size();
                            audio.resize(at + count);
                            std::memcpy(audio.data() + at, data, count * sizeof(int16_t));
                            pcm->Unlock();
                        }
                        Release(pcm);
                    }
                    Release(sound);
                }
                return !audioDone || audio.size() > 0;
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
                return SUCCEEDED(hr) ? r : nullptr;
            }

            IMFSourceReader* reader = nullptr;       // the picture
            IMFSourceReader* audioReader = nullptr;  // the sound
            bool started = false, comStarted = false;
            int width = 0, height = 0, rate = 0, channels = 0;
            int32_t stride = 0;
            LONG left = 0, top = 0;   // the display aperture in the decoded frame
            int frameHeight = 0;      // the decoded frame's height, padding included (where NV12's colour plane starts)
            int sourceWidth = 0, sourceHeight = 0; // the picture in the decoded frame; width x height is what's shown
            bool nv12 = false, bt709 = true;
            double time = 0;
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
