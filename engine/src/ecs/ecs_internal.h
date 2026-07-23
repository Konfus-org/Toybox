#pragma once
#include "tbx/ecs/container.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/serialization/json.h"

namespace tbx::internal
{
    static Json bounds_to_json(const Vec3& center, const float radius)
    {
        return Json {{"center", {center.x, center.y, center.z}}, {"radius", radius}};
    }

    /// @brief
    /// Purpose: The single toy-creation recipe — fresh id + ToyInfo{uuid, name, parent} + a
    /// default Transform — shared by ToyContainer::add and Toy::spawn so the two never drift.
    Toy create_toy(Registry& registry, std::string name, ToyId parent = NULL_TOY);

    /// @brief
    /// Purpose: The streaming primitive update_sandbox drives — loads streamed kits in sight of
    /// any frustum and collapses those out of sight of all, flushing a pending open() first.
    void stream(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        std::span<const Frustum> frustums);

    /// @brief
    /// Purpose: The per-frame sandbox tick: settles builtin components, flushes a pending open(),
    /// then streams kits by camera sight. tbx::run() calls this once, after scripts/physics settle
    /// transforms and before rendering.
    void update_sandbox(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        WindowsState& windows);
}
