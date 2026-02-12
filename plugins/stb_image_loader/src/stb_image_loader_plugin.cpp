#include "stb_image_loader_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_meta.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/graphics/texture.h"
#include <stb_image.h>
#include <memory>
#include <string>
#include <vector>

namespace tbx::plugins
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

    void StbImageLoaderPlugin::on_attach(IPluginHost& host)
    {
        _asset_manager = &host.get_asset_manager();
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>(host.get_settings().working_directory);
    }

    void StbImageLoaderPlugin::on_detach()
    {
        _asset_manager = nullptr;
    }

    void StbImageLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadTextureRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_texture_request(*request);
    }

    void StbImageLoaderPlugin::set_file_ops(std::shared_ptr<IFileOps> file_ops)
    {
        _file_ops = std::move(file_ops);
    }

    void StbImageLoaderPlugin::on_load_texture_request(LoadTextureRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Stb image loader: missing texture asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::CANCELLED;
            request.result.flag_failure("Stb image loader cancelled.");
            return;
        }

        if (!_asset_manager)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Stb image loader: asset manager unavailable.");
            return;
        }

        if (!_file_ops)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Stb image loader: file services unavailable.");
            return;
        }

        TextureSettings load_settings = {};
        load_settings.wrap = request.wrap;
        load_settings.filter = request.filter;
        load_settings.format = request.format;
        load_settings.mipmaps = request.mipmaps;
        load_settings.compression = request.compression;

        const std::filesystem::path resolved_path = _asset_manager->resolve_asset_path(request.path);
        auto meta_path = resolved_path;
        meta_path += ".meta";
        std::string meta_data;
        if (_file_ops->read_file(meta_path, FileDataFormat::UTF8_TEXT, meta_data))
        {
            AssetMeta meta = {};
            AssetMetaParser parser = {};
            if (parser.try_parse_from_source(meta_data, resolved_path, meta)
                && meta.texture_settings.has_value())
                load_settings = *meta.texture_settings;
        }

        std::string encoded_image;
        if (!_file_ops->read_file(resolved_path, FileDataFormat::BINARY, encoded_image))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(build_load_failure_message(resolved_path, "file could not be read"));
            return;
        }

        stbi_set_flip_vertically_on_load(true);
        int width = 0;
        int height = 0;
        const int desired_channels = (load_settings.format == TextureFormat::RGB) ? 3 : 4;
        stbi_uc* raw_data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(encoded_image.data()),
            static_cast<int>(encoded_image.size()),
            &width,
            &height,
            nullptr,
            desired_channels);
        if (!raw_data)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(resolved_path, stbi_failure_reason()));
            return;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        std::vector<Pixel> pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
        Texture texture(
            resolution,
            load_settings.wrap,
            load_settings.filter,
            load_settings.format,
            pixels,
            load_settings.mipmaps,
            load_settings.compression);
        *asset = texture;

        request.state = MessageState::HANDLED;
    }
}
