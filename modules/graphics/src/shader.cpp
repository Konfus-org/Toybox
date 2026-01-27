#include "tbx/graphics/shader.h"

namespace tbx
{
    const std::shared_ptr<Shader>& get_standard_vertex_shader()
    {
        static const std::shared_ptr<Shader> shader =
            std::make_shared<Shader>(standard_vertex_shader_source, ShaderType::Vertex);
        return shader;
    }

    const std::shared_ptr<Shader>& get_standard_fragment_shader()
    {
        static const std::shared_ptr<Shader> shader =
            std::make_shared<Shader>(standard_fragment_shader_source, ShaderType::Fragment);
        return shader;
    }
}
