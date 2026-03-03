#pragma once
#include "opengl_resources/opengl_shader.h"
#include "tbx/common/uuid.h"
#include "tbx/math/matrices.h"
#include <vector>

namespace opengl_rendering
{
    struct DrawCall
    {
        tbx::Uuid shader_program;
        std::vector<tbx::Uuid> meshes;
        std::vector<OpenGlMaterialParams> materials;
        std::vector<tbx::Mat4> transforms;
    };

    struct OpenGlFrameContext
    {
        tbx::Color clear_color;
        tbx::Mat4 view_projection;
        std::vector<DrawCall> draw_calls;
    };
}
