#include "tbx/gfx/texture.h"
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>

namespace tbx::tests
{
    TEST(Texture, SaveRoundTripsThroughLoad)
    {
        // Arrange: a 2x2 texture with four distinct opaque pixels.
        auto original = gfx::Texture();
        original.width = 2;
        original.height = 2;
        constexpr unsigned char BYTES[] = {
            255, 0,   0,   255, // red
            0,   255, 0,   255, // green
            0,   0,   255, 255, // blue
            255, 255, 0,   255}; // yellow
        for (const unsigned char value : BYTES)
            original.pixels.push_back(std::byte(value));
        const auto path = std::filesystem::temp_directory_path() / "tbx_texture_test.bmp";

        // Act
        const auto saved = save(original, path);
        const auto loaded = assets::load<gfx::Texture>(path);

        // Assert: the BMP written by save() decodes back to the exact same image.
        ASSERT_TRUE(saved.has_value()) << saved.error();
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->width, original.width);
        EXPECT_EQ(loaded->height, original.height);
        EXPECT_EQ(loaded->pixels, original.pixels);
    }

    TEST(Texture, SaveRejectsAnEmptyTexture)
    {
        // Arrange
        const auto empty = gfx::Texture();
        const auto path = std::filesystem::temp_directory_path() / "tbx_texture_empty.bmp";

        // Act
        const auto saved = save(empty, path);

        // Assert
        EXPECT_FALSE(saved.has_value());
        EXPECT_FALSE(std::filesystem::exists(path));
    }
}
