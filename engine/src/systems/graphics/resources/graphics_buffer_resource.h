#pragma once
#include "graphics_resource_requests.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include <any>
#include <memory>
#include <vector>

namespace tbx::detail
{
    class GraphicsBufferResource final : public GraphicsResource
    {
      public:
        GraphicsBufferResource(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsBufferDesc desc,
            std::vector<uint8> upload_data);
        ~GraphicsBufferResource() noexcept override;

      public:
        void update(const std::any& data) override;

      private:
        bool create_resource();

      private:
        GraphicsBufferDesc _desc = {};
        std::vector<uint8> _upload_data = {};
    };
}
