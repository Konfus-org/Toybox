#include "tbx/ecs/entities.h"
#include <string>

namespace tbx
{
    std::string to_string(const Entity& entity)
    {
        const auto& desc = entity.get_description();
        const auto id_value = static_cast<uint32>(entity.get_id());
        return std::string("Toy(ID: ") + std::to_string(id_value) + ", Name: " + desc.name
               + ", Tag: " + desc.tag + ", Layer: " + desc.layer + ")";
    }
}
