#include "tbx/gfx/texture.h"
#include "tbx/files/files.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

namespace tbx
{
    template <>
    Result<Texture> load<Texture>(const std::filesystem::path& path)
    {
        auto bytes = files::read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes->data()),
            static_cast<int>(bytes->size()),
            &width,
            &height,
            &channels,
            4);
        if (!pixels)
            return fail("could not decode image '{}': {}", path.string(), stbi_failure_reason());
        auto texture = Texture {.width = width, .height = height};
        texture.pixels.assign(
            reinterpret_cast<const std::byte*>(pixels),
            reinterpret_cast<const std::byte*>(pixels) + width * height * 4);
        stbi_image_free(pixels);
        return texture;
    }
}
