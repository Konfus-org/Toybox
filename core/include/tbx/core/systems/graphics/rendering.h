#pragma once
#include "tbx/core/utils/result.h"
#include "tbx/core/systems/graphics/camera.h"
#include "tbx/core/interfaces/graphics_backend.h"
#include "tbx/core/systems/graphics/render_pipeline.h"
#include "tbx/core/systems/graphics/settings.h"
#include "tbx/core/interfaces/window_manager.h"
#include "tbx/core/systems/math/size.h"
#include "tbx/core/tbx_api.h"
#include <memory>
#include <vector>

namespace tbx
{
    struct TBX_API RenderViewSubmission
    {
        Window output_window = {};
        Camera camera = {};
        Size resolution = {};
    };

    /// @brief
    /// Purpose: Coordinates Toybox-side render submission against a graphics backend.
    /// @details
    /// Ownership: Owns the Toybox render pipeline; does not own backend resources.
    /// Thread Safety: Not inherently thread-safe; callers should synchronize backend access.
    class TBX_API Rendering
    {
      public:
        Result initialize(IGraphicsBackend& backend, const GraphicsSettings& settings);
        void setup_default_pipeline();

        Result submit(IGraphicsBackend& backend, const RenderViewSubmission& view) const;
        Result submit(IGraphicsBackend& backend, const std::vector<RenderViewSubmission>& views) const;

        const GraphicsRenderPipeline& get_pipeline() const;

      private:
        void rebuild_pipeline(IGraphicsBackend& backend);

      private:
        std::unique_ptr<GraphicsRenderPipeline> _pipeline = nullptr;
        IGraphicsBackend* _backend = nullptr;
        bool _is_initialized = false;
    };
}
