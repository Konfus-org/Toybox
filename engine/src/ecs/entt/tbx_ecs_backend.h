#pragma once
// The entt ECS backend (cmake tbx_backend(ECS entt)). This is the ONE file that names entt;
// the registry types it defines here ARE the ecs storage seam (tbx/ecs/registry.h includes
// this). Swapping ECS libraries means dropping a sibling folder that defines the same
// tbx::ecs aliases over a different registry — nothing else in the engine sees entt.
#include <entt/entt.hpp>

namespace tbx::ecs
{
    using Registry = entt::registry;
    using ToyId = entt::entity;
    inline constexpr entt::null_t NULL_TOY = entt::null;
}
