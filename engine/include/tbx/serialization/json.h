#pragma once
#include "tbx/assets/load.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include <type_traits>
#include <tbx_serialization_backend.h>

// The JSON seam: Json is the backend document type (a deliberate public seam — use its own
// API directly: Json::parse(text, nullptr, false), Json::accept(text), value.dump(indent)),
// and the generic walker below is the one serializer — TypeInfo tables interpreted at
// runtime, no per-type serialization code anywhere in the engine.
namespace tbx::serialization
{
    /// @brief
    /// Purpose: Serializes an object of the given type to JSON, stamped with the type's name
    /// and version so json_read can migrate older data later.
    TBX_API Json json_write(const reflection::TypeInfo& type, const std::byte* object);

    /// @brief
    /// Purpose: Typed convenience over the byte-based walker. Constrained away from pointers
    /// so byte-pointer call sites hit the base overload instead of serializing the pointer.
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Json json_write(const reflection::TypeInfo& type, const T& object)
    {
        return json_write(type, reinterpret_cast<const std::byte*>(&object));
    }

    /// @brief
    /// Purpose: Populates an object from JSON produced by json_write. Runs the type's migrate
    /// hook when the stored version is older; unknown fields are dropped, missing fields keep
    /// their current values.
    TBX_API Result<void> json_read(const reflection::TypeInfo& type, std::byte* object, const Json& data);

    /// @brief
    /// Purpose: Typed convenience over the byte-based walker (see json_write's constraint).
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<void> json_read(const reflection::TypeInfo& type, T& object, const Json& data)
    {
        return json_read(type, reinterpret_cast<std::byte*>(&object), data);
    }

    /// @brief
    /// Purpose: Serializes any registered type (reflection::register_type) to JSON. The
    /// ecs::Sandbox/ecs::Toy kit saves live next to ecs::Kit (ecs/kit.h) as plain overloads.
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<Json> save(const T& object)
    {
        const auto type = reflection::describe<T>();
        if (!type)
            return fail(
                "cannot save: type is not registered (tbx::reflection::register_type it first)");
        return ok(json_write(type->get(), object));
    }

    /// @brief
    /// Purpose: Populates any registered type from JSON produced by save(); the type's
    /// migrate hook runs for older versions.
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<void> load(T& object, const Json& data)
    {
        const auto type = reflection::describe<T>();
        if (!type)
            return fail(
                "cannot load: type is not registered (tbx::reflection::register_type it first)");
        return json_read(type->get(), object, data);
    }
}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads (and validates) a JSON document from disk. Declared here because it
    /// specializes the assets::load primary template (assets/load.h) next to the Json seam.
    template <>
    TBX_API Result<serialization::Json> load<serialization::Json>(
        const std::filesystem::path& path);
}
