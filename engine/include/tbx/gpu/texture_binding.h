#pragma once
#include "tbx/api.h"
#include "tbx/gpu/depth_target.h"
#include "tbx/gpu/render_target.h"
#include "tbx/gpu/texture2d.h"
#include <functional>
#include <variant>

namespace tbx
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
