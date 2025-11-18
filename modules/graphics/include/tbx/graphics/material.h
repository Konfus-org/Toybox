#pragma once
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include "tbx/std/smart_pointers.h"
#include "tbx/std/uuid.h"
#include "tbx/tbx_api.h"
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
        explicit ShaderProgram(Ref<Shader> shader)
            : shaders({ shader }) {}
        explicit ShaderProgram(std::vector<Ref<Shader>> shaders)
            : shaders(std::move(shaders)) {}

        std::vector<Ref<Shader>> shaders = {};
        Uuid id = Uuid::generate();
    };

    /// <summary>
    /// A shader program with textures.
    /// </summary>
    struct TBX_API Material
    {
        Material() = default;
        Material(std::vector<Ref<Shader>> shaders)
            : shader_program(ShaderProgram(std::move(shaders))) {}
        Material(std::vector<Ref<Shader>> shaders, Ref<Texture> texture)
            : shader_program(ShaderProgram(std::move(shaders)))
            , textures({ texture }) {}
        Material(std::vector<Ref<Shader>> shaders, std::vector<Ref<Texture>> textures)
            : shader_program(ShaderProgram(std::move(shaders)))
            , textures(std::move(textures)) {}

        ShaderProgram shader_program = {};
        std::vector<Ref<Texture>> textures = {};
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
