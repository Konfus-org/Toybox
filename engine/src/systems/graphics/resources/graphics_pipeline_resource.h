#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include <memory>

namespace tbx::detail
{
    class GraphicsPipelineResource final : public GraphicsResource
    {
      public:
        GraphicsPipelineResource(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsPipelineDesc desc);
        ~GraphicsPipelineResource() noexcept override;

      private:
        bool create_resource();

      private:
        GraphicsPipelineDesc _desc = {};
    };
}
