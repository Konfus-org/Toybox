#include "tbx/audio/audio.h"
#include "tbx/audio/clip.h"
#include "tbx/audio/source.h"
#include "tbx/runtime.h"
#include <gtest/gtest.h>
#include <cstring>

namespace tbx
{
    /// @brief
    /// Purpose: Builds a minimal PCM16 mono WAV in memory (no filesystem in unit tests).
    static std::vector<std::byte> build_wav(const std::vector<int16>& samples)
    {
        auto bytes = std::vector<std::byte>();
        auto push = [&bytes](const void* data, const size count)
        {
            const auto* raw = static_cast<const std::byte*>(data);
            bytes.insert(bytes.end(), raw, raw + count);
        };
        const uint32 data_size = static_cast<uint32>(samples.size() * 2);
        const uint32 riff_size = 36 + data_size;
        const uint16 format = 1, channels = 1, block = 2, bits = 16;
        const uint32 rate = 48000, byte_rate = rate * block;
        push("RIFF", 4);
        push(&riff_size, 4);
        push("WAVE", 4);
        push("fmt ", 4);
        const uint32 fmt_size = 16;
        push(&fmt_size, 4);
        push(&format, 2);
        push(&channels, 2);
        push(&rate, 4);
        push(&byte_rate, 4);
        push(&block, 2);
        push(&bits, 2);
        push("data", 4);
        push(&data_size, 4);
        push(samples.data(), data_size);
        return bytes;
    }

    TEST(Audio, ParseWavDecodesPcm16)
    {
        // Arrange
        const auto bytes = build_wav({0, 16384, -16384, 32767});

        // Act
        const auto clip = parse_wav(bytes);

        // Assert
        ASSERT_TRUE(clip.has_value()) << clip.error();
        EXPECT_EQ(clip->channels, 1);
        EXPECT_EQ(clip->sample_rate, 48000);
        ASSERT_EQ(clip->samples.size(), 4u);
        EXPECT_NEAR(clip->samples[1], 0.5f, 0.001f);
        EXPECT_NEAR(clip->samples[2], -0.5f, 0.001f);
    }

    TEST(Audio, ParseWavRejectsGarbage)
    {
        // Arrange
        const auto garbage = std::vector<std::byte>(64, std::byte {0x42});

        // Act
        const auto clip = parse_wav(garbage);

        // Assert
        EXPECT_FALSE(clip.has_value());
    }

    TEST(Audio, UpdateWithoutListenerIsHarmless)
    {
        // Arrange
        auto runtime = Runtime();
        runtime.state->sandbox.add("Speaker").with(AudioSource {});

        // Act / Assert: no listener, no clip loaded — surviving IS the behavior.
        update_audio(
            runtime.state->audio,
            runtime.state->sandbox,
            runtime.state->assets,
            runtime.state->events,
            0.016f);
        update_audio(
            runtime.state->audio,
            runtime.state->sandbox,
            runtime.state->assets,
            runtime.state->events,
            0.016f);
        SUCCEED();
    }
}
