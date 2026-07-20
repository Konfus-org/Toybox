#pragma once
#include "tbx/core/json.h"
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
    Result<Json> save(const T& object)
    {
        const auto type = get_type_registry().find(TypeSlot<T>::hash);
        if (!type)
            return fail("cannot save: type is not registered (tbx::register_type it first)");
        return ok(json_write(type->get(), object));
    }

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
    Result<void> load(T& object, const Json& data)
    {
        const auto type = get_type_registry().find(TypeSlot<T>::hash);
        if (!type)
            return fail("cannot load: type is not registered (tbx::register_type it first)");
        return json_read(type->get(), object, data);
    }

    /// @brief
    /// Purpose: Instantiates a kit body into a sandbox. Nested kit references resolve
    /// recursively through the resolver; cycles are load errors; root position offsets every
    /// parentless toy.
    Result<KitInstance> load(
        Sandbox& sandbox,
        const Json& kit,
        const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f),
        const KitResolver& resolver = {});

    /// @brief
    /// Purpose: Loads a sandbox layout: {"kits": [{reference, mode, position}]}. ALWAYS
    /// entries load immediately; STREAMED entries load/unload by distance to the streaming
    /// focus (Sandbox::stream_from), using bounds stored in each kit at save time.
    Result<void> load_layout(Sandbox& sandbox, const Json& layout, const KitResolver& resolver);
}
