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
    ///  A material is a collection of shaders
    /// </summary>
    struct TBX_EXPORT Material
    {
        Material() = default;
        Material(Ref<Shader> shader)
            : Shaders({shader}) {}
        Material(std::vector<Ref<Shader>> shaders)
            : Shaders(shaders) {}

        std::vector<Ref<Shader>> Shaders = {};
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// A material instance is a material at runtime, it represents a material with different params and textures.
    /// </summary>
    struct TBX_EXPORT MaterialInstance
    {
        MaterialInstance() = default;
        MaterialInstance(Ref<Material> material)
            : MaterialId(material->Id) {}
        MaterialInstance(Ref<Material> material, Ref<Texture> texture)
            : MaterialId(material->Id)
            , Textures({texture}) {}
        MaterialInstance(Ref<Material> material, std::vector<Ref<Texture>> textures)
            : MaterialId(material->Id)
            , Textures(textures) {}

        Uid MaterialId = Uid::Invalid;
        std::vector<Ref<Texture>> Textures = { MakeRef<Texture>() }; // default to one small white texture
        Uid InstanceId = Uid::Generate();
    };
}