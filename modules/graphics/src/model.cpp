#include "tbx/graphics/model.h"

namespace tbx
{
    Model::Model()
    {
        meshes = {quad};
        materials = {Material()};
        parts = {ModelPart()};
    }

    Model::Model(const Mesh& mesh)
    {
        meshes = {mesh};
        materials = {Material()};
        parts = {ModelPart()};
    }

    Model::Model(const Mesh& mesh, const Material& material)
    {
        meshes = {mesh};
        materials = {material};
        parts = {ModelPart()};
    }
}
