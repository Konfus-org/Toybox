#pragma once
#include "tbx/api.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/utils/result.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The scripting coordinator: routes sources to the backend that owns their extension
// (extensionless names go to the first backend) and fans update() out to all. The state is
// runtime.scripts. Main thread only.
namespace tbx
{
    namespace internal { struct RuntimeState; } // forward-declared to avoid a cycle (runtime.h includes this header)

    /// @brief
    /// Purpose: One scripting language. Backends coexist — C++, Lua, and C# can all run at
    /// once — so this is a real interface, not a link-time swap: each compiled-in backend
    /// (scripting/luau/, later csharp/...) implements it and claims sources by extension.
    /// @details
    /// Ownership: Owned by Scripts. Thread Safety: Main thread only.
    class TBX_DLL_EXPORT ScriptBackend
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
        /// Purpose: Compiles a source under its asset id and caches its bytecode; a syntax error
        /// comes back here (and leaves any previous bytecode intact). Calling it again for the same
        /// id recompiles — the reload path. The coordinator owns when this runs: it responds to the
        /// script asset's load/reload message.
        virtual Result<void> compile_script(
            const Uuid& id,
            const std::string& name,
            std::string_view source) = 0;

        /// @brief
        /// Purpose: Builds the toy's instance and runs its start hook. The coordinator calls it
        /// once — the first frame it sees the toy scripted, and again only after the script was
        /// unloaded (removed or reloaded) — so the backend does not dedupe; each call starts fresh.
        virtual void call_script_start(Toy toy, const Uuid& source_id) = 0;

        /// @brief
        /// Purpose: Runs the toy's per-frame update hook (delta_time in seconds).
        virtual void call_script_update(Toy toy, const Uuid& source_id, float delta_time) = 0;

        /// @brief
        /// Purpose: Runs the toy's fixed-step hook alongside physics (fixed_delta_time in seconds).
        virtual void call_script_fixed_update(
            Toy toy,
            const Uuid& source_id,
            float fixed_delta_time) = 0;

        /// @brief
        /// Purpose: Fires the toy's cleanup hook (the counterpart to start) — nothing else. The
        /// coordinator calls it right before purge_script when it sees a script go away (toy
        /// despawned, Script block removed, or shutdown). A removed toy passes a handle that is no
        /// longer alive, so scripts guard toy access with toy:is_alive().
        virtual void call_script_cleanup(Toy toy, const Uuid& source_id) = 0;

        /// @brief
        /// Purpose: Frees the toy's script instance (its VM ref would otherwise pin it forever —
        /// the GC can't reclaim a referenced value). Memory only: the coordinator owns when this
        /// runs and has already fired cleanup if it was needed.
        virtual void purge_script(Toy toy, const Uuid& source_id) = 0;
    };

    /// @brief
    /// Purpose: The scripting module's state, held by value on the Runtime: the backends
    /// (VMs), built lazily against the runtime's sandbox, plus which script-source assets
    /// were acquired. Declared after the sandbox in RuntimeState, so the VMs die before
    /// their world.
    struct TBX_DLL_EXPORT ScriptsState
    {
        ScriptsState() = default;
        ~ScriptsState() = default;

        ScriptsState(const ScriptsState&) = delete;
        ScriptsState& operator=(const ScriptsState&) = delete;

        std::vector<std::unique_ptr<ScriptBackend>> backends;
        std::unordered_set<Uuid> acquired_sources;

        // The active (started) scripts: each toy that has been started and the source it ran. The
        // coordinator starts a script the first frame it appears here-absent, and reaps one that
        // vanished (toy despawned or Script block removed) with cleanup + purge. This is where
        // "which scripts are live and when to start/purge them" lives — the backend only obeys.
        std::unordered_map<ToyId, Uuid> live_scripts;

        // Sources re-registered since last update (hot reload) — their running instances get
        // dropped and re-started next update, picking up the new code.
        std::unordered_set<Uuid> reloaded_sources;
    };

    /// @brief
    /// Purpose: (Re)compiles a script source under its asset id and flags it so any running
    /// instances restart on the new code next update. This is the one registration path — call it
    /// in response to the script asset's load/reload message. A syntax error comes back here and
    /// leaves the previous (working) bytecode running.
    TBX_DLL_EXPORT Result<void> compile_script(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        std::string_view source);

    // ---- Internal (engine machinery; not the user-facing API) ----
    // The per-frame script passes and the shutdown reap — driven by tbx::run(), not by scripts.
    namespace internal
    {
        /// @brief
        /// Purpose: Builds the compiled-in scripting backends against this runtime. Idempotent.
        TBX_DLL_EXPORT void initialize_scripting(RuntimeState& runtime);

        /// @brief
        /// Purpose: Runs every scripted toy's fixed-cadence hook; called from the fixed step alongside
        /// physics so scripts can do physics-rate work. Iterates the sandbox's scripted toys (the
        /// coordinator owns the loop; backends just run one toy at a time).
        TBX_DLL_EXPORT void fixed_update_scripts(
            ScriptsState& state,
            Sandbox& sandbox,
            float fixed_delta_time);

        /// @brief
        /// Purpose: Runs every scripted toy across every backend, first loading (once) every
        /// script-source asset a spawned toy references — the store announces it and the reload
        /// glue hands it to the owning backend. Then reaps any script that vanished since last frame
        /// (toy despawned or Script block removed): fires its cleanup hook and frees it. Called by
        /// tbx::run() every frame.
        TBX_DLL_EXPORT void update_scripts(
            ScriptsState& state,
            Sandbox& sandbox,
            AssetsState& assets,
            EventsState& events,
            float delta_time);

        /// @brief
        /// Purpose: Fires cleanup then frees every live script instance — the shutdown pass, run once
        /// while the sandbox is still alive (so cleanup gets live toys) before the VMs are destroyed.
        TBX_DLL_EXPORT void purge_scripts(ScriptsState& state, Sandbox& sandbox);
    }
}
