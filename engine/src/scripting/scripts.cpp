#include "tbx/scripting/scripts.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include <filesystem>

namespace tbx::scripts
{
    //// COMPILED-IN BACKEND FACTORIES ////
    // One line per backend folder; TBX_SCRIPTING_HAS_* comes from tbx_backend_list(). Adding a
    // language = a folder implementing ScriptBackend + its factory declared here.

#ifdef TBX_SCRIPTING_HAS_LUAU
    std::unique_ptr<ScriptBackend> make_luau_backend(Sandbox& sandbox);
#endif

    //// STATE ////

    /// @brief
    /// Purpose: The module's whole state: the backends, built against one bound sandbox.
    struct ScriptsState
    {
        Sandbox* sandbox = nullptr;
        std::vector<std::unique_ptr<ScriptBackend>> _backends;
    };

    static std::unique_ptr<ScriptsState> g_scripts = {};

    //// BOUNDARY ////

    void bind(Sandbox& sandbox)
    {
        if (g_scripts && g_scripts->sandbox == &sandbox)
            return;
        g_scripts = std::make_unique<ScriptsState>();
        g_scripts->sandbox = &sandbox;
        register_builtin_blocks();
#ifdef TBX_SCRIPTING_HAS_LUAU
        add_backend(make_luau_backend(sandbox));
#endif
    }

    void add_backend(std::unique_ptr<ScriptBackend> backend)
    {
        if (!g_scripts)
        {
            TBX_ERROR("scripts::add_backend needs a bound sandbox (scripts::bind first)");
            return;
        }
        TBX_INFO("scripting backend '{}' ready", backend->get_name());
        g_scripts->_backends.push_back(std::move(backend));
    }

    void purge()
    {
        g_scripts.reset();
    }

    static std::optional<std::reference_wrapper<ScriptBackend>> route(const std::string& name)
    {
        if (!g_scripts || g_scripts->_backends.empty())
            return {};
        const auto extension = std::filesystem::path(name).extension().string();
        if (!extension.empty())
            for (const auto& backend : g_scripts->_backends)
                if (backend->owns_extension(extension))
                    return *backend;
        // Extensionless names (tests, generated sources) run on the first backend.
        return *g_scripts->_backends.front();
    }

    /// @brief
    /// Purpose: The deterministic id for manually-loaded (non-asset) sources.
    static Uuid derived_script_id(const std::string& name)
    {
        const uint64 hashed = hash(name);
        return Uuid {.hi = hashed, .lo = ~hashed};
    }

    Result<void> load_source(
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().load_source(id, name, source);
    }

    Result<AssetHandle<ScriptSource>> load_source(
        const std::string& name,
        const std::string_view source)
    {
        const Uuid id = derived_script_id(name);
        auto loaded = load_source(id, name, source);
        if (!loaded)
            return std::unexpected(loaded.error());
        return ok(AssetHandle<ScriptSource>(id));
    }

    Result<void> reload_source(
        const Uuid& id,
        const std::string& name,
        const std::string_view source)
    {
        const auto backend = route(name);
        if (!backend)
            return fail("no scripting backend can run '{}'", name);
        return backend->get().reload_source(id, name, source);
    }

    Result<void> reload_source(const std::string& name, const std::string_view source)
    {
        return reload_source(derived_script_id(name), name, source);
    }

    bool owns(const std::string_view extension)
    {
        if (!g_scripts)
            return false;
        for (const auto& backend : g_scripts->_backends)
            if (backend->owns_extension(extension))
                return true;
        return false;
    }

    void fixed_update(const float fixed_delta_time)
    {
        if (!g_scripts)
            return;
        for (const auto& backend : g_scripts->_backends)
            backend->fixed_update(fixed_delta_time);
    }

    void update(const float delta_time)
    {
        if (!g_scripts)
            return;
        // Every backend runs; each skips Script blocks whose source it never loaded, so mixed
        // C++/Lua/C# toys coexist in one sandbox.
        for (const auto& backend : g_scripts->_backends)
            backend->update(delta_time);
    }
}
