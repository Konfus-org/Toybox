#pragma once
#include "tbx/core/color.h"
#include "tbx/core/typedefs.h"
#include "tbx/gfx/depth_target.h"
#include "tbx/gfx/render_target.h"
#include <functional>
#include <optional>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: What happens to an attachment's existing contents when a pass begins.
    enum class LoadOperation : uint8
    {
        KEEP,
        CLEAR
    };

    /// @brief
    /// Purpose: One render pass, modern-API style (Vulkan/Metal/WebGPU): attachments plus
    /// their load operation, explicit begin/end. With no target set the pass renders to the
    /// window swapchain; a color_target renders offscreen; a depth_target is a depth-only
    /// pass (shadow maps).
    struct RenderPassDescription
    {
        std::optional<std::reference_wrapper<const RenderTarget>> color_target = {};
        std::optional<std::reference_wrapper<const DepthTarget>> depth_target = {};
        LoadOperation load = LoadOperation::KEEP;
        Color clear_color = {};
    };
}
