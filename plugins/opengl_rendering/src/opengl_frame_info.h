#pragma once
#include "opengl_material.h"
#include "opengl_render_target.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include <span>
#include <unordered_set>
#include <vector>

namespace tbx::plugins
{
    /// <summary>Per-frame lighting data used by the OpenGL renderer.</summary>
    /// <remarks>
    /// Purpose: Stores the prepared light uniforms and tracks which programs already received them.
    /// Ownership: Owns uniform storage; does not own any external resources.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    struct OpenGlFrameLightingInfo
    {
        std::vector<ShaderUniform> uniforms = {};
        int point_light_count = 0;
        int area_light_count = 0;
        int spot_light_count = 0;
        int directional_light_count = 0;
        bool has_directional_shadow = false;
        Mat4 directional_shadow_matrix = Mat4(1.0f);
        uint32 directional_shadow_texture = 0U;
        std::unordered_set<uint32> uploaded_program_ids = {};
    };

    /// <summary>Single mesh draw request for a frame camera.</summary>
    /// <remarks>
    /// Purpose: Captures the minimal inputs needed to draw one mesh with a material.
    /// Ownership: Non-owning pointer to a cached material; valid for the frame.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    struct OpenGlFrameMeshDrawRequest
    {
        Uuid mesh_id = {};
        const OpenGlMaterial* material = nullptr;
        std::span<const ShaderUniform> material_overrides = {};
        Mat4 model_matrix = Mat4(1.0f);
    };

    /// <summary>Per-camera render data for one frame.</summary>
    /// <remarks>
    /// Purpose: Holds view/projection data and the list of draw requests visible to this camera.
    /// Ownership: Owns draw request storage; does not own referenced cached resources.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    struct OpenGlFrameCameraInfo
    {
        Mat4 view_projection = Mat4(1.0f);
        Vec3 position = Vec3(0.0f);
        std::vector<OpenGlFrameMeshDrawRequest> draw_requests = {};
        std::unordered_set<uint32> uploaded_program_ids = {};
    };

    /// <summary>Aggregated per-window frame render inputs for the OpenGL renderer.</summary>
    /// <remarks>
    /// Purpose: Centralizes all data required to render a frame (target, lighting, cameras, requests).
    /// Ownership: Owns per-frame storage; does not own the render target pointer.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    struct OpenGlFrameBufferInfo
    {
        Uuid window_id = {};
        Size window_size = {0U, 0U};
        Size render_resolution = {1U, 1U};
        OpenGlRenderTarget* render_target = nullptr;
        OpenGlFrameLightingInfo lighting = {};
        std::vector<OpenGlFrameCameraInfo> cameras = {};
    };
}
