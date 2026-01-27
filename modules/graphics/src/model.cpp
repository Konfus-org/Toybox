#include "tbx/graphics/model.h"

namespace tbx
{
    const std::shared_ptr<Model>& get_default_model()
    {
        static const std::shared_ptr<Model> model = std::make_shared<Model>();
        return model;
    }

    Model::Model() = default;

    Model::Model(const Model& other) = default;

    Model::Model(Model&& other) noexcept = default;

    Model& Model::operator=(const Model& other) = default;

    Model& Model::operator=(Model&& other) noexcept = default;

    Model::~Model() noexcept = default;
}
