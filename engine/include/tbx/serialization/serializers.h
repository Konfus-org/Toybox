#pragma once
#include "tbx/api.h"

namespace tbx::serialization
{
    /// @brief
    /// Purpose: Registers every builtin type's serializer (how it reads/writes on disk) —
    /// reflection::initialize() calls this right after the type registrations so
    /// Format::DEFAULT's reflection check always holds. Idempotent.
    TBX_API void register_builtin_serializers();
}
