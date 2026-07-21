#pragma once
#include "tbx/core/api.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/ecs/builtin_blocks.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: One scripting language. Backends coexist — C++, Lua, and C# can all run at
    /// once — so this is a real interface, not a link-time swap: each compiled-in backend
    /// (scripting/luau/, later csharp/...) implements it and claims sources by extension.
    /// @details
    /// Ownership: Owned by Scripts. Thread Safety: Main thread only.
    class TBX_API ScriptBackend
    {
      public:
        virtual ~ScriptBackend() = default;

      public:
        /// @brief
        /// Purpose: The backend's display name ("luau", "csharp", ...).
        virtual std::string_view get_name() const = 0;

        /// @brief
        /// Purpose: True when this backend runs sources with the given extension (".luau").
        virtual bool owns_extension(std::string_view extension) const = 0;

        /// @brief
        /// Purpose: Compiles and caches a source under its asset id (Script blocks reference
        /// it by AssetHandle); the name is diagnostics/routing only.
        virtual Result<void> load_source(
            const Uuid& id,
            const std::string& name,
            std::string_view source) = 0;

        /// @brief
        /// Purpose: Recompiles a source in place; live instances restart on their next update
        /// and script_reloaded fires. This IS hot reload.
        virtual Result<void> reload_source(
            const Uuid& id,
            const std::string& name,
            std::string_view source) = 0;

        /// @brief
        /// Purpose: Runs every scripted toy whose source this backend loaded.
        virtual void update(float delta_time) = 0;

        /// @brief
        /// Purpose: Runs scripts' fixed-cadence hook (physics-rate logic).
        virtual void fixed_update(float fixed_delta_time) = 0;
    };

    /// @brief
    /// Purpose: The scripting coordinator: routes sources to the backend that owns their
    /// extension (extensionless names go to the first backend) and fans update() out to all.
    /// @details
    /// Ownership: Owns every backend (compiled-in ones from TBX_SCRIPTING_BACKENDS plus any the
    /// game adds). Thread Safety: Main thread only.
    class TBX_API Scripts final
    {
      public:
        Scripts(Sandbox& sandbox, Events& events);

      public:
        Scripts(const Scripts&) = delete;
        Scripts& operator=(const Scripts&) = delete;

      public:
        /// @brief
        /// Purpose: Registers an additional backend (e.g. the game exe's own C++ "scripting").
        void add_backend(std::unique_ptr<ScriptBackend> backend);

        /// @brief
        /// Purpose: Registers a source under an explicit asset id (the asset pipeline path).
        Result<void> load_source(const Uuid& id, const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Compiles a source under a deterministic id derived from its name and
        /// returns the handle Script blocks use — the manual/test path.
        Result<AssetHandle<ScriptSource>> load_source(
            const std::string& name,
            std::string_view source);

        /// @brief
        /// Purpose: Hot reload under an explicit asset id.
        Result<void> reload_source(const Uuid& id, const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Hot reload under the name-derived id (the manual/test path).
        Result<void> reload_source(const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Runs every scripted toy across every backend. Called by tbx::run().
        void update(float delta_time);

        /// @brief
        /// Purpose: Runs every backend's fixed-cadence hook; called from the fixed step
        /// alongside physics so scripts can do physics-rate work.
        void fixed_update(float fixed_delta_time);

        /// @brief
        /// Purpose: True when some backend runs files with the given extension (".luau") —
        /// listeners use it to filter asset events down to script sources.
        bool owns(std::string_view extension) const;

      private:
        std::optional<std::reference_wrapper<ScriptBackend>> route(const std::string& name);

      private:
        std::vector<std::unique_ptr<ScriptBackend>> _backends;
    };
}
