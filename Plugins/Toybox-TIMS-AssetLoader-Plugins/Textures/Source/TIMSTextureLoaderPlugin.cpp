#include "TIMSTextureLoaderPlugin.h"
#include "Tbx/Debug/Asserts.h"
#include <stb_image.h>
#include <cstring>

namespace Tbx::Plugins::TIMS
{
    /////////////// LOADER /////////////////////

    bool TIMSTextureLoaderPlugin::CanLoadTexture(const std::filesystem::path& filepath) const
    {
        return filepath.extension() == ".png" ||
               filepath.extension() == ".jpg" ||
               filepath.extension() == ".jpeg";
    }

    Ref<Texture> TIMSTextureLoaderPlugin::LoadTexture(const std::filesystem::path& filepath)
    {
        // Load texture with stbimg
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);
        stbi_uc* data = stbi_load(filepath.string().c_str(), &width, &height, &channels, 0);
        
        // Ensure texture loaded correctly
        if (!data)
        {
            TBX_ASSERT(false, "TIMS: Failed to load texture file at {}!", filepath.string());
            return nullptr;
        }

        // Allocate and copy into vector
        size_t dataSize = static_cast<size_t>(width) * height * channels;
        std::vector<unsigned char> pixelData(dataSize);
        std::memcpy(pixelData.data(), data, dataSize);

        // Create tbx in memory texture and return it
        auto texture = Create(
            Size(width, height),
            TextureWrap::Repeat,
            TextureFilter::Nearest,
            channels == 4 ? TextureFormat::RGBA : TextureFormat::RGB,
            pixelData);

        // Free the stb data
        stbi_image_free(data);

        return texture;
    }
}
