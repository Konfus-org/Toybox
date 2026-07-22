#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gpu/shader_source.h"
#include <utility>
#include <vector>


namespace tbx::gpu
{
    /// @brief
    /// Purpose: Full-screen post processing: just a list of fragment shaders, applied to the
    /// rendered scene in order. Each shader samples u_scene (plus u_resolution and u_time).
    /// One per sandbox (the first wins).
    struct TBX_API PostProcessing : Block
    {
        std::vector<assets::Handle<ShaderSource>> shaders = {};

        // Fluent setters — each returns *this for one-chain construction. add_shader appends one
        // stage so the whole chain reads as PostProcessing{}.add_shader(a).add_shader(b).
        PostProcessing& set_shaders(std::vector<assets::Handle<ShaderSource>> value) { shaders = std::move(value); return *this; }
        PostProcessing& add_shader(assets::Handle<ShaderSource> value) { shaders.push_back(std::move(value)); return *this; }
    };
}
