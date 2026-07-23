#pragma once
#include "tbx/api.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/ecs/toy.h"
#include <optional>
#include <string>

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
    /// Purpose: Opens a kit as the whole running world (REPLACE) and returns the world for chaining —
    /// open(root).open(extra, OpenMode::ADDITIVE).spawn("A"). The world resolves on the next tick.
    TBX_DLL_EXPORT Sandbox& open(const AssetHandle<Kit>& root);

    /// @brief
    /// Purpose: Closes the running world — every toy and all streaming state gone, ready to open
    /// another. No argument: there is one running world.
    TBX_DLL_EXPORT void close();
}
