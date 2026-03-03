#include "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"
#include "tbx/app/app_settings.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/files/json.h"
#include "tbx/graphics/texture.h"
#include <memory>
#include <stb_image.h>
#include <string>
#include <vector>

namespace stb_image_loader
{
    static bool try_parse_texture_settings(
        const tbx::Json& data,
        tbx::TextureSettings& out_settings)
    {
        auto texture_data = tbx::Json();
        if (!data.try_get_child("texture", texture_data))
            return false;

        auto settings = tbx::TextureSettings();
        texture_data.try_get<tbx::TextureWrap>("wrap", settings.wrap);
        texture_data.try_get<tbx::TextureFilter>("filter", settings.filter);
        texture_data.try_get<tbx::TextureFormat>("format", settings.format);
        texture_data.try_get<tbx::TextureMipmaps>("mipmaps", settings.mipmaps);
        texture_data.try_get<tbx::TextureCompression>("compression", settings.compression);
        out_settings = settings;
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

    void StbImageLoaderPlugin::on_attach(tbx::IPluginHost& host)
    {
        if (!_file_ops)
            _file_ops =
                std::make_unique<tbx::FileOperator>(host.get_settings().paths.working_directory);
    }

    void StbImageLoaderPlugin::on_detach() {}

    void StbImageLoaderPlugin::on_recieve_message(tbx::Message& msg)
    {
        auto* request = handle_message<tbx::LoadTextureRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_texture_request(*request);
    }

    void StbImageLoaderPlugin::on_load_texture_request(tbx::LoadTextureRequest& request) const
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("Stb image loader: missing texture asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = tbx::MessageState::CANCELLED;
            request.result.flag_failure("Stb image loader cancelled.");
            return;
        }

        if (!_file_ops)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("Stb image loader: file services unavailable.");
            return;
        }

        tbx::TextureSettings load_settings = {
            .wrap = request.wrap,
            .filter = request.filter,
            .format = request.format,
            .mipmaps = request.mipmaps,
            .compression = request.compression,
        };

        auto meta_path = request.path;
        meta_path += ".meta";
        if (std::string meta_data = {};
            _file_ops->read_file(meta_path, tbx::FileDataFormat::UTF8_TEXT, meta_data))
        {
            try
            {
                const auto data = tbx::Json(meta_data);
                if (auto parsed_settings = tbx::TextureSettings();
                    try_parse_texture_settings(data, parsed_settings))
                {
                    load_settings = parsed_settings;
                }
            }
            catch (...)
            {
                // Ignore meta parsing errors and fall back to request settings.
                TBX_TRACE_WARNING("Failed to parse texture meta data for {}", request.path);
            }
        }

        std::string encoded_image;
        if (!_file_ops->read_file(request.path, tbx::FileDataFormat::BINARY, encoded_image))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, "file could not be read"));
            return;
        }

        stbi_set_flip_vertically_on_load(true);
        int width = 0;
        int height = 0;
        const int desired_channels = load_settings.format == tbx::TextureFormat::RGB ? 3 : 4;
        stbi_uc* raw_data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(encoded_image.data()),
            static_cast<int>(encoded_image.size()),
            &width,
            &height,
            nullptr,
            desired_channels);
        if (!raw_data)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, stbi_failure_reason()));
            return;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        const std::vector pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        const tbx::Size resolution = {
            static_cast<tbx::uint32>(width),
            static_cast<tbx::uint32>(height)};
        const tbx::Texture texture(
            resolution,
            load_settings.wrap,
            load_settings.filter,
            load_settings.format,
            load_settings.mipmaps,
            load_settings.compression,
            pixels);
        *asset = texture;

        request.state = tbx::MessageState::HANDLED;
    }
}
