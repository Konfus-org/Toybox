#pragma once
#include "tbx/core/result.h"
#include "tbx/reflect/type_info.h"

namespace tbx
{
    // The one serializer: a generic walker that interprets TypeInfo tables at runtime. No
    // per-type serialization code exists anywhere in the engine.

    /// @brief
    /// Purpose: Serializes an object of the given type to JSON, stamped with the type's name
    /// and version so json_read can migrate older data later.
    Json json_write(const TypeInfo& type, const void* object);

    /// @brief
    /// Purpose: Populates an object from JSON produced by json_write. Runs the type's migrate
    /// hook when the stored version is older; unknown fields are dropped, missing fields keep
    /// their current values.
    Result<void> json_read(const TypeInfo& type, void* object, const Json& data);
}
