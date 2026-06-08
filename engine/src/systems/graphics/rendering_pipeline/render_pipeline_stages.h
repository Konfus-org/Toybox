#pragma once
#include "render_pipeline_context.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/types/assets/world.h"
#include "tbx/utils/result.h"
#include <cstdint>

namespace tbx
{
    /// @brief
    /// Purpose: Identifies reusable GPU resource transition points between render stages.
    enum class GpuBarrierPoint : uint8_t
    {
        VISIBILITY_TO_RASTER,
        SHADOW_TO_GBUFFER,
        GBUFFER_TO_LIGHTING,
        LIGHTING_TO_PRESENT,
    };

    /// @brief
    /// Purpose: Uploads renderable world data and prepares per-frame GPU resources.
    class FrameUploadStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            World& world,
            const Window& output,
            IWindowManager& window_manager,
            const GraphicsSettings& settings,
            float elapsed_time,
            UploadedFrameResources& resources);
    };

    /// @brief
    /// Purpose: Runs GPU visibility culling work for the current frame.
    class VisibilityCullingStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            const UploadedFrameResources& resources);
    };

    /// @brief
    /// Purpose: Applies reusable GPU resource transitions between render stages.
    class GpuBarrierStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            const FrameBuffers& buffers,
            GpuBarrierPoint point);
    };

    /// @brief
    /// Purpose: Renders shadow-casting draw batches into the shadow atlas.
    class ShadowAtlasStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            const UploadedFrameResources& resources);
    };

    /// @brief
    /// Purpose: Draws visible renderable batches into gbuffer attachments.
    class GBufferStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            const UploadedFrameResources& resources);
    };

    /// @brief
    /// Purpose: Resolves lighting into the final HDR render target.
    class LightingResolveStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            const UploadedFrameResources& resources);
    };

    /// @brief
    /// Purpose: Presents the final frame texture and failure fallback frame.
    class PresentStage final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            RenderPipelineContext& context,
            AssetManager& assets,
            const UploadedFrameResources& resources);
        Result execute_failure_frame(IGraphicsBackend& backend);
    };
}
