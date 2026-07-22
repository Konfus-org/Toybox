#pragma once
#include "tbx/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Registers every builtin type's serializer (how it reads/writes on disk) —
    /// initialize_reflection() calls this right after the type registrations so
    /// SerializerFormat::DEFAULT's reflection check always holds. Idempotent.
    TBX_API void register_builtin_serializers();
}
