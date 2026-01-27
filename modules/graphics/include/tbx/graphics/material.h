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
        explicit ShaderProgram(std::shared_ptr<Shader> shader)
            : shaders({std::move(shader)})
        {
        }
        explicit ShaderProgram(std::vector<std::shared_ptr<Shader>> shaders)
            : shaders(std::move(shaders))
        {
        }

        std::vector<std::shared_ptr<Shader>> shaders = {
            standard_fragment_shader,
            standard_vertex_shader,
        };
        Uuid id = Uuid::generate();
    };

    /// <summary>
    /// A shader program with textures.
    /// </summary>
    struct TBX_API Material
    {
        Material() = default;
        Material(std::vector<std::shared_ptr<Shader>> shaders)
            : shader_program(ShaderProgram(std::move(shaders)))
        {
        }
        Material(std::vector<std::shared_ptr<Shader>> shaders, std::shared_ptr<Texture> texture)
            : shader_program(ShaderProgram(std::move(shaders)))
            , textures({std::move(texture)})
        {
        }
        Material(
            std::vector<std::shared_ptr<Shader>> shaders,
            std::vector<std::shared_ptr<Texture>> textures)
            : shader_program(ShaderProgram(shaders))
            , textures(textures)
        {
        }
        Material(
            RgbaColor rgba_color,
            std::vector<std::shared_ptr<Shader>> shaders,
            std::vector<std::shared_ptr<Texture>> textures)
            : color(rgba_color)
            , shader_program(ShaderProgram(shaders))
            , textures(textures)
        {
        }

        RgbaColor color = {255, 255, 255, 255};
        ShaderProgram shader_program = {};
        std::vector<std::shared_ptr<Texture>> textures = {};
        Uuid id = Uuid::generate();
    };

    /// <summary>Purpose: Retrieves the shared default material instance.</summary>
    /// <remarks>Ownership: Returns a shared pointer owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const std::shared_ptr<Material>& get_standard_material();

    /// <summary>Purpose: Provides the shared default material instance.</summary>
    /// <remarks>Ownership: Returns a reference that participates in shared ownership
    /// of the default material instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const std::shared_ptr<Material>& standard_material = get_standard_material();
}
