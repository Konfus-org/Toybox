#include "tbx/audio/clip.h"
#include "tbx/files/files.h"
#include <cstring>

namespace tbx
{
    //// WAV PARSING ////

    static uint32 read_u32(const std::byte* at)
    {
        uint32 value = 0;
        std::memcpy(&value, at, sizeof(value));
        return value;
    }

    static uint16 read_u16(const std::byte* at)
    {
        uint16 value = 0;
        std::memcpy(&value, at, sizeof(value));
        return value;
    }

    Result<AudioClip> parse_wav(const std::span<const std::byte> bytes)
    {
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0
            || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
            return fail("not a RIFF/WAVE payload");

        auto clip = AudioClip {};
        uint16 format = 0;
        uint16 bits_per_sample = 0;
        bool has_format = false;

        // Walk the chunks; fmt must precede data.
        size offset = 12;
        while (offset + 8 <= bytes.size())
        {
            const std::byte* chunk = bytes.data() + offset;
            const uint32 chunk_size = read_u32(chunk + 4);
            const std::byte* payload = chunk + 8;
            if (offset + 8 + chunk_size > bytes.size())
                return fail("truncated WAV chunk");

            if (std::memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16)
            {
                format = read_u16(payload);
                clip.channels = read_u16(payload + 2);
                clip.sample_rate = static_cast<int>(read_u32(payload + 4));
                bits_per_sample = read_u16(payload + 14);
                has_format = true;
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                if (!has_format)
                    return fail("WAV data chunk before fmt chunk");
                if (format == 1 && bits_per_sample == 16)
                {
                    const size count = chunk_size / 2;
                    clip.samples.reserve(count);
                    for (size i = 0; i < count; ++i)
                    {
                        int16 sample = 0;
                        std::memcpy(&sample, payload + i * 2, 2);
                        clip.samples.push_back(static_cast<float>(sample) / 32768.0f);
                    }
                }
                else if (format == 3 && bits_per_sample == 32)
                {
                    const size count = chunk_size / 4;
                    clip.samples.resize(count);
                    std::memcpy(clip.samples.data(), payload, count * 4);
                }
                else
                    return fail(
                        "unsupported WAV encoding (format {}, {} bits)", format, bits_per_sample);
                return clip;
            }
            offset += 8 + chunk_size + (chunk_size & 1); // chunks are word-aligned
        }
        return fail("WAV has no data chunk");
    }

    Result<AudioClip> deserialize_clip(const std::filesystem::path& path)
    {
        auto bytes = read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        return parse_wav(*bytes);
    }
}
