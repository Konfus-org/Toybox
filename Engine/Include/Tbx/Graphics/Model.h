#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Material.h"

namespace Tbx
{
    /// <summary>
    /// A model is just a mesh and a material
    /// </summary>
    struct Model
    {
        TBX_EXPORT Model(const Mesh& mesh, const MaterialInstance& material)
            : _mesh(mesh), _currentMaterial(material) {}

        TBX_EXPORT const Mesh& GetMesh() const { return _mesh; }
        TBX_EXPORT const MaterialInstance& GetMaterial() const { return _currentMaterial; }

    private:
        Mesh _mesh;
        MaterialInstance _currentMaterial;
    };
}
