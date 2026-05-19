#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_pass.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/size.h"
#include "tbx/types/uuid.h"
#include "tbx/types/window.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores all renderer-owned data needed to submit one frame.
    /// @details
    /// Ownership: Value type built per-frame. Backend resources referenced by UUID remain owned by
    /// GraphicsResourceManager and the active graphics backend.
    /// Thread Safety: Intended for one render-lane submission at a time.
    struct TBX_API FrameData
    {
        Window output_window = {};
        Size render_resolution = {};
        Size output_resolution = {};
        GraphicsView view = {};
        uint64 frame_index = 0U;

        std::vector<GraphicsRenderPass> skybox_passes = {};
        std::vector<GraphicsRenderPass> opaque_passes = {};
        std::vector<GraphicsRenderPass> alpha_cutout_passes = {};
        std::vector<GraphicsRenderPass> lighting_passes = {};
        std::vector<GraphicsRenderPass> transparent_passes = {};
        std::vector<GraphicsRenderPass> post_process_passes = {};
    };

    using RenderData = FrameData;
}
