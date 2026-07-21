#pragma once
#include "tbx/assets/load.h"
#include "tbx/ecs/toy.h"
#include "tbx/math/math.h"
#include "tbx/reflect/json_walker.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <span>

namespace tbx
{
    class Sandbox;

    /// @brief
    /// Purpose: A set of things: toys (with their blocks and stickers) plus references to
    /// other kits, recursively — one concept covering prefab, scene, level, and chunk. A kit
    /// is an ordinary asset (AssetHandle<Kit>, .kit files); the body is its parsed serialized
    /// form, and only the kit save/load pair below looks inside it.
    struct TBX_API Kit
    {
        Json body = {};
    };

    /// @brief
    /// Purpose: Handle to one instantiated kit; Sandbox::despawn(instance) removes exactly
    /// the toys it spawned (including toys from nested kit references).
    struct TBX_API KitInstance
    {
        uint64 id = 0;
    };

    /// @brief
    /// Purpose: Loads a .kit file (validated JSON kit body).
    template <>
    TBX_API Result<Kit> load<Kit>(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Serializes chosen toys (blocks, stickers, parent links, bounds) as a kit.
    TBX_API Kit save(Sandbox& sandbox, std::span<const Toy> toys);

    /// @brief
    /// Purpose: Instantiates a kit into a sandbox — the load half of save() round-trips and
    /// what Sandbox::spawn(AssetHandle<Kit>) runs on. Nested kit references resolve
    /// recursively through the sandbox's assets; cycles are load errors; root position
    /// offsets every parentless toy.
    TBX_API Result<KitInstance> load(
        Sandbox& sandbox,
        const Kit& kit,
        const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f));
}
