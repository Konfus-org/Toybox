#include "tbx/scripting/scripts.h"
#include "../../bindings/luau/luau_bindings.h"
#include "tbx/core/log.h"
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <cstdlib>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: One live script attachment: the Lua module-table instance for one toy.
    struct ScriptInstance
    {
        int table_ref = -1;
        uint64 script_hash = 0;
        uint32 generation = 0;
        bool is_started = false;
    };

    /// @brief
    /// Purpose: Luau-side state; lives behind the Scripts boundary so VM types never escape
    /// this backend folder.
    struct Scripts::State
    {
        lua_State* lua = nullptr; // owned; closed in ~Scripts
        std::unordered_map<uint64, std::string> bytecode_by_hash;
        std::unordered_map<uint64, uint32> generation_by_hash;
        std::unordered_map<uint32, ScriptInstance> instances; // keyed by ToyId value
    };

    //// SCRIPT REGISTRATION ////

    static void register_script_block()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_block<Script>("Script").field("source", &Script::source);
    }

    //// HELPERS ////

    static Result<std::string> compile_source(
        Scripts::State& state,
        const std::string& name,
        std::string_view source);

    static void drop_instance(Scripts::State& state, ScriptInstance& instance)
    {
        if (instance.table_ref >= 0)
            lua_unref(state.lua, instance.table_ref);
        instance = ScriptInstance {};
    }

    static void call_script_function(
        lua_State* lua,
        Sandbox& sandbox,
        const int table_ref,
        const char* function_name,
        const ToyId entity,
        const std::optional<float> delta_time)
    {
        lua_getref(lua, table_ref);
        lua_getfield(lua, -1, function_name);
        if (!lua_isfunction(lua, -1))
        {
            lua_pop(lua, 2);
            return;
        }
        push_toy(lua, sandbox, entity);
        int argument_count = 1;
        if (delta_time)
        {
            lua_pushnumber(lua, *delta_time);
            argument_count = 2;
        }
        if (lua_pcall(lua, argument_count, 0, 0) != 0)
        {
            log_error("script error in {}: {}", function_name, lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
        lua_pop(lua, 1); // the instance table
    }

    //// SCRIPTS ////

    Scripts::Scripts(Sandbox& sandbox, Events& events)
        : _sandbox(sandbox)
        , _events(events)
        , _state(std::make_unique<State>())
    {
        register_script_block();
        _state->lua = luaL_newstate();
        luaL_openlibs(_state->lua);
        open_tbx_bindings(_state->lua, sandbox);
    }

    Scripts::~Scripts()
    {
        lua_close(_state->lua); // releases every instance ref with it
    }

    static Result<std::string> compile_source(
        Scripts::State& state,
        const std::string& name,
        std::string_view source)
    {
        auto options = lua_CompileOptions {};
        size_t bytecode_size = 0;
        char* bytecode = luau_compile(source.data(), source.size(), &options, &bytecode_size);
        auto compiled = std::string(bytecode, bytecode_size);
        std::free(bytecode);

        // Compile errors only surface at load time — validate now so callers hear about them.
        if (luau_load(state.lua, name.c_str(), compiled.data(), compiled.size(), 0) != 0)
        {
            auto error =
                fail("script '{}' failed to compile: {}", name, lua_tostring(state.lua, -1));
            lua_pop(state.lua, 1);
            return error;
        }
        lua_pop(state.lua, 1); // the validated closure
        return compiled;
    }

    Result<void> Scripts::load_source(const std::string& name, std::string_view source)
    {
        auto compiled = compile_source(*_state, name, source);
        if (!compiled)
            return std::unexpected(compiled.error());
        const uint64 hash = hash_name(name);
        _state->bytecode_by_hash[hash] = std::move(*compiled);
        _state->generation_by_hash.try_emplace(hash, 1);
        return {};
    }

    Result<void> Scripts::reload_source(const std::string& name, std::string_view source)
    {
        auto compiled = compile_source(*_state, name, source);
        if (!compiled)
            return std::unexpected(compiled.error());
        const uint64 hash = hash_name(name);
        _state->bytecode_by_hash[hash] = std::move(*compiled);
        ++_state->generation_by_hash[hash]; // live instances restart on their next update
        _events.get().script_reloaded.emit({.script_hash = hash});
        return {};
    }

    void Scripts::update(const float delta_time)
    {
        auto& registry = _sandbox.get().get_registry();
        for (const auto [entity, script] : registry.view<Script>().each())
        {
            const uint64 hash = hash_name(script.source);
            const auto bytecode = _state->bytecode_by_hash.find(hash);
            if (bytecode == _state->bytecode_by_hash.end())
                continue; // source not loaded (yet) — silently idle, assets may still stream
            const uint32 generation = _state->generation_by_hash[hash];

            ScriptInstance& instance = _state->instances[static_cast<uint32>(entity)];
            const bool is_stale = instance.table_ref < 0 || instance.script_hash != hash
                || instance.generation != generation;
            if (is_stale)
            {
                drop_instance(*_state, instance);
                if (luau_load(
                        _state->lua,
                        script.source.c_str(),
                        bytecode->second.data(),
                        bytecode->second.size(),
                        0)
                        != 0
                    || lua_pcall(_state->lua, 0, 1, 0) != 0)
                {
                    log_error("script '{}': {}", script.source, lua_tostring(_state->lua, -1));
                    lua_pop(_state->lua, 1);
                    continue;
                }
                if (!lua_istable(_state->lua, -1))
                {
                    log_error("script '{}' must return a table", script.source);
                    lua_pop(_state->lua, 1);
                    continue;
                }
                instance.table_ref = lua_ref(_state->lua, -1);
                lua_pop(_state->lua, 1);
                instance.script_hash = hash;
                instance.generation = generation;
                instance.is_started = false;
            }

            if (!instance.is_started)
            {
                instance.is_started = true;
                call_script_function(
                    _state->lua, _sandbox.get(), instance.table_ref, "start", entity, {});
            }
            call_script_function(
                _state->lua,
                _sandbox.get(),
                instance.table_ref,
                "update",
                entity,
                delta_time);
        }
    }
}
