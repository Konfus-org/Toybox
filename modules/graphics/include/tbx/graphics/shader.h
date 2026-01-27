#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <variant>

namespace tbx
{
    using UniformData = std::variant<bool, int, float, Vec2, Vec3, RgbaColor, Mat4>;

    // A uniform variable that can be uploaded to a shader.
    struct TBX_API ShaderUniform
    {
        std::string name = "";
        UniformData data = 0;
    };

    // The type of a shader.
    enum class TBX_API ShaderType
    {
        None,
        Vertex,
        Fragment,
        Compute
    };

    // A shader is a program that runs on the GPU and is responsible for rendering.
    // It consists of a source code and a type.
    struct TBX_API Shader
    {
        Shader() = default;
        Shader(const std::string& source, ShaderType type)
            : source(source)
            , type(type)
        {
        }

        std::string source = "";
        ShaderType type = ShaderType::None;
        Uuid id = Uuid::generate();
    };

    inline const std::string standard_vertex_shader_source = R"(
        #version 450 core

        layout(location = 0) in vec3 in_position;
        layout(location = 1) in vec4 in_vert_color;
        layout(location = 2) in vec3 in_normal; // TODO: implement normals!
        layout(location = 3) in vec2 in_tex_coord;

        out vec4 color;
        out vec4 vert_color;
        out vec2 tex_coord;

        uniform mat4 view_proj_uniform;
        uniform mat4 model_uniform;
        uniform vec4 color_uniform = vec4(1.0, 1.0, 1.0, 1.0);

        void main()
        {
            color = color_uniform;
            vert_color = in_vert_color;
            tex_coord = in_tex_coord;
            gl_Position = view_proj_uniform * model_uniform * vec4(in_position, 1.0);
        }
    )";

    inline const std::string standard_fragment_shader_source = R"(
        #version 450 core

        layout(location = 0) out vec4 out_color;

        in vec4 color;
        in vec4 vert_color;
        in vec3 normal; // TODO: implement normals!
        in vec2 tex_coord;

        uniform sampler2D texture_uniform;

        void main()
        {
            vec4 texture_color = color;
            texture_color *= texture(texture_uniform, tex_coord);
            out_color = texture_color;
        }
    )";

    /// <summary>Purpose: Retrieves the default vertex shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the instance owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const Shader& get_standard_vertex_shader();

    /// <summary>Purpose: Retrieves the default fragment shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the instance owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const Shader& get_standard_fragment_shader();

    /// <summary>Provides the default vertex shader instance.</summary>
    /// <remarks>Purpose: Supplies the default vertex shader for new materials.
    /// Ownership: Returns a reference to the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Shader& standard_vertex_shader = get_standard_vertex_shader();

    /// <summary>Provides the default fragment shader instance.</summary>
    /// <remarks>Purpose: Supplies the default fragment shader for new materials.
    /// Ownership: Returns a reference to the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Shader& standard_fragment_shader = get_standard_fragment_shader();

    /// <summary>Purpose: Provides the default vertex shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Shader& vert_shader = get_standard_vertex_shader();

    /// <summary>Purpose: Provides the default fragment shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Shader& frag_shader = get_standard_fragment_shader();
}
