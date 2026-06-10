#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/viewport.h"
#include "tbx/types/window.h"
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores the last buffer resource bound to one indexed GL binding point.
    struct OpenGlBufferSlotBinding
    {
        tbx::GpuId resource = tbx::INVALID_GPU_ID;
        uint64 offset = 0U;
        uint64 range = 0U;
    };

    /// @brief
    /// Purpose: Stores the last raster state applied to OpenGL.
    struct OpenGlPipelineState
    {
        tbx::GpuId id = tbx::INVALID_GPU_ID;
        tbx::MaterialDepthFunction depth_function = tbx::MaterialDepthFunction::LESS;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_blending_enabled = false;
        bool is_culling_enabled = true;
        float depth_bias_constant = 0.0F;
        float depth_bias_slope = 0.0F;
        tbx::CullMode cull_mode = tbx::CullMode::BACK;
        tbx::BlendEquation blend_equation = tbx::BlendEquation::ALPHA;
    };

    /// @brief
    /// Purpose: Tracks backend-side GL state caches.
    struct OpenGlState
    {
        tbx::Window current_target = {};
        tbx::Viewport current_viewport = {};
        OpenGlPipelineState current_pipeline_state = {};

        tbx::VsyncMode vsync_mode = tbx::VsyncMode::OFF;
        int32 max_uniform_buffer_bindings = -1;

        std::vector<tbx::GpuId> bound_samplers = {};
        std::vector<tbx::GpuId> bound_sampled_textures = {};
        std::vector<tbx::GpuId> bound_vertex_buffers = {};
        std::vector<OpenGlBufferSlotBinding> bound_storage_buffers = {};
        std::vector<OpenGlBufferSlotBinding> bound_uniform_buffers = {};
        tbx::GpuId bound_index_buffer = tbx::INVALID_GPU_ID;

        bool is_loaded = false;
        bool is_render_pass_active = false;
        bool has_current_pipeline_state = false;
    };
}
