#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include "tbx/reflection/type_info.h"
#include "tbx/serialization/json.h"
#include <type_traits>

namespace tbx::serialization
{
    // The one serializer: a generic walker that interprets TypeInfo tables at runtime. No
    // per-type serialization code exists anywhere in the engine.

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
}
