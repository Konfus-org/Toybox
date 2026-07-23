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
    /// schema — THE one registration call. Self-guards on is_reflection_ready(), so calling it
    /// twice is a no-op; hosts and tests call it once before loading anything themselves.
    TBX_DLL_EXPORT void initialize_reflection();

    /// @brief
    /// Purpose: True once the builtin type shapes are registered — the readiness check the
    /// engine subsystems assert before they touch reflected types.
    TBX_DLL_EXPORT bool is_reflection_ready();

    // ---- Internal (engine machinery; not the user-facing API) ----
    namespace internal
    {
        /// @brief
        /// Purpose: Drops every registered type so the next initialize_reflection() rebuilds from
        /// scratch — for tests that need a clean registry between cases. Not for runtime use.
        TBX_DLL_EXPORT void purge_reflection_registry();
    }
}
