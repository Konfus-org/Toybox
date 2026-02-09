#include "stb_image_loader_plugin.h"
#include "tbx/assets/messages.h"
#include "tbx/graphics/texture.h"
#include <stb_image.h>
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

        stbi_set_flip_vertically_on_load(true);
        const std::string texture_path = request.path.string();
        int width = 0;
        int height = 0;
        const int desired_channels = (request.format == TextureFormat::RGB) ? 3 : 4;
        stbi_uc* raw_data =
            stbi_load(texture_path.c_str(), &width, &height, nullptr, desired_channels);
        if (!raw_data)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(texture_path, stbi_failure_reason()));
            return;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        std::vector<Pixel> pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
        Texture texture(resolution, request.wrap, request.filter, request.format, pixels);
        *asset = texture;

        request.state = MessageState::HANDLED;
    }
}
