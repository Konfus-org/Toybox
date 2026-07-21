#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_context.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/systems/world/manager.h"
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
        explicit ScriptSystem(
            std::shared_ptr<AssetManager> asset_manager,
            ServiceProvider& services);
        explicit ScriptSystem(
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
        std::weak_ptr<Script> try_get_script(const ScriptLookup& lookup) override;
        void fixed_update(const DeltaTime& dt);
        void update(const DeltaTime& dt);

        /// @brief Applies override values onto the LIVE instance of one binding (the editor tweaking
        /// a script field mid-play) so the running script keeps its per-instance state. Succeeds as a
        /// no-op when the binding has no live instance —
        /// while not simulating there is nothing to touch; the binding's stored overrides land on the
        /// next instantiate. The json carries the same per-field typed values instantiate consumes.
        Result apply_overrides(const ScriptLookup& lookup, const Json& overrides);

        /// @brief Destroys every live script instance so the next update re-instantiates them fresh
        /// (running on_start again) against the current world. Used when the world is wholesale
        /// replaced (e.g. an editor leaving play mode restores its pre-play snapshot) so no per-script
        /// runtime state carries across the reset. Instances are otherwise reaped lazily on a
        /// simulating frame, which never runs while play is stopped.
        void reset();

      private:
        struct State;
        // Shared per-binding pass for both update rates; only the variable-rate pass (fixed ==
        // false) reaps instances whose bindings disappeared.
        void update(const DeltaTime& dt, bool fixed);

        void consume_script_reloads();
        void on_asset_reloaded(const AssetReloadedEvent& event);

        // Drops all runtime instances and evicts cached script prototypes before a plugin unloads,
        // so no script object outlives the module its vtable lives in. Instances are lazily
        // recreated on the next update against whatever script types are registered after the
        // reload.
        void on_plugins_unloading();

      private:
        std::unique_ptr<State> _state = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IMessageCoordinator> _message_coordinator = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::shared_ptr<ServiceProvider> _service_provider_alias = {};
        std::weak_ptr<ServiceProvider> _services = {};
    };
}
