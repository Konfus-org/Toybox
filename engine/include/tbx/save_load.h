#pragma once
#include "tbx/serialization/serialization.h"
#include "tbx/core/result.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/reflect/json_walker.h"
#include <span>

namespace tbx
{
    // THE save/load surface — one generic pair for everything registered. Objects round-trip
    // through the reflection walker; sandboxes/toys round-trip as kits.

    /// @brief
    /// Purpose: Serializes any registered type (register_type/register_block) to JSON.
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<Json> save(const T& object);

    /// @brief
    /// Purpose: Serializes chosen toys (blocks, stickers, parent links, bounds) as a kit body.
    Json save(Sandbox& sandbox, std::span<const Toy> toys);

    /// @brief
    /// Purpose: Serializes the whole sandbox — every live toy — as a kit body.
    Json save(Sandbox& sandbox);

    /// @brief
    /// Purpose: Populates any registered type from JSON produced by save(); the type's migrate
    /// hook runs for older versions.
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<void> load(T& object, const Json& data);

    /// @brief
    /// Purpose: Instantiates a kit body into a sandbox. Nested kit references resolve
    /// recursively through the resolver; cycles are load errors; root position offsets every
    /// parentless toy.
    Result<KitInstance> load(
        Sandbox& sandbox,
        const Json& kit,
        const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f),
        const KitResolver& resolver = {});

}

#include "tbx/save_load.inl"
