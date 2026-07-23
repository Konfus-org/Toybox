#include "tbx/scripting/scripts.h"
#include "builtin_backends.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/reflection.h"
#include "tbx/runtime.h"
#include "tbx/scripting/script.h"
#include "tbx/serialization/serializers.h"
#include "tbx/utils/hash.h"
#include <filesystem>
#include <unordered_set>

namespace tbx
{
    //// COMPILED-IN BACKEND FACTORIES ////

    // One line per backend folder; TBX_SCRIPTING_HAS_* comes from tbx_backend_list(). Adding a
    // language = a folder implementing ScriptBackend + its factory declared here.

#ifdef TBX_SCRIPTING_HAS_LUAU
    std::unique_ptr<ScriptBackend> make_luau_backend(RuntimeState& runtime);
#endif

    static std::optional<std::reference_wrapper<ScriptBackend>> route(
        ScriptsState& state,
        const std::string& name)
    {
        if (state.backends.empty())
            return {};
        const auto extension = std::filesystem::path(name).extension().string();
        if (!extension.empty())
            for (const auto& backend : state.backends)
                if (backend->owns_extension(extension))
                    return *backend;
        // Extensionless names (tests, generated sources) run on the first backend.
        return *state.backends.front();
    }

    //// BOUNDARY ////

    void initialize(RuntimeState& runtime)
    {
        ScriptsState& state = runtime.scripts;
        if (!state.backends.empty())
            return;
        TBX_ASSERT(
            is_reflection_ready() && is_serialization_ready(),
            "reflection not initialized — initialize_assets() must run before scripting");
#ifdef TBX_SCRIPTING_HAS_LUAU
        state.backends.push_back(make_luau_backend(runtime));
        TBX_INFO("scripting backend '{}' ready", state.backends.back()->get_name());
#endif
    }

    /// @brief
    /// Purpose: Starts a toy's script the first time it is seen active — the coordinator owns
    /// start-once, so we call start only on a script not already in live_scripts (and once more
    /// after its source is swapped). Every backend but the owning one no-ops.
    static void ensure_started(ScriptsState& state, Toy toy, const Uuid& source_id)
    {
        Uuid& tracked = state.live_scripts[toy.get_id()]; // invalid Uuid{} if newly seen
        if (tracked == source_id)
            return; // already started for this source — don't start again on every update
        tracked = source_id;
        for (const auto& backend : state.backends)
            backend->call_script_start(toy, source_id);
    }

    void fixed_update_scripts(ScriptsState& state, Sandbox& sandbox, const float fixed_delta_time)
    {
        sandbox.for_each_with<Script>(
            [&](Toy toy, Script& script)
            {
                if (!toy.is_enabled() || !script.source.is_valid())
                    return;

                ensure_started(state, toy, script.source.id); // no-op once started
                for (const auto& backend : state.backends)
                    backend->call_script_fixed_update(toy, script.source.id, fixed_delta_time);
            });
    }

    Result<void> compile_script(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(state, name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        // Recompile the source (a syntax error comes back and leaves the old bytecode running),
        // then flag it: update_scripts drops any running instances and re-starts them next frame on
        // the new code. The first load runs this too — nothing is live yet, so the flag is a no-op.
        if (const auto compiled = backend->get().compile_script(id, name, source); !compiled)
            return std::unexpected(compiled.error());
        state.reloaded_sources.insert(id);
        return ok();
    }

    /// @brief
    /// Purpose: Fires cleanup then frees one toy's script instance at every backend (all but the
    /// owning one no-op). The toy may already be dead — call_script_cleanup guards, and scripts
    /// guard toy access with toy:is_alive().
    static void cleanup_and_purge(
        ScriptsState& state,
        Sandbox& sandbox,
        const ToyId id,
        const Uuid& source_id)
    {
        const Toy toy(sandbox, id);
        for (const auto& backend : state.backends)
        {
            backend->call_script_cleanup(toy, source_id);
            backend->purge_script(toy, source_id);
        }
    }

    void update_scripts(
        ScriptsState& state,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const float delta_time)
    {
        // Restart hot-reloaded sources: drop their running instances (no cleanup — a recompile is a
        // silent swap) so the pass below sees them un-started and starts them fresh on the new
        // code.
        if (!state.reloaded_sources.empty())
        {
            for (auto it = state.live_scripts.begin(); it != state.live_scripts.end();)
            {
                if (!state.reloaded_sources.contains(it->second))
                {
                    ++it;
                    continue;
                }
                const Toy toy(sandbox, it->first);
                for (const auto& backend : state.backends)
                    backend->purge_script(toy, it->second);
                it = state.live_scripts.erase(it);
            }
            state.reloaded_sources.clear();
        }

        // One pass over scripted toys: acquire each source once, remember who is scripted this
        // frame (present), and start-once + update the enabled ones. ensure_started only calls
        // start the first time it sees a toy, so we never blindly re-start on every update.
        std::unordered_map<ToyId, Uuid> present;
        sandbox.for_each_with<Script>(
            [&](Toy toy, Script& script)
            {
                if (!script.source.id.is_valid())
                    return;
                // Script sources are ordinary assets: acquire each once — the store then emits
                // asset_reloaded and the reload glue hands it to the owning backend.
                if (!state.acquired_sources.contains(script.source.id))
                {
                    state.acquired_sources.insert(script.source.id);
                    if (const auto acquired = load_asset_now(assets, events, script.source);
                        !acquired)
                        TBX_ERROR(
                            "script source '{}': {}",
                            script.source.id.to_string(),
                            acquired.error());
                }
                present.emplace(toy.get_id(), script.source.id);
                if (!toy.is_enabled())
                    return;
                ensure_started(state, toy, script.source.id);
                for (const auto& backend : state.backends)
                    backend->call_script_update(toy, script.source.id, delta_time);
            });

        // Reap scripts that vanished — a started toy despawned, its Script block was removed, or
        // its source swapped: fire cleanup, then free the instance. This is the only place removal
        // is noticed, so cleanup fires promptly (a live handle when only the block was removed). A
        // disabled-but-present toy stays in `present`, so its instance survives until re-enabled.
        for (auto it = state.live_scripts.begin(); it != state.live_scripts.end();)
        {
            const auto p = present.find(it->first);
            if (p != present.end() && p->second == it->second)
            {
                ++it;
                continue;
            }
            cleanup_and_purge(state, sandbox, it->first, it->second);
            it = state.live_scripts.erase(it);
        }
    }

    void purge_scripts(ScriptsState& state, Sandbox& sandbox)
    {
        for (const auto& [id, source_id] : state.live_scripts)
            cleanup_and_purge(state, sandbox, id, source_id);
        state.live_scripts.clear();
    }
}
