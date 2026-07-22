#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/utils/api.h"
#include <vector>

namespace tbx::gfx
{
    /// @brief
    /// Purpose: Full-screen post processing: just a list of fragment shaders, applied to the
    /// rendered scene in order. Each shader samples u_scene (plus u_resolution and u_time).
    /// One per sandbox (the first wins).
    struct TBX_API PostProcessing : ecs::Block
    {
        std::vector<assets::AssetHandle<ShaderSource>> shaders = {};
    };
}
