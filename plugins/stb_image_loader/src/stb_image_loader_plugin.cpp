#include "stb_image_loader_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/messages.h"
#include "tbx/files/filesystem.h"
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

    void StbImageAssetLoaderPlugin::on_attach(Application& host)
    {
        _filesystem = &host.get_filesystem();
    }

    void StbImageAssetLoaderPlugin::on_detach()
    {
        _filesystem = nullptr;
    }

    void StbImageAssetLoaderPlugin::on_recieve_message(Message& msg)
    {
        on_message(
            msg,
            [this](LoadTextureRequest& request)
            {
                on_load_texture_request(request);
            });
    }

    void StbImageAssetLoaderPlugin::on_load_texture_request(LoadTextureRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Stb image loader: missing texture asset wrapper.");
            return;
        }

        const bool has_payload = asset->read(
            [](const Texture* payload)
            {
                return payload != nullptr;
            });
        if (!has_payload)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Stb image loader: missing texture payload.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::Cancelled;
            request.result.flag_failure("Stb image loader cancelled.");
            return;
        }

        if (!_filesystem)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Stb image loader: filesystem unavailable.");
            return;
        }

        stbi_set_flip_vertically_on_load(true);
        const std::filesystem::path resolved = resolve_asset_path(request.path);
        const std::string resolved_string = resolved.string();
        int width = 0;
        int height = 0;
        const int desired_channels = (request.format == TextureFormat::RGB) ? 3 : 4;
        stbi_uc* raw_data =
            stbi_load(resolved_string.c_str(), &width, &height, nullptr, desired_channels);
        if (!raw_data)
        {
            request.state = MessageState::Error;
            request.result.flag_failure(
                build_load_failure_message(resolved, stbi_failure_reason()));
            return;
        }

        const auto pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height)
                                 * static_cast<size_t>(desired_channels);
        std::vector<Pixel> pixels(raw_data, raw_data + pixel_count);
        stbi_image_free(raw_data);

        asset->write(
            [&](Texture* payload)
            {
                if (!payload)
                {
                    return;
                }

                const Uuid existing_id = payload->id;
                Size resolution = {static_cast<uint32>(width), static_cast<uint32>(height)};
                Texture texture(resolution, request.wrap, request.filter, request.format, pixels);
                texture.id = existing_id;
                *payload = texture;
            });
        request.state = MessageState::Handled;
        request.result.flag_success();
    }

    std::filesystem::path StbImageAssetLoaderPlugin::resolve_asset_path(
        const std::filesystem::path& path) const
    {
        if (path.is_absolute() || !_filesystem)
        {
            return path;
        }

        return _filesystem->get_assets_directory() / path;
    }
}
