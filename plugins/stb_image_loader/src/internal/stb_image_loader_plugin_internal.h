#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/texture.h"
#include <memory>
#include <stb_image.h>
#include <string>
#include <vector>

namespace stb_image_loader::internal
{
    static bool try_parse_texture(const tbx::Json& data, tbx::Texture& out_texture)
    {
        auto texture_data = tbx::Json();
        if (!data.try_get_child("texture", texture_data))
            return false;

        auto texture = out_texture;
        texture_data.try_get<tbx::TextureWrap>("wrap", texture.wrap);
        texture_data.try_get<tbx::TextureFilter>("filter", texture.filter);
        texture_data.try_get<tbx::TextureFormat>("format", texture.format);
        texture_data.try_get<tbx::TextureMipmaps>("mipmaps", texture.mipmaps);
        texture_data.try_get<tbx::TextureCompression>("compression", texture.compression);
        out_texture = texture;
        return true;
    }

    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        const char* reason)
    {
        std::string message = "Stb image loader failed to load image: ";
        message.append(path.string());
        if (reason && *reason)
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

}
