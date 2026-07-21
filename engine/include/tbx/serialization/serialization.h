#pragma once
#include "tbx/ecs/sandbox.h"
#include "tbx/serialization/json_walker.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include <span>
#include <type_traits>


namespace tbx
{
    // THE save/load surface — one generic pair for everything: tbx::save<Sandbox>(sandbox)
    // (the whole world as a kit), tbx::save<Toy>(toy) (a one-toy kit), tbx::save(any
    // registered type) (the reflection walker). The kit pair itself — save(sandbox, toys)
    // and load(sandbox, kit) — lives next to Kit in ecs/kit.h.

    /// @brief
    /// Purpose: Serializes the whole sandbox — every live toy — as a kit.
    template <typename T>
        requires(std::is_same_v<T, Sandbox>)
    Result<Kit> save(T& sandbox);

    /// @brief
    /// Purpose: Serializes one toy (with its blocks and stickers) as a kit.
    template <typename T>
        requires(std::is_same_v<T, Toy>)
    Result<Kit> save(const T& toy);

    /// @brief
    /// Purpose: Serializes any registered type (reflection::describe / register_block) to JSON.
    template <typename T>
        requires(!std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<Json> save(const T& object);

    /// @brief
    /// Purpose: Populates any registered type from JSON produced by save(); the type's migrate
    /// hook runs for older versions.
    template <typename T>
        requires(!std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<void> load(T& object, const Json& data);
}

#include "tbx/serialization/serialization.inl"
