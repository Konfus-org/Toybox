#include "tbx/scripting/scripts.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include <filesystem>

namespace tbx
{
    //// COMPILED-IN BACKEND FACTORIES ////
    // One line per backend folder; TBX_SCRIPTING_HAS_* comes from tbx_backend_list(). Adding a
    // language = a folder implementing ScriptBackend + its factory declared here.

#ifdef TBX_SCRIPTING_HAS_LUAU
    std::unique_ptr<ScriptBackend> make_luau_backend(Sandbox& sandbox, Events& events);
#endif

    //// SCRIPTS ////

    Scripts::Scripts(Sandbox& sandbox, Events& events)
    {
        register_builtin_blocks();
#ifdef TBX_SCRIPTING_HAS_LUAU
        add_backend(make_luau_backend(sandbox, events));
#endif
    }

    void Scripts::add_backend(std::unique_ptr<ScriptBackend> backend)
    {
        TBX_INFO("scripting backend '{}' ready", backend->get_name());
        _backends.push_back(std::move(backend));
    }

    std::optional<std::reference_wrapper<ScriptBackend>> Scripts::route(const std::string& name)
    {
        if (_backends.empty())
            return {};
        const auto extension = std::filesystem::path(name).extension().string();
        if (!extension.empty())
            for (const auto& backend : _backends)
                if (backend->owns_extension(extension))
                    return *backend;
        // Extensionless names (tests, generated sources) run on the first backend.
        return *_backends.front();
    }

    /// @brief
    /// Purpose: The deterministic id for manually-loaded (non-asset) sources.
    static Uuid derived_script_id(const std::string& name)
    {
        const uint64 hashed = hash(name);
        return Uuid {.hi = hashed, .lo = ~hashed};
    }

    Result<void> Scripts::load_source(
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().load_source(id, name, source);
    }

    Result<AssetHandle<ScriptSource>> Scripts::load_source(
        const std::string& name,
        const std::string_view source)
    {
        const Uuid id = derived_script_id(name);
        auto loaded = load_source(id, name, source);
        if (!loaded)
            return std::unexpected(loaded.error());
        return ok(AssetHandle<ScriptSource>(id));
    }

    Result<void> Scripts::reload_source(
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().reload_source(id, name, source);
    }

    Result<void> Scripts::reload_source(const std::string& name, const std::string_view source)
    {
        return reload_source(derived_script_id(name), name, source);
    }

    bool Scripts::owns(const std::string_view extension) const
    {
        for (const auto& backend : _backends)
            if (backend->owns_extension(extension))
                return true;
        return false;
    }

    void Scripts::fixed_update(const float fixed_delta_time)
    {
        for (const auto& backend : _backends)
            backend->fixed_update(fixed_delta_time);
    }

    void Scripts::update(const float delta_time)
    {
        // Every backend runs; each skips Script blocks whose source it never loaded, so mixed
        // C++/Lua/C# toys coexist in one sandbox.
        for (const auto& backend : _backends)
            backend->update(delta_time);
    }
}
