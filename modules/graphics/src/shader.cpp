#include "tbx/graphics/shader.h"

namespace tbx
{
    const Shader& get_standard_vertex_shader()
    {
        static const Shader shader = Shader(standard_vertex_shader_source, ShaderType::Vertex);
        return shader;
    }

    const Shader& get_standard_fragment_shader()
    {
        static const Shader shader = Shader(standard_fragment_shader_source, ShaderType::Fragment);
        return shader;
    }
}
