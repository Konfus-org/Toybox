#pragma once
#include "graphics_resource_requests.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/types/handle.h"
#include "tbx/types/texture.h"
#include <memory>
#include <vector>

namespace tbx::detail
{
    class GraphicsTextureResource final : public GraphicsResource
    {
      public:
        GraphicsTextureResource(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsTextureDesc desc,
            std::vector<uint8> upload_data);
        GraphicsTextureResource(
            std::weak_ptr<IGraphicsBackend> backend,
            const Handle& handle,
            const Texture& texture);
        ~GraphicsTextureResource() noexcept override;

      public:
        void update(const std::any& data) override;

      private:
        bool create_resource();

      private:
        static uint64 get_texture_channel_count(TextureFormat format);
        static uint64 get_texture_pixel_count(const Texture& texture);
        static uint64 get_texture_source_byte_size(const Texture& texture);
        static GraphicsTextureDesc make_texture_desc(const Texture& texture, const Handle& handle);
        static std::vector<uint8> make_upload_data(const Texture& texture);

      private:
        GraphicsTextureDesc _desc = {};
        std::vector<uint8> _upload_data = {};
    };
}
