#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    ///  A material is a collection of shaders
    /// </summary>
    struct TBX_EXPORT Material
    {
        std::vector<Shader> Shaders = {};
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// A material instance is a material at runtime, it represents a material with different params and textures.
    /// </summary>
    struct TBX_EXPORT MaterialInstance
    {
        Ref<Material> Material = nullptr;
        std::vector<Texture> Textures = { Texture() }; // default to one small white texture
        Uid Id = Uid::Generate();
    };
}