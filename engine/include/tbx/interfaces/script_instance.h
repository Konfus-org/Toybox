#pragma once
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Language-neutral handle to one live script instance. A scripting backend returns these
    /// from instantiate(); ScriptSystem drives the lifetime hooks.
    /// @details
    /// Ownership: Owned (shared) by ScriptSystem for the lifetime of its entity binding. The empty
    /// default hooks make every callback optional so a backend's instance implements only what it needs.
    /// Thread Safety: Driven from the main update thread.
    class TBX_API IScriptInstance
    {
      public:
        virtual ~IScriptInstance() noexcept = default;

        virtual void on_start() {}
        virtual void on_update(const DeltaTime&) {}
        virtual void on_fixed_update(const DeltaTime&) {}
        virtual void on_destroy() {}
    };
}
