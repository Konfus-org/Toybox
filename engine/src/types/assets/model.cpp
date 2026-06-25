#include "tbx/types/assets/model.h"

namespace tbx
{
    Model::Model()
    {
        meshes = {Mesh::QUAD};
        parts = {ModelPart()};
        slots = {Handle()};
    }

    Model::Model(Mesh mesh)
    {
        meshes = {std::move(mesh)};
        parts = {ModelPart()};
        slots = {Handle()};
    }

    Model::Model(Mesh mesh, Handle slot)
    {
        meshes = {std::move(mesh)};
        parts = {ModelPart()};
        slots = {std::move(slot)};
    }
}
