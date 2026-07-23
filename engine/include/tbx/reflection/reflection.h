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
    /// Purpose: True once the builtin type shapes are registered — the readiness check the
    /// engine subsystems assert before they touch reflected types.
    TBX_DLL_EXPORT bool is_reflection_ready();

}
