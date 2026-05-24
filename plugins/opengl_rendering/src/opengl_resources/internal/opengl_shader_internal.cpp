#include "opengl_shader_internal.h"

namespace opengl_rendering::internal
{
    tbx::Result create_shaders(
        const tbx::ShaderProgram& shader_desc,
        std::vector<std::shared_ptr<OpenGlShader>>& out_shaders)
    {
        auto result = tbx::Result {};
        if (shader_desc.sources.empty())
        {
            result.flag_failure("OpenGL backend: pipeline has no shader sources.");
            return result;
        }

        out_shaders.reserve(shader_desc.sources.size());
        for (const auto& source : shader_desc.sources)
        {
            auto shader = std::make_shared<OpenGlShader>(source);
            if (!shader->compile())
            {
                auto message = std::string("OpenGL backend: shader compilation failed.");
                if (!shader->get_last_error().empty())
                {
                    message += " ";
                    message += shader->get_last_error();
                }

                result.flag_failure(std::move(message));
                return result;
            }

            out_shaders.push_back(std::move(shader));
        }

        result.flag_success();
        return result;
    }
}
