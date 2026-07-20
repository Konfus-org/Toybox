#pragma once
#include <entt/entt.hpp>

namespace tbx
{
    // The one ECS-storage seam: nothing outside this header names entt. Swapping the registry
    // library means re-pointing these aliases and keeping their semantics.
    using Registry = entt::registry;
    using ToyId = entt::entity;
    inline constexpr entt::null_t NULL_TOY = entt::null;
}
