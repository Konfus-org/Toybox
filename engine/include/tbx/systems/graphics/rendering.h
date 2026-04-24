#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/render_pipeline.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/math/size.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <optional>
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
        Rendering() = default;
        ~Rendering() noexcept = default;

      public:
        const GraphicsRenderPipeline& get_pipeline() const;
        Result initialize(IGraphicsBackend& backend, const GraphicsSettings& settings);
        Result submit(IGraphicsBackend& backend, const RenderViewSubmission& view) const;
        Result submit(IGraphicsBackend& backend, const std::vector<RenderViewSubmission>& views)
            const;

      private:
        void rebuild_pipeline(IGraphicsBackend& backend);
        void setup_default_pipeline();

      private:
        std::unique_ptr<GraphicsRenderPipeline> _pipeline = nullptr;
        std::optional<std::reference_wrapper<IGraphicsBackend>> _backend = std::nullopt;
        bool _is_initialized = false;
    };
}
