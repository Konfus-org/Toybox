#include "Tbx/PCH.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Math/Size.h"
#include <filesystem>

namespace Tbx
{
    Texture::Texture(const Size& size, int channels, std::shared_ptr<TextureData> data)
    {
        _width = size.Width;
        _height = size.Height;
        _channels = channels;
        _data = data;
    }
}