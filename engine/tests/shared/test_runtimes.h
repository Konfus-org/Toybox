#pragma once
#include "tbx/engine_types.generated.h"
#include "tbx/systems/plugin_api/runtime_registrations.h"

namespace tbx::tests
{
    /// @brief Registers the engine's serializable/asset/component types into the engine-core
    /// container exactly once per test binary. Auto-registration no longer exists: production hosts
    /// register at launcher startup, so tests exercising engine types (components, assets, input
    /// maps) must call this first. EngineTests does it globally via its test environment.
    inline void ensure_engine_types_registered()
    {
        static const bool registered = []
        {
            register_engine_types(engine_core_runtime());
            return true;
        }();
        static_cast<void>(registered);
    }
}
