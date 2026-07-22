#pragma once
// The whole reflection surface: field/type records, the process-wide registry, and the
// fluent registration builder.
#include "tbx/api.h"
#include "tbx/reflection/field_info.h"
#include "tbx/reflection/type_info.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/reflection/type_registry.h"

namespace tbx
{
    /// @brief
    /// Purpose: Registers every builtin type — blocks, asset types, and the App/.tapp
    /// schema — THE one registration call. Idempotent; run() and every subsystem entry
    /// point call it, hosts and tests call it before loading anything themselves.
    TBX_API void initialize_reflection();
}
