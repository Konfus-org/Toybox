#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/viewport.h"
#include "tbx/types/window.h"
#include <unordered_map>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores the last buffer resource bound to one indexed GL binding point.
    struct OpenGlBufferSlotBinding
    {
        tbx::Uuid resource = {};
        uint64 offset = 0U;
        uint64 range = 0U;
    };

    /// @brief
    /// Purpose: Stores the last raster state applied to OpenGL.
    struct OpenGlPipelineState
    {
        tbx::Uuid id = {};
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_blending_enabled = false;
        bool is_culling_enabled = true;
        tbx::GraphicsCullMode cull_mode = tbx::GraphicsCullMode::BACK;
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

        std::unordered_map<uint32, tbx::Uuid> bound_samplers = {};
        std::unordered_map<uint32, tbx::Uuid> bound_sampled_textures = {};
        std::unordered_map<uint32, tbx::Uuid> bound_image_textures = {};
        std::unordered_map<uint32, tbx::Uuid> bound_vertex_buffers = {};
        std::unordered_map<uint32, OpenGlBufferSlotBinding> bound_storage_buffers = {};
        std::unordered_map<uint32, OpenGlBufferSlotBinding> bound_uniform_buffers = {};
        tbx::Uuid bound_index_buffer = {};

        bool is_loaded = false;
        bool is_compute_pass_active = false;
        bool is_render_pass_active = false;
        bool has_current_pipeline_state = false;
    };
}
