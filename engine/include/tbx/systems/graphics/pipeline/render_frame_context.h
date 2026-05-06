#pragma once
#include "tbx/systems/graphics/render_graph.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/math/matrices.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>

namespace tbx
{
    class IGraphicsBackend;

    /// @brief
    /// Purpose: Per-frame shared data passed through the prepare phase of all render operations.
    /// @details
    /// Input fields are set by the scheduler before the first prepare() call.
    /// Output fields are written by operations during prepare() and consumed by subsequent
    /// operations. All Uuid outputs default to invalid; consumers must validate before use.
    struct TBX_API RenderFrameContext
    {
        // Inputs — set by the scheduler before prepare begins
        std::reference_wrapper<IGraphicsBackend> backend;
        std::reference_wrapper<GraphicsResourceManager> resource_manager;
        std::reference_wrapper<const RenderGraph> render_graph;
        Mat4 view_projection = Mat4(1.0F);
        Vec3 camera_position = {};
        uint64 frame_index = 0U;

        // Outputs — written by operations during prepare(), consumed by later operations
        Uuid view_uniform_buffer = {};
        bool has_skybox = false;
    };
}
