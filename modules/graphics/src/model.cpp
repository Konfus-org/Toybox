#include "tbx/graphics/model.h"
#include <utility>

namespace tbx
{
    Model::Model()
    {
        meshes = {quad};
        materials = {Material()};
        parts = {ModelPart()};
    }

    Model::Model(Mesh mesh)
    {
        meshes = {std::move(mesh)};
        materials = {Material()};
        parts = {ModelPart()};
    }

    Model::Model(Mesh mesh, Material material)
    {
        meshes = {std::move(mesh)};
        materials = {std::move(material)};
        parts = {ModelPart()};
    }
}
