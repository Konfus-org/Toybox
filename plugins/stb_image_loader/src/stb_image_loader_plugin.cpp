#include "stb_image_loader_plugin.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/texture.h"
#include "tbx/utils/string_utils.h"
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

    void StbImageLoader::on_attach()
    {
        auto registry = serialization_registry.lock();
        if (!registry)
            return;

        registry->register_reader<tbx::Texture>(
            {"png", "jpg", "jpeg", "tga", "bmp"},
            [this](
                const std::filesystem::path& asset_path,
                const tbx::TextureLoadParameters& parameters,
                const tbx::AssetLoadMetadata& metadata,
                tbx::Texture& texture)
            {
                return read_texture(asset_path, parameters, metadata, texture);
            });
    }

    void StbImageLoader::on_detach()
    {
        if (auto registry = serialization_registry.lock())
        {
            registry->deregister_reader<tbx::Texture>();
        }

        file_ops = {};
        serialization_registry = {};
    }

    tbx::Result StbImageLoader::read_texture(
        const std::filesystem::path& asset_path,
        const tbx::TextureLoadParameters& parameters,
        const tbx::AssetLoadMetadata&,
        tbx::Texture& texture) const
    {
        auto result = tbx::Result();
        auto files = file_ops.lock();
        if (!files)
        {
            result.failure("Stb image loader: file services unavailable.");
            return result;
        }

        tbx::Texture load_texture = parameters.texture;
        auto meta_path = asset_path;
        meta_path += ".meta";
        if (files->exists(meta_path))
        {
            auto meta_data = std::string {};
            if (!files->read_file(meta_path, tbx::FileDataFormat::UTF8_TEXT, meta_data))
            {
                result.failure(
                    build_load_failure_message(asset_path, "texture metadata could not be read"));
                return result;
            }

            const auto meta_result = tbx::read_json_asset_meta_Texture(meta_data, load_texture);
            if (!meta_result.succeeded())
            {
                result.failure(meta_result.get_report());
                return result;
            }
        }

        std::string encoded_image;
        if (!files->read_file(asset_path, tbx::FileDataFormat::BINARY, encoded_image))
        {
            result.failure(build_load_failure_message(asset_path, "file could not be read"));
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
            result.failure(build_load_failure_message(asset_path, stbi_failure_reason()));
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
        result.ok();
        return result;
    }
}
