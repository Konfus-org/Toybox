#pragma once
#include "../post_processor.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/render_pass.h"
#include <memory>

namespace tbx
{
    struct PipelineResources;

    /// @brief
    /// Purpose: Built-in post pass — ensures its offscreen targets in prepare, then in execute renders
    /// the tag mask (when a tag-gated effect is active) and runs the post-processing chain (the world's
    /// own PostProcessing effects) compositing the scene to the swapchain.
    class PostPass final : public RenderPass
    {
      public:
        PostPass(
            PipelineResources& resources,
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager);
        Result prepare(FramePassContext& context) override;
        Result execute(FramePassContext& context) override;

      private:
        // Draws the tag-masked entities flat into the tag mask the post chain samples. Run by execute
        // only when a tag-gated effect is active this frame.
        Result render_tag_mask(FramePassContext& context);

        PipelineResources& _resources;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<AssetManager> _asset_manager;
        // The post processor — owns the offscreen scene/tag-mask targets + the effect chain. Freed
        // (RAII) when this pass is destroyed.
        PostProcessor _post;
    };
}
