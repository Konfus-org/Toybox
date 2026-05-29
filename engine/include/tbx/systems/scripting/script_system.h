#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Creates and updates runtime script instances from entity script containers.
    class TBX_API ScriptSystem final : public IScriptResolver
    {
      public:
        ScriptSystem(std::weak_ptr<AssetManager> asset_manager, ServiceProvider& services);
        ~ScriptSystem() noexcept;

      public:
        ScriptSystem(const ScriptSystem&) = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;
        ScriptSystem(ScriptSystem&&) noexcept = delete;
        ScriptSystem& operator=(ScriptSystem&&) noexcept = delete;

      public:
        void fixed_update(const DeltaTime& dt);
        void update(const DeltaTime& dt);

        Script* try_get_script(const ScriptLookup& lookup) override;

      private:
        struct Impl;
        std::unique_ptr<Impl> _impl = nullptr;
    };
}
