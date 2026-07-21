#pragma once
#include "tbx/core/api.h"
#include "tbx/core/result.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/reflect/json_walker.h"
#include "tbx/serialization/json.h"
#include <span>
#include <type_traits>

namespace tbx
{
    // THE save/load surface — one generic pair for everything: tbx::save<Sandbox>(sandbox)
    // (the whole world as a kit), tbx::save<Toy>(toy) (a one-toy kit), tbx::save(any
    // registered type) (the reflection walker). Kits load back through tbx::load or
    // Sandbox::spawn.

    /// @brief
    /// Purpose: Serializes chosen toys (blocks, stickers, parent links, bounds) as a kit body.
    TBX_API Json save(Sandbox& sandbox, std::span<const Toy> toys);

    /// @brief
    /// Purpose: Serializes the whole sandbox — every live toy — as a kit body.
    template <typename T>
        requires(std::is_same_v<T, Sandbox>)
    Result<Json> save(T& sandbox);

    /// @brief
    /// Purpose: Serializes one toy (with its blocks and stickers) as a kit body.
    template <typename T>
        requires(std::is_same_v<T, Toy>)
    Result<Json> save(const T& toy);

    /// @brief
    /// Purpose: Serializes any registered type (register_type/register_block) to JSON.
    template <typename T>
        requires(
            !std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<Json> save(const T& object);

    /// @brief
    /// Purpose: Instantiates a kit body into a sandbox (delegates to Sandbox::spawn). Nested
    /// kit references resolve recursively through the resolver; cycles are load errors; root
    /// position offsets every parentless toy.
    TBX_API Result<KitInstance> load(
        Sandbox& sandbox,
        const Json& kit,
        const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f),
        const KitResolver& resolver = {});

    /// @brief
    /// Purpose: Populates any registered type from JSON produced by save(); the type's migrate
    /// hook runs for older versions.
    template <typename T>
        requires(
            !std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<void> load(T& object, const Json& data);
}

#include "tbx/serialization/serialization.inl"
