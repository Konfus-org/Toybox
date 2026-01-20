#include "tbx/graphics/model.h"

namespace tbx
{
    Model::Model() = default;

    Model::Model(const Model& other) = default;

    Model::Model(Model&& other) noexcept = default;

    Model& Model::operator=(const Model& other) = default;

    Model& Model::operator=(Model&& other) noexcept = default;

    Model::~Model() noexcept = default;
}
