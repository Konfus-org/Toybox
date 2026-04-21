#pragma once
#include "tbx/common/result.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/render_graph.h"
#include "tbx/graphics/window.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Coordinates Toybox-side render submission against a graphics backend.
    /// @details
    /// Ownership: Does not own the backend or scene data; backends own realized GPU state.
    /// Thread Safety: Not inherently thread-safe; callers should synchronize backend access.
    class TBX_API Rendering
    {
      public:
        /// @brief
        /// Purpose: Opens a frame and presents it using the command-based backend contract.
        /// @details
        /// Ownership: Reads submission data during the call. Render passes issue explicit backend
        /// commands separately.
        /// Thread Safety: Not inherently thread-safe; callers should synchronize backend access.
        Result submit(
            IGraphicsBackend& backend,
            const Window& output_window,
            const Camera& view,
            const Size& resolution,
            const RenderGraph& scene) const;
    };
}
