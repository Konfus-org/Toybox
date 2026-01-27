#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// A collection of shaders.
    /// </summary>
    struct TBX_API ShaderProgram
    {
        ShaderProgram() = default;
        explicit ShaderProgram(Shader shader)
            : shaders({std::move(shader)})
        {
        }
        explicit ShaderProgram(std::vector<Shader> shaders)
            : shaders(std::move(shaders))
        {
        }

        std::vector<Shader> shaders = {
            standard_fragment_shader,
            standard_vertex_shader,
        };
        Uuid id = Uuid::generate();
    };

    /// <summary>Purpose: Retrieves the shared default shader program instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default program owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const ShaderProgram& get_standard_shader_program();

    /// <summary>Purpose: Provides the shared default shader program instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default shader program instance.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const ShaderProgram& standard_shader_program = get_standard_shader_program();

    /// <summary>
    /// A shader program with textures.
    /// </summary>
    struct TBX_API Material
    {
        Material() = default;
        Material(std::vector<Shader> shaders)
            : shader_program(ShaderProgram(std::move(shaders)))
        {
        }
        Material(std::vector<Shader> shaders, std::shared_ptr<Texture> texture)
            : shader_program(ShaderProgram(std::move(shaders)))
            , textures({std::move(texture)})
        {
        }
        Material(
            std::vector<Shader> shaders,
            std::vector<std::shared_ptr<Texture>> textures)
            : shader_program(ShaderProgram(shaders))
            , textures(textures)
        {
        }
        Material(
            RgbaColor rgba_color,
            std::vector<Shader> shaders,
            std::vector<std::shared_ptr<Texture>> textures)
            : color(rgba_color)
            , shader_program(ShaderProgram(shaders))
            , textures(textures)
        {
        }

        RgbaColor color = {255, 255, 255, 255};
        ShaderProgram shader_program = get_standard_shader_program();
        std::vector<std::shared_ptr<Texture>> textures = {};
        Uuid id = Uuid::generate();
    };


}
