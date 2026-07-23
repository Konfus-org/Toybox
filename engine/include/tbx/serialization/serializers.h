#pragma once
#include "tbx/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Registers every builtin type's serializer (how it reads/writes on disk) —
    /// initialize_reflection() calls this right after the type registrations so
    /// SerializerFormat::DEFAULT's reflection check always holds. Self-guards on
    /// is_serialization_ready(), so calling it twice is a no-op.
    TBX_DLL_EXPORT void register_builtin_serializers();

    /// @brief
    /// Purpose: True once the builtin serializers are registered — the readiness check the
    /// engine subsystems assert before they read/write assets.
    TBX_DLL_EXPORT bool is_serialization_ready();

    // ---- Internal (engine machinery; not the user-facing API) ----
    namespace internal
    {
        /// @brief
        /// Purpose: Drops every registered serializer so the next register_builtin_serializers()
        /// rebuilds from scratch — for tests that need a clean registry between cases. Not for
        /// runtime use: describe_serializer<T>()/read<T>/write<T> must not run between a purge and
        /// the re-registration that re-points SerializerSlot<T>::info.
        TBX_DLL_EXPORT void purge_serialization_registry();
    }
}
