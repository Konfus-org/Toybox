#pragma once
#include "tbx/ecs/sandbox.h"
#include "tbx/utils/typedefs.h"
#include <span>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Instantiates a kit into the sandbox — the implementation behind Sandbox::add(kit).
    /// Internal (takes the asset system explicitly); the public entry is the Sandbox::add methods,
    /// which pass the wired refs.
    Result<Toy> instantiate_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const Kit& kit,
        const Vec3& position);
    Result<Toy> instantiate_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const AssetHandle<Kit>& kit,
        const Vec3& position);

    /// @brief
    /// Purpose: Loads streamed kits in sight of any frustum and collapses those out of sight of
    /// all — the streaming half of update_sandbox(). Internal to the ecs implementation.
    void stream(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        std::span<const Frustum> frustums);

    /// @brief
    /// Purpose: Copies a kit's toys under `parent`, expanding nested immediate kits and
    /// registering nested streamed kits — the shared body of add() and stream-in. Defined in
    /// kit.cpp with the kit machinery; declared here so stream() (sandbox.cpp) can reach it.
    /// Internal to the ecs implementation — not part of the public sandbox API.
    Result<void> instantiate_under(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        Toy parent,
        const Kit& kit,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned);
}
