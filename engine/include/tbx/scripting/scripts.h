#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/utils/result.h"
#include "tbx/scripting/script.h"
#include "tbx/utils/typedefs.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>


// The scripting coordinator: routes sources to the backend that owns their extension
// (extensionless names go to the first backend) and fans update() out to all. The state is
// runtime.scripts. Main thread only.
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
        /// it by Handle); the name is diagnostics/routing only.
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
    /// Purpose: The scripting module's state, held by value on the Runtime: the backends
    /// (VMs), built lazily against the runtime's sandbox, plus which script-source assets
    /// were acquired. Declared after the sandbox in RuntimeState, so the VMs die before
    /// their world.
    struct TBX_API ScriptsState
    {
        ScriptsState() = default;
        ~ScriptsState() = default;

        ScriptsState(const ScriptsState&) = delete;
        ScriptsState& operator=(const ScriptsState&) = delete;

        std::vector<std::unique_ptr<ScriptBackend>> backends;
        std::unordered_set<Uuid> acquired_sources;
    };

    /// @brief
    /// Purpose: Registers an additional backend (e.g. the game exe's own C++ "scripting").
    TBX_API void add_backend(ScriptsState& state, std::unique_ptr<ScriptBackend> backend);

    /// @brief
    /// Purpose: Runs every backend's fixed-cadence hook; called from the fixed step alongside
    /// physics so scripts can do physics-rate work.
    TBX_API void fixed_update_scripts(ScriptsState& state, float fixed_delta_time);

    /// @brief
    /// Purpose: Registers a source under an explicit asset id (the asset pipeline path).
    TBX_API Result<void> load_source(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        std::string_view source);

    /// @brief
    /// Purpose: Compiles a source under a deterministic id derived from its name and returns
    /// the handle Script blocks use — the manual/test path.
    TBX_API Result<AssetHandle<ScriptSource>> load_source(
        ScriptsState& state,
        const std::string& name,
        std::string_view source);

    /// @brief
    /// Purpose: True when some backend runs files with the given extension (".luau") —
    /// listeners use it to filter asset events down to script sources.
    TBX_API bool owns_script_extension(const ScriptsState& state, std::string_view extension);

    /// @brief
    /// Purpose: Hot reload under an explicit asset id.
    TBX_API Result<void> reload_source(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        std::string_view source);

    /// @brief
    /// Purpose: Hot reload under the name-derived id (the manual/test path).
    TBX_API Result<void> reload_source(
        ScriptsState& state,
        const std::string& name,
        std::string_view source);

    /// @brief
    /// Purpose: Runs every scripted toy across every backend, first loading (once) every
    /// script-source asset a spawned toy references — the store announces it and the reload
    /// glue hands it to the owning backend. Called by tbx::run() every frame.
    TBX_API void update_scripts(
        ScriptsState& state,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        float delta_time);
}
