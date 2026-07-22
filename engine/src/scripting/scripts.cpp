#include "tbx/scripting/scripts.h"
#include "tbx/app.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/reflection.h"
#include "tbx/scripting/script.h"
#include "tbx/runtime.h"
#include "tbx/utils/hash.h"
#include "builtin_backends.h"
#include <filesystem>
#include <unordered_set>

namespace tbx::scripts
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

    /// @brief
    /// Purpose: The deterministic id for manually-loaded (non-asset) sources.
    static Uuid derived_script_id(const std::string& name)
    {
        const uint64 hashed = hash(name);
        return Uuid {.hi = hashed, .lo = ~hashed};
    }

    //// BOUNDARY ////

    void initialize(RuntimeState& runtime)
    {
        ScriptsState& state = runtime.scripts;
        if (!state.backends.empty())
            return;
        reflection::initialize();
#ifdef TBX_SCRIPTING_HAS_LUAU
        state.backends.push_back(make_luau_backend(runtime));
        TBX_INFO("scripting backend '{}' ready", state.backends.back()->get_name());
#endif
    }

    void add_backend(ScriptsState& state, std::unique_ptr<ScriptBackend> backend)
    {
        TBX_INFO("scripting backend '{}' ready", backend->get_name());
        state.backends.push_back(std::move(backend));
    }

    void fixed_update(ScriptsState& state, const float fixed_delta_time)
    {
        for (const auto& backend : state.backends)
            backend->fixed_update(fixed_delta_time);
    }

    Result<void> load_source(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(state, name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().load_source(id, name, source);
    }

    Result<assets::AssetHandle<ScriptSource>> load_source(
        ScriptsState& state,
        const std::string& name,
        const std::string_view source)
    {
        const Uuid id = derived_script_id(name);
        auto loaded = load_source(state, id, name, source);
        if (!loaded)
            return std::unexpected(loaded.error());
        return ok(assets::AssetHandle<ScriptSource>(id));
    }

    bool owns(const ScriptsState& state, const std::string_view extension)
    {
        for (const auto& backend : state.backends)
            if (backend->owns_extension(extension))
                return true;
        return false;
    }

    Result<void> reload_source(
        ScriptsState& state,
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(state, name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().reload_source(id, name, source);
    }

    Result<void> reload_source(
        ScriptsState& state,
        const std::string& name,
        const std::string_view source)
    {
        return reload_source(state, derived_script_id(name), name, source);
    }

    void update(
        ScriptsState& state,
        ecs::Sandbox& sandbox,
        assets::AssetsState& assets,
        events::EventsState& events,
        const float delta_time)
    {
        // Script sources referenced by spawned toys are ordinary assets: acquire each once —
        // the store emits asset_reloaded and the reload glue hands it to the right backend.
        for (auto&& [entity, script] : sandbox.get_registry().view<Script>().each())
        {
            if (!script.source.id.is_valid() || state.acquired_sources.contains(script.source.id))
                continue;
            state.acquired_sources.insert(script.source.id);
            if (const auto acquired = assets::load_now(assets, events, script.source); !acquired)
                TBX_ERROR(
                    "script source '{}': {}",
                    script.source.id.to_string(),
                    acquired.error());
        }

        // Every backend runs; each skips Script blocks whose source it never loaded, so mixed
        // C++/Lua/C# toys coexist in one sandbox.
        for (const auto& backend : state.backends)
            backend->update(delta_time);
    }
}
