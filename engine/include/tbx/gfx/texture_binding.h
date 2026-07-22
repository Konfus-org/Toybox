#pragma once
#include "tbx/utils/api.h"
#include "tbx/gfx/depth_target.h"
#include "tbx/gfx/render_target.h"
#include "tbx/gfx/texture2d.h"
#include <functional>
#include <variant>

namespace tbx::gfx
{
    /// @brief
    /// Purpose: One texture bound for one draw, modern-API style (Vulkan/Metal/WebGPU bind
    /// groups): bindings travel WITH the draw call instead of mutating loose slot state.
    struct TBX_API TextureBinding
    {
        int slot = 0;
        std::variant<
            std::reference_wrapper<const Texture2d>,
            std::reference_wrapper<const DepthTarget>,
            std::reference_wrapper<const RenderTarget>>
            texture;
    };
}
