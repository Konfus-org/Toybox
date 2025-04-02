#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Texture.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include <stb_image.h>

namespace Tbx
{
    Texture::Texture(const std::string& path)
    {
        int width, height, channels;
        auto* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        TBX_ASSERT(data, "Failed to load texture: {0}", path);

        std::shared_ptr<TextureData> sharedData(data, [](TextureData* dataToDelete)
        {
            stbi_image_free(dataToDelete);
        });

        _data = sharedData;
        _width = width;
        _height = height;
        _channels = channels;
    }
}