#pragma once
#include "tbx/api.h"
#include "tbx/ecs/toy.h"
#include <optional>
#include <string>
#include <vector>

// The scene verbs over the running runtime (tbx::get_runtime().sandbox) — the SAME functions C++ systems and
// scripts call, so there is one clean API and the bindings generate from it. Main-thread only (they read
// the global runtime). Declarations only; implemented in scene.cpp against get_runtime().
namespace tbx
{
    /// @brief
    /// Purpose: Creates a toy (with a default Transform) in the running world and returns its handle.
    TBX_DLL_EXPORT Toy spawn(std::string name);

    /// @brief
    /// Purpose: The first toy with the given name in the running world, or nothing.
    TBX_DLL_EXPORT std::optional<Toy> find(std::string name);

    /// @brief
    /// Purpose: Removes a toy and its whole subtree from the running world.
    TBX_DLL_EXPORT void despawn(Toy toy);

    /// @brief
    /// Purpose: Every toy in the running world, as handles.
    TBX_DLL_EXPORT std::vector<Toy> toys();
}
