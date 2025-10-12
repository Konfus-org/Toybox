#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"
#include <string>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// A collection of shaders.
    /// </summary>
    struct TBX_EXPORT ShaderProgram
    {
        ShaderProgram() = default;
        ShaderProgram(Ref<Shader> shader)
            : Shaders({shader}) {}
        ShaderProgram(std::vector<Ref<Shader>> shaders)
            : Shaders(shaders) {}

        std::vector<Ref<Shader>> Shaders = {};
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// A shader program with textures.
    /// </summary>
    struct TBX_EXPORT Material
    {
        using ShaderProgramType = ShaderProgram;

        Material() = default;
        Material(std::vector<Ref<Shader>> shaders)
            : ShaderProgram(ShaderProgramType(shaders)) {}
        Material(std::vector<Ref<Shader>> shaders, Ref<Texture> texture)
            : ShaderProgram(ShaderProgramType(shaders))
            , Textures({texture}) {}
        Material(std::vector<Ref<Shader>> shaders, std::vector<Ref<Texture>> textures)
            : ShaderProgram(ShaderProgramType(shaders))
            , Textures(textures) {}

        ShaderProgramType ShaderProgram = {};
        std::vector<Ref<Texture>> Textures = { MakeRef<Texture>() }; // default to one small white texture
        bool Transparent = false;
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// References a shared material by identifier.
    /// </summary>
    struct TBX_EXPORT MaterialInstance
    {
        Uid MaterialId = Uid::Invalid;
        Uid InstanceId = Uid::Generate();
    };
}
