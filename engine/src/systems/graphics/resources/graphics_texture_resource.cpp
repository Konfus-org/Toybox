#include "graphics_texture_resource.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <utility>

namespace tbx::detail
{
    GraphicsTextureResource::GraphicsTextureResource(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsTextureDesc desc,
        std::vector<uint8> upload_data)
        : GraphicsResource(std::move(backend))
        , _desc(std::move(desc))
        , _upload_data(std::move(upload_data))
    {
        if (!create_resource())
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed to create resource.",
                to_string(get_uuid()));
    }

    GraphicsTextureResource::GraphicsTextureResource(
        std::weak_ptr<IGraphicsBackend> backend,
        const Handle& handle,
        const Texture& texture)
        : GraphicsResource(std::move(backend))
    {
        if (texture.resolution.width == 0U || texture.resolution.height == 0U)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}): texture size is invalid for handle '{}'.",
                to_string(get_uuid()),
                to_string(handle));
            return;
        }

        const uint64 source_byte_size = get_texture_source_byte_size(texture);
        if (source_byte_size > 0U && static_cast<uint64>(texture.pixels.size()) < source_byte_size)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}): texture pixel data is smaller than expected.",
                to_string(get_uuid()));
            return;
        }

        _desc = make_texture_desc(texture, handle);
        _upload_data = make_upload_data(texture);
        if (!create_resource())
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed to create resource from texture handle '{}'.",
                to_string(get_uuid()),
                to_string(handle));
    }

    void GraphicsTextureResource::update(const std::any& data)
    {
        const auto request = std::any_cast<GraphicsTextureUpdateRequest>(&data);
        if (request == nullptr)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed to apply invalid update payload.",
                to_string(get_uuid()));
            return;
        }

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed: graphics backend is unavailable.",
                to_string(get_uuid()));
            return;
        }

        const Result result = backend->update_texture(
            get_uuid(),
            request->desc,
            request->data,
            request->data_size);
        if (!result)
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) update failed: {}",
                to_string(get_uuid()),
                result.get_report());
    }

    GraphicsTextureResource::~GraphicsTextureResource() noexcept
    {
        if (!get_uuid().is_valid())
            return;

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_WARNING_ONCE(
                "Graphics texture resource ({}) failed to unload: graphics backend is unavailable.",
                to_string(get_uuid()));
            return;
        }

        const Result result = backend->unload(get_uuid());
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed to unload: {}",
                to_string(get_uuid()),
                result.get_report());
        }
    }

    bool GraphicsTextureResource::create_resource()
    {
        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics texture resource ({}) failed to create: graphics backend is unavailable.",
                to_string(get_uuid()));
            return false;
        }

        const void* upload_data = _upload_data.empty() ? nullptr : _upload_data.data();
        auto resource_uuid = Uuid {};
        const Result result = backend->upload_texture(
            _desc,
            upload_data,
            static_cast<uint64>(_upload_data.size()),
            resource_uuid);
        if (result)
        {
            set_uuid(resource_uuid);
            return true;
        }

        TBX_TRACE_ERROR_ONCE(
            "Graphics texture resource ({}) create failed: {}",
            to_string(resource_uuid),
            result.get_report());
        return false;
    }

    uint64 GraphicsTextureResource::get_texture_channel_count(const TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGB:
                return 3U;
            case TextureFormat::RGBA:
                return 4U;
            default:
                return 4U;
        }
    }

    uint64 GraphicsTextureResource::get_texture_pixel_count(const Texture& texture)
    {
        return static_cast<uint64>(texture.resolution.width)
               * static_cast<uint64>(texture.resolution.height);
    }

    uint64 GraphicsTextureResource::get_texture_source_byte_size(const Texture& texture)
    {
        return get_texture_pixel_count(texture) * get_texture_channel_count(texture.format);
    }

    GraphicsTextureDesc GraphicsTextureResource::make_texture_desc(
        const Texture& texture,
        const Handle& handle)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED,
            .format = GraphicsTextureFormat::RGBA8,
            .size = texture.resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = std::string("Texture ") + to_string(handle),
        };
    }

    std::vector<uint8> GraphicsTextureResource::make_upload_data(const Texture& texture)
    {
        if (texture.pixels.empty())
            return {};

        if (texture.format != TextureFormat::RGB)
            return std::vector<uint8>(texture.pixels.begin(), texture.pixels.end());

        auto upload_data = std::vector<uint8> {};
        upload_data.reserve(static_cast<size>(get_texture_pixel_count(texture) * 4U));
        const uint64 pixel_data_size = static_cast<uint64>(texture.pixels.size());
        for (uint64 source_index = 0U; source_index + 2U < pixel_data_size; source_index += 3U)
        {
            upload_data.push_back(texture.pixels[static_cast<size>(source_index)]);
            upload_data.push_back(texture.pixels[static_cast<size>(source_index + 1U)]);
            upload_data.push_back(texture.pixels[static_cast<size>(source_index + 2U)]);
            upload_data.push_back(static_cast<Pixel>(255U));
        }

        return upload_data;
    }
}

