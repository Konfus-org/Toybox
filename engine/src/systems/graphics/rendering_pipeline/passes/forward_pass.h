#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/render_pass.h"
#include <memory>

namespace tbx
{
    struct PipelineResources;

    /// @brief
    /// Purpose: Built-in scene pass — the forward (lit) pass. One indirect draw per material bucket,
    /// into the offscreen scene target when post-processing is active or straight to the swapchain
    /// otherwise, sampling the shadow maps through the sample group the shadow pass published.
    class ForwardPass final : public RenderPass
    {
      public:
        ForwardPass(PipelineResources& resources, std::weak_ptr<IGraphicsBackend> backend);
        Result execute(FramePassContext& context) override;

      private:
        PipelineResources& _resources;
        std::weak_ptr<IGraphicsBackend> _backend;
    };
}
