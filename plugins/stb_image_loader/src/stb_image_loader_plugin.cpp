#include "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/texture.h"
#include <memory>
#include <stb_image.h>
#include <string>
#include <vector>


namespace stb_image_loader
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

    void StbImageLoaderPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _serialization_registry = &service_provider.get_service<tbx::SerializationRegistry>();
        if (!_file_ops)
            _file_ops = std::make_unique<tbx::FileOperator>(
                service_provider.get_service<tbx::AppSettings>().paths.working_directory);

        _serialization_registry->register_reader<tbx::Texture>(
            [this](const std::filesystem::path& asset_path,
                   const tbx::TextureLoadParameters& parameters)
            {
                return read_texture(asset_path, parameters);
            });
    }

    void StbImageLoaderPlugin::on_detach()
    {
        if (_serialization_registry)
        {
            _serialization_registry->deregister_reader<tbx::Texture>();
        }

        _serialization_registry = nullptr;
    }

    std::shared_ptr<tbx::Texture> StbImageLoaderPlugin::read_texture(
        const std::filesystem::path& asset_path,
        const tbx::TextureLoadParameters& parameters) const
    {
        if (!_file_ops)
        {
            TBX_TRACE_WARNING("Stb image loader: file services unavailable.");
            return {};
        }

        tbx::Texture load_texture = parameters.texture;

        auto meta_path = asset_path;
        meta_path += ".meta";
        if (std::string meta_data = {};
            _file_ops->read_file(meta_path, tbx::FileDataFormat::UTF8_TEXT, meta_data))
        {
            try
            {
                const auto data = tbx::Json(meta_data);
                try_parse_texture(data, load_texture);
            }
            catch (...)
            {
                // Ignore meta parsing errors and fall back to request settings.
                TBX_TRACE_WARNING(
                    "Failed to parse texture meta data for {}",
                    asset_path.string());
            }
        }

        std::string encoded_image;
        if (!_file_ops->read_file(asset_path, tbx::FileDataFormat::BINARY, encoded_image))
        {
            TBX_TRACE_WARNING(
                "{}",
                build_load_failure_message(asset_path, "file could not be read"));
            return {};
        }

        stbi_set_flip_vertically_on_load(true);
        int width = 0;
        int height = 0;
        const int desired_channels = load_texture.format == tbx::TextureFormat::RGB ? 3 : 4;
        stbi_uc* raw_data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(encoded_image.data()),
            static_cast<int>(encoded_image.size()),
            &width,
            &height,
            nullptr,
            desired_channels);
        if (!raw_data)
        {
            TBX_TRACE_WARNING(
                "{}",
                build_load_failure_message(asset_path, stbi_failure_reason()));
            return {};
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        const std::vector pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        const tbx::Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
        return std::make_shared<tbx::Texture>(
            resolution,
            load_texture.wrap,
            load_texture.filter,
            load_texture.format,
            load_texture.mipmaps,
            load_texture.compression,
            pixels);
    }
}
