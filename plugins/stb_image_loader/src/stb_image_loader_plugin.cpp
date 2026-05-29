#include "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/texture.h"
#include <stb_image.h>

namespace stb_image_loader
{
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
        _serialization_registry = service_provider.get_service<tbx::SerializationRegistry>();
        auto serialization_registry = _serialization_registry.lock();
        if (!serialization_registry)
            return;

        if (!_file_ops)
        {
            auto settings = service_provider.get_service<tbx::AppSettings>().lock();
            if (!settings)
                return;

            _file_ops = std::make_unique<tbx::FileOperator>(settings->paths.working_directory);
        }

        serialization_registry->register_loader<tbx::Texture>(
            [this](
                const std::filesystem::path& asset_path,
                const tbx::TextureLoadParameters& parameters,
                const tbx::AssetLoadMetadata& metadata,
                tbx::Texture& texture)
            {
                return read_texture(asset_path, parameters, metadata, texture);
            });
    }

    void StbImageLoaderPlugin::on_detach(tbx::ServiceProvider&)
    {
        if (auto serialization_registry = _serialization_registry.lock())
        {
            serialization_registry->deregister_loader<tbx::Texture>();
        }

        _serialization_registry = {};
    }

    tbx::Result StbImageLoaderPlugin::read_texture(
        const std::filesystem::path& asset_path,
        const tbx::TextureLoadParameters& parameters,
        const tbx::AssetLoadMetadata&,
        tbx::Texture& texture) const
    {
        auto result = tbx::Result {};
        if (!_file_ops)
        {
            result.flag_failure("Stb image loader: file services unavailable.");
            return result;
        }

        tbx::Texture load_texture = parameters.texture;
        auto meta_path = asset_path;
        meta_path += ".meta";
        if (_file_ops->exists(meta_path))
        {
            auto meta_data = std::string {};
            if (!_file_ops->read_file(meta_path, tbx::FileDataFormat::UTF8_TEXT, meta_data))
            {
                result.flag_failure(
                    build_load_failure_message(asset_path, "texture metadata could not be read"));
                return result;
            }

            const auto meta_result = tbx::tbx_read_json_asset_meta_Texture(meta_data, load_texture);
            if (!meta_result.succeeded())
            {
                result.flag_failure(meta_result.get_report());
                return result;
            }
        }

        std::string encoded_image;
        if (!_file_ops->read_file(asset_path, tbx::FileDataFormat::BINARY, encoded_image))
        {
            result.flag_failure(build_load_failure_message(asset_path, "file could not be read"));
            return result;
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
            result.flag_failure(build_load_failure_message(asset_path, stbi_failure_reason()));
            return result;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        const std::vector pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        const tbx::Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
        texture = tbx::Texture(
            resolution,
            load_texture.wrap,
            load_texture.filter,
            load_texture.format,
            load_texture.mipmaps,
            load_texture.compression,
            pixels);
        result.flag_success();
        return result;
    }
}
