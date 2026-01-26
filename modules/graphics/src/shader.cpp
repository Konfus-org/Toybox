#include "tbx/graphics/shader.h"

namespace tbx
{
    const std::string default_vertex_shader_source = R"(
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

    const std::string default_fragment_shader_source = R"(
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

    namespace detail
    {
        const std::shared_ptr<Shader>& get_standard_vertex_shader()
        {
            static const std::shared_ptr<Shader> shader =
                std::make_shared<Shader>(default_vertex_shader_source, ShaderType::Vertex);
            return shader;
        }

        const std::shared_ptr<Shader>& get_standard_fragment_shader()
        {
            static const std::shared_ptr<Shader> shader =
                std::make_shared<Shader>(default_fragment_shader_source, ShaderType::Fragment);
            return shader;
        }
    }
}
