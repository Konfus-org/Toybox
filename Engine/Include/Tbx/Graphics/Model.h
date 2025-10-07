#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Material.h"

namespace Tbx
{
    /// <summary>
    /// A model is just a mesh and a material
    /// </summary>
    struct TBX_EXPORT Model
    {
        Mesh Mesh;
        Material Material;
    };
}
