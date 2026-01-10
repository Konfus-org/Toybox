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
            std::make_shared<Shader>(Shader::default_frag),
            std::make_shared<Shader>(Shader::default_vert),
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

        ShaderProgram shader_program = {};
        std::vector<std::shared_ptr<Texture>> textures = {};
        Uuid id = Uuid::generate();
    };

    /// <summary>
    /// References a shared material by identifier.
    /// </summary>
    struct TBX_API MaterialInstance
    {
        Uuid material_id = {};
        Uuid instance_id = Uuid::generate();
    };
}
