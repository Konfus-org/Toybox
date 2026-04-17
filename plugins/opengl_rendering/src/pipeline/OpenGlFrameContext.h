#pragma once
#include "opengl_resources/opengl_shader.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/graphics_settings.h"
#include "tbx/graphics/post_processing.h"
#include "tbx/math/matrices.h"
#include <vector>

namespace opengl_rendering
{
    struct DrawCall
    {
        DrawCall(tbx::Uuid shader_program, const bool is_two_sided)
            : shader_program(shader_program)
            , is_two_sided(is_two_sided)
        {
        }

        tbx::Uuid shader_program;
        bool is_two_sided = false;
        std::vector<tbx::Uuid> meshes;
        std::vector<OpenGlMaterialParams> materials;
        std::vector<tbx::Mat4> transforms;
    };

    /// @brief
    /// Purpose: Captures one mesh batch for directional shadow-map rendering.
    /// @details
    /// Ownership: Stores mesh handles and transforms by value for one frame.
    /// Thread Safety: Safe to move between threads; render-thread mutation only.
    struct ShadowDrawCall
    {
        bool is_two_sided = false;
        std::vector<tbx::Uuid> meshes;
        std::vector<tbx::Mat4> transforms;
        std::vector<tbx::Vec3> bounds_centers;
        std::vector<float> bounds_radii;
    };

    /// @brief
    /// Purpose: Captures one transparent surface draw submitted after deferred lighting.
    /// @details
    /// Ownership: Stores mesh, transform, and material data by value for one frame.
    /// Thread Safety: Safe to move between threads; render-thread mutation only.
    struct TransparentDrawCall
    {
        tbx::Uuid shader_program = {};
        bool is_two_sided = false;
        tbx::Uuid mesh = {};
        OpenGlMaterialParams material = {};
        tbx::Mat4 transform = tbx::Mat4(1.0F);
        float camera_distance_squared = 0.0F;
    };

    /// @brief
    /// Purpose: Captures one directional light in the render-thread frame payload.
    /// @details
    /// Ownership: Value type copied into the frame context.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct DirectionalLightFrameData
    {
        tbx::Vec3 direction = tbx::Vec3(0.0F, 0.0F, -1.0F);
        float ambient_intensity = 0.03F;
        tbx::Vec3 radiance = tbx::Vec3(1.0F, 1.0F, 1.0F);
        float casts_shadows = 0.0F;
        uint32 shadow_cascade_offset = 0U;
        uint32 shadow_cascade_count = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    /// @brief
    /// Purpose: Stores one stabilized directional shadow cascade for the lighting pass.
    /// @details
    /// Ownership: Value type copied into the frame context and GPU upload payloads.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct ShadowCascadeFrameData
    {
        tbx::Mat4 light_view_projection = tbx::Mat4(1.0F);
        float split_depth = 0.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        float blend_distance = 0.0F;
        uint32 texture_layer = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    /// @brief
    /// Purpose: Stores one projected local-light shadow map for spot and area lights.
    /// @details
    /// Ownership: Value type copied into the frame context and GPU upload payloads.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct ProjectedShadowFrameData
    {
        tbx::Mat4 light_view_projection = tbx::Mat4(1.0F);
        float near_plane = 0.1F;
        float far_plane = 10.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        uint32 texture_layer = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
        float padding2 = 0.0F;
    };

    /// @brief
    /// Purpose: Stores directional shadow-map state consumed by render passes for one frame.
    /// @details
    /// Ownership: Owns cascade values by copy and references no external resources.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct ShadowFrameData
    {
        uint32 directional_map_resolution = 2048U;
        uint32 local_map_resolution = 1024U;
        uint32 point_map_resolution = 1024U;
        float softness = 1.0F;
        float max_distance = 90.0F;
        std::vector<ShadowCascadeFrameData> directional_cascades = {};
        std::vector<ProjectedShadowFrameData> spot_maps = {};
        std::vector<ProjectedShadowFrameData> area_maps = {};
    };

    /// @brief
    /// Purpose: Captures one point light in the render-thread frame payload.
    /// @details
    /// Ownership: Value type copied into the frame context.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct PointLightFrameData
    {
        tbx::Vec3 position = tbx::Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        tbx::Vec3 radiance = tbx::Vec3(1.0F, 1.0F, 1.0F);
        float shadow_bias = 0.00035F;
        int shadow_index = -1;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    /// @brief
    /// Purpose: Captures one spot light in the render-thread frame payload.
    /// @details
    /// Ownership: Value type copied into the frame context.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct SpotLightFrameData
    {
        tbx::Vec3 position = tbx::Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        tbx::Vec3 direction = tbx::Vec3(0.0F, 0.0F, -1.0F);
        float inner_cos = 0.93969262F;
        tbx::Vec3 radiance = tbx::Vec3(1.0F, 1.0F, 1.0F);
        float outer_cos = 0.81915206F;
        int shadow_index = -1;
        float shadow_bias = 0.00035F;
        float padding0 = 0.0F;
    };

    /// @brief
    /// Purpose: Captures one rectangular area light in the render-thread frame payload.
    /// @details
    /// Ownership: Value type copied into the frame context.
    /// Thread Safety: Safe to copy between threads; render-thread mutation only.
    struct AreaLightFrameData
    {
        tbx::Vec3 position = tbx::Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        tbx::Vec3 direction = tbx::Vec3(0.0F, 0.0F, -1.0F);
        float half_width = 0.5F;
        tbx::Vec3 radiance = tbx::Vec3(1.0F, 1.0F, 1.0F);
        float half_height = 0.5F;
        tbx::Vec3 right = tbx::Vec3(1.0F, 0.0F, 0.0F);
        float shadow_bias = 0.00045F;
        tbx::Vec3 up = tbx::Vec3(0.0F, 1.0F, 0.0F);
        int shadow_index = -1;
    };

    struct OpenGlFrameContext
    {
        tbx::Color clear_color = tbx::Color::BLACK;
        tbx::Size render_size = {0U, 0U};
        bool has_camera = false;
        tbx::Vec3 camera_position = tbx::Vec3(0.0F, 0.0F, 0.0F);
        float camera_near_plane = 0.1F;
        float camera_far_plane = 1000.0F;
        bool is_camera_perspective = true;
        float camera_vertical_fov_degrees = 60.0F;
        float camera_aspect = 1.7777778F;
        tbx::Mat4 view_matrix = tbx::Mat4(1.0F);
        tbx::Mat4 projection_matrix = tbx::Mat4(1.0F);
        tbx::Mat4 view_projection = tbx::Mat4(1.0F);
        tbx::Mat4 inverse_view_projection = tbx::Mat4(1.0F);
        std::vector<DrawCall> draw_calls;
        std::vector<ShadowDrawCall> shadow_draw_calls;
        std::vector<TransparentDrawCall> transparent_draw_calls;
        std::vector<DirectionalLightFrameData> directional_lights;
        std::vector<PointLightFrameData> point_lights;
        std::vector<SpotLightFrameData> spot_lights;
        std::vector<AreaLightFrameData> area_lights;
        ShadowFrameData shadows = {};
        bool has_post_processing = false;
        tbx::PostProcessing post_processing = {};
        tbx::RenderStage render_stage = tbx::RenderStage::FINAL_COLOR;
    };
}
