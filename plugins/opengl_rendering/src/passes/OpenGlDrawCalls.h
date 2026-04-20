#pragma once
#include "opengl_resources/opengl_shader.h"
#include "tbx/graphics/render_pipeline.h"
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

        tbx::Uuid shader_program = {};
        bool is_two_sided = false;
        std::vector<tbx::Uuid> meshes = {};
        std::vector<OpenGlMaterialParams> materials = {};
        std::vector<tbx::Mat4> transforms = {};
    };

    struct ShadowDrawCall
    {
        bool is_two_sided = false;
        std::vector<tbx::Uuid> meshes = {};
        std::vector<tbx::Mat4> transforms = {};
        std::vector<tbx::Vec3> bounds_centers = {};
        std::vector<float> bounds_radii = {};
    };

    struct TransparentDrawCall
    {
        tbx::Uuid shader_program = {};
        bool is_two_sided = false;
        tbx::Uuid mesh = {};
        OpenGlMaterialParams material = {};
        tbx::Mat4 transform = tbx::Mat4(1.0F);
        float camera_distance_squared = 0.0F;
    };
}
