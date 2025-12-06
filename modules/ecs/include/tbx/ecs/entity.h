#pragma once
#include <entt/entt.hpp>

namespace tbx
{
    // An entity identifier.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using EntityHandle = entt::entity;

    namespace invalid
    {
        inline constexpr EntityHandle entity_handle = entt::null;
    }
}
