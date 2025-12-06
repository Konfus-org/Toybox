#pragma once
#include <entt/entt.hpp>

namespace tbx
{
    // A registry managing entities and their components.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    using Registry = entt::registry;
}
