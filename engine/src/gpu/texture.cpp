#include "tbx/gpu/texture.h"
#include "tbx/files/files.h"
#include "tbx/utils/typedefs.h"
#include <memory>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

namespace tbx
{
    Result<Texture> gpu_deserialize_texture(const std::filesystem::path& path)
    {
        auto bytes = read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        int width = 0;
        int height = 0;
        int channels = 0;
        // C boundary: stb hands back a malloc'd pixel buffer it expects freed via
        // stbi_image_free.
        const auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>(
            stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(bytes->data()),
                static_cast<int>(bytes->size()),
                &width,
                &height,
                &channels,
                4),
            &stbi_image_free);
        if (!pixels)
            return fail("could not decode image '{}': {}", path.string(), stbi_failure_reason());
        auto texture = Texture();
        texture.width = width;
        texture.height = height;
        texture.pixels.assign(
            reinterpret_cast<const std::byte*>(pixels.get()),
            reinterpret_cast<const std::byte*>(pixels.get()) + width * height * 4);
        return texture;
    }

    Result<void> gpu_serialize_texture(const Texture& texture, const std::filesystem::path& path)
    {
        const auto width = static_cast<uint32>(texture.width);
        const auto height = static_cast<uint32>(texture.height);
        if (width == 0 || height == 0
            || texture.pixels.size() < static_cast<size>(width) * height * 4)
            return fail("texture has no pixels to save");

        // 32-bit BMP, BGRA. The negative height marks the rows as top-down, matching the
        // texture's layout, so no flip on either side of the round trip.
        const uint32 pixel_bytes = width * height * 4;
        auto file = std::vector<std::byte>(54 + pixel_bytes);
        const auto put_u32 = [&file](const size at, const uint32 value)
        {
            file[at + 0] = static_cast<std::byte>(value & 0xFFu);
            file[at + 1] = static_cast<std::byte>((value >> 8u) & 0xFFu);
            file[at + 2] = static_cast<std::byte>((value >> 16u) & 0xFFu);
            file[at + 3] = static_cast<std::byte>((value >> 24u) & 0xFFu);
        };
        file[0] = std::byte('B');
        file[1] = std::byte('M');
        put_u32(2, 54 + pixel_bytes); // file size
        put_u32(10, 54); // pixel data offset
        put_u32(14, 40); // DIB header size
        put_u32(18, width);
        put_u32(22, static_cast<uint32>(-static_cast<int32>(height))); // top-down
        file[26] = std::byte(1); // planes
        file[28] = std::byte(32); // bits per pixel

        for (size pixel = 0; pixel < static_cast<size>(width) * height; ++pixel)
        {
            const std::byte* rgba = texture.pixels.data() + pixel * 4;
            std::byte* bgra = file.data() + 54 + pixel * 4;
            bgra[0] = rgba[2];
            bgra[1] = rgba[1];
            bgra[2] = rgba[0];
            bgra[3] = rgba[3];
        }
        return write_bytes(path, file);
    }
}
