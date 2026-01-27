#include "tbx/graphics/material.h"

namespace tbx
{
    const std::shared_ptr<Material>& get_standard_material()
    {
        static const std::shared_ptr<Material> material = std::make_shared<Material>();
        return material;
    }
}
