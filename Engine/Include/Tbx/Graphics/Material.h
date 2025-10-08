#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"
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
        Material() = default;
        Material(Ref<ShaderProgram> shaders)
            : ShaderProgram(shaders) {}
        Material(Ref<ShaderProgram> shaders, Ref<Texture> texture)
            : ShaderProgram(shaders)
            , Textures({texture}) {}
        Material(Ref<ShaderProgram> shaders, std::vector<Ref<Texture>> textures)
            : ShaderProgram(shaders)
            , Textures(textures) {}

        Ref<ShaderProgram> ShaderProgram = nullptr;
        std::vector<Ref<Texture>> Textures = { MakeRef<Texture>() }; // default to one small white texture
        Uid Id = Uid::Generate();
    };
}