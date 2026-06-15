#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    class IMessageCoordinator;

    /// @brief
    /// Purpose: Creates and updates runtime script instances from entity script containers.
    class TBX_API ScriptSystem final : public IScriptResolver
    {
      public:
        ScriptSystem(std::shared_ptr<AssetManager> asset_manager, ServiceProvider& services);
        ScriptSystem(
            std::weak_ptr<ServiceProvider> services,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            std::weak_ptr<IMessageCoordinator> message_coordinator);
        ~ScriptSystem() noexcept;

      public:
        ScriptSystem(const ScriptSystem&) = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;
        ScriptSystem(ScriptSystem&&) noexcept = delete;
        ScriptSystem& operator=(ScriptSystem&&) noexcept = delete;

      public:
        void fixed_update(const DeltaTime& dt);
        void update(const DeltaTime& dt);

        std::weak_ptr<Script> try_get_script(const ScriptLookup& lookup) override;

      private:
        struct State;
        void consume_script_reloads();
        void on_asset_reloaded(const AssetReloadedEvent& event);

      private:
        std::unique_ptr<State> _state = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IMessageCoordinator> _message_coordinator = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::shared_ptr<ServiceProvider> _service_provider_alias = {};
        std::weak_ptr<ServiceProvider> _services = {};
    };
}
