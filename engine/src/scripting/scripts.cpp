#include "tbx/scripting/scripts.h"
#include "../bindings/luau/luau_bindings.h"
#include "tbx/core/log.h"
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <cstdlib>

namespace tbx
{
    //// SCRIPT REGISTRATION ////

    static void register_script_block()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_block<Script>("Script").field("source", &Script::source);
    }

    //// SCRIPTS ////

    Scripts::Scripts(Sandbox& sandbox, Events& events)
        : _sandbox(sandbox)
        , _events(events)
    {
        register_script_block();
        _lua = luaL_newstate();
        luaL_openlibs(_lua);
        open_tbx_bindings(_lua, sandbox);
    }

    Scripts::~Scripts()
    {
        lua_close(_lua); // releases every instance ref with it
    }

    Result<std::string> Scripts::compile(const std::string& name, std::string_view source)
    {
        auto options = lua_CompileOptions {};
        size_t bytecode_size = 0;
        char* bytecode = luau_compile(source.data(), source.size(), &options, &bytecode_size);
        auto compiled = std::string(bytecode, bytecode_size);
        std::free(bytecode);

        // Compile errors only surface at load time — validate now so callers hear about them.
        if (luau_load(_lua, name.c_str(), compiled.data(), compiled.size(), 0) != 0)
        {
            auto error = fail("script '{}' failed to compile: {}", name, lua_tostring(_lua, -1));
            lua_pop(_lua, 1);
            return error;
        }
        lua_pop(_lua, 1); // the validated closure
        return compiled;
    }

    Result<void> Scripts::load_source(const std::string& name, std::string_view source)
    {
        auto compiled = compile(name, source);
        if (!compiled)
            return std::unexpected(compiled.error());
        const uint64 hash = hash_name(name);
        _bytecode_by_hash[hash] = std::move(*compiled);
        _generation_by_hash.try_emplace(hash, 1);
        return {};
    }

    Result<void> Scripts::reload_source(const std::string& name, std::string_view source)
    {
        auto compiled = compile(name, source);
        if (!compiled)
            return std::unexpected(compiled.error());
        const uint64 hash = hash_name(name);
        _bytecode_by_hash[hash] = std::move(*compiled);
        ++_generation_by_hash[hash]; // live instances restart on their next update
        _events.get().script_reloaded.emit({.script_hash = hash});
        return {};
    }

    void Scripts::drop_instance(Instance& instance)
    {
        if (instance.table_ref >= 0)
            lua_unref(_lua, instance.table_ref);
        instance = Instance {};
    }

    static void call_script_function(
        lua_State* lua,
        Sandbox& sandbox,
        int table_ref,
        const char* function_name,
        entt::entity entity,
        std::optional<float> delta_time)
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

    void Scripts::update(float delta_time)
    {
        auto& registry = _sandbox.get().get_registry();
        for (const auto [entity, script] : registry.view<Script>().each())
        {
            const uint64 hash = hash_name(script.source);
            const auto bytecode = _bytecode_by_hash.find(hash);
            if (bytecode == _bytecode_by_hash.end())
                continue; // source not loaded (yet) — silently idle, assets may still stream
            const uint32 generation = _generation_by_hash[hash];

            Instance& instance = _instances[static_cast<uint32>(entity)];
            const bool is_stale = instance.table_ref < 0 || instance.script_hash != hash
                || instance.generation != generation;
            if (is_stale)
            {
                drop_instance(instance);
                if (luau_load(
                        _lua,
                        script.source.c_str(),
                        bytecode->second.data(),
                        bytecode->second.size(),
                        0)
                        != 0
                    || lua_pcall(_lua, 0, 1, 0) != 0)
                {
                    log_error("script '{}': {}", script.source, lua_tostring(_lua, -1));
                    lua_pop(_lua, 1);
                    continue;
                }
                if (!lua_istable(_lua, -1))
                {
                    log_error("script '{}' must return a table", script.source);
                    lua_pop(_lua, 1);
                    continue;
                }
                instance.table_ref = lua_ref(_lua, -1);
                lua_pop(_lua, 1);
                instance.script_hash = hash;
                instance.generation = generation;
                instance.is_started = false;
            }

            if (!instance.is_started)
            {
                instance.is_started = true;
                call_script_function(
                    _lua, _sandbox.get(), instance.table_ref, "start", entity, {});
            }
            call_script_function(
                _lua, _sandbox.get(), instance.table_ref, "update", entity, delta_time);
        }
    }
}
