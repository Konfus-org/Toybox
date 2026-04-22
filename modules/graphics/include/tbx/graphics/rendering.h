#pragma once
#include "tbx/common/result.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/render_pipeline.h"
#include "tbx/graphics/settings.h"
#include "tbx/graphics/window.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
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
        GraphicsRenderPipeline _pipeline = {};
        bool _is_initialized = false;
    };
}
