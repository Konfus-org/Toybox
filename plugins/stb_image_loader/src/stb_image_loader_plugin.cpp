#include "stb_image_loader_plugin.h"
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
    static bool try_parse_texture_settings(const Json& data, TextureSettings& out_settings)
    {
        auto texture_data = Json();
        if (!data.try_get_child("texture", texture_data))
            return false;

        auto settings = TextureSettings();
        static_cast<void>(texture_data.try_get<TextureWrap>("wrap", settings.wrap));
        static_cast<void>(texture_data.try_get<TextureFilter>("filter", settings.filter));
        static_cast<void>(texture_data.try_get<TextureFormat>("format", settings.format));
        static_cast<void>(texture_data.try_get<TextureMipmaps>("mipmaps", settings.mipmaps));
        static_cast<void>(
            texture_data.try_get<TextureCompression>("compression", settings.compression));
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
            _file_ops = std::make_shared<FileOperator>(host.get_settings().paths.working_directory);
    }

    void StbImageLoaderPlugin::on_detach() {}

    void StbImageLoaderPlugin::on_recieve_message(tbx::Message& msg)
    {
        auto* request = handle_message<LoadTextureRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_texture_request(*request);
    }

    void StbImageLoaderPlugin::set_file_ops(std::shared_ptr<tbx::IFileOps> file_ops)
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

        if (!_file_ops)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Stb image loader: file services unavailable.");
            return;
        }

        TextureSettings load_settings = {
            .wrap = request.wrap,
            .filter = request.filter,
            .format = request.format,
            .mipmaps = request.mipmaps,
            .compression = request.compression,
        };

        auto meta_path = request.path;
        meta_path += ".meta";
        std::string meta_data = {};
        if (_file_ops->read_file(meta_path, FileDataFormat::UTF8_TEXT, meta_data))
        {
            try
            {
                auto data = Json(meta_data);
                auto parsed_settings = TextureSettings();
                if (try_parse_texture_settings(data, parsed_settings))
                    load_settings = parsed_settings;
            }
            catch (...)
            {
                // Ignore meta parsing errors and fall back to request settings.
            }
        }

        std::string encoded_image;
        if (!_file_ops->read_file(request.path, FileDataFormat::BINARY, encoded_image))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, "file could not be read"));
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
                build_load_failure_message(request.path, stbi_failure_reason()));
            return;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        std::vector<Pixel> pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
        tbx::Texture texture(
            resolution,
            load_settings.wrap,
            load_settings.filter,
            load_settings.format,
            load_settings.mipmaps,
            load_settings.compression,
            pixels);
        *asset = texture;

        request.state = MessageState::HANDLED;
    }
}
