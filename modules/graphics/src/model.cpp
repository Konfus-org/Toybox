#include "tbx/graphics/model.h"

namespace tbx
{
    const std::shared_ptr<Model>& get_default_model()
    {
        static const std::shared_ptr<Model> model = std::make_shared<Model>();
        return model;
    }
}
