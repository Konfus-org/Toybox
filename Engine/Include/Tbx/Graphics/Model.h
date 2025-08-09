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
        EXPORT Model(const Mesh& mesh, const Material& material)
            : _mesh(mesh), _currentMaterial(material) {}

        EXPORT const Mesh& GetMesh() const { return _mesh; }
        EXPORT const Material& GetMaterial() const { return _currentMaterial; }

    private:
        Mesh _mesh;
        Material _currentMaterial;
    };
}
