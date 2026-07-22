#include "luau_bindings.h"
#include "tbx/runtime.h"
#include "tbx/debug/log.h"
#include "tbx/scripting/scripts.h"
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <cstdlib>
#include <memory>
#include <unordered_map>

namespace tbx::scripts
{
    /// @brief
    /// Purpose: One compiled source this backend runs: bytecode plus its diagnostics name and
    /// a reload generation live instances compare against — one map entry, one lookup.
    struct CompiledScript
    {
        std::string bytecode = {};
        std::string name = {};
        uint32 generation = 1;
    };

    /// @brief
    /// Purpose: One live script attachment: the Lua module-table instance for one toy.
    struct LuauInstance
    {
        int table_ref = -1;
        Uuid script_id = {};
        uint32 generation = 0;
        bool is_started = false;
    };

    /// @brief
    /// Purpose: The Luau scripting backend. VM types never escape this folder; other language
    /// backends (csharp/...) sit beside it and run simultaneously.
    class LuauBackend final : public ScriptBackend
    {
      public:
        explicit LuauBackend(RuntimeState& runtime)
            : _runtime(runtime)
        {
            _lua = luaL_newstate();
            luaL_openlibs(_lua);
            open_tbx_bindings(_lua, runtime);
        }

        ~LuauBackend() override
        {
            lua_close(_lua); // releases every instance ref with it
        }

      public:
        LuauBackend(const LuauBackend&) = delete;
        LuauBackend& operator=(const LuauBackend&) = delete;

      public:
        std::string_view get_name() const override
        {
            return "luau";
        }

        bool owns_extension(const std::string_view extension) const override
        {
            return extension == ".luau" || extension == ".lua";
        }

        Result<void> load_source(
            const Uuid& id,
            const std::string& name,
            const std::string_view source) override
        {
            auto compiled = compile_source(name, source);
            if (!compiled)
                return std::unexpected(compiled.error());
            _scripts_by_id[id] = CompiledScript {.bytecode = std::move(*compiled), .name = name};
            return ok();
        }

        Result<void> reload_source(
            const Uuid& id,
            const std::string& name,
            const std::string_view source) override
        {
            auto compiled = compile_source(name, source);
            if (!compiled)
                return std::unexpected(compiled.error());
            CompiledScript& script = _scripts_by_id[id];
            const uint32 generation = script.generation + 1;
            script = CompiledScript {
                .bytecode = std::move(*compiled),
                .name = name,
                // Live instances restart on their next update.
                .generation = generation};
            _runtime.get().events.script_reloaded.emit({.id = id});
            return ok();
        }

        void update(const float delta_time) override
        {
            run_scripts("update", delta_time);
        }

        void fixed_update(const float fixed_delta_time) override
        {
            run_scripts("fixed_update", fixed_delta_time);
        }

      private:
        void run_scripts(const char* function_name, const float delta_time)
        {
            auto& registry = _runtime.get().sandbox.get_registry();
            for (const auto [entity, script] : registry.view<Script>().each())
            {
                if (!registry.get<ToyHandle>(entity).is_enabled)
                    continue;
                const Uuid id = script.source.id;
                const auto found = _scripts_by_id.find(id);
                if (found == _scripts_by_id.end())
                    continue; // not this backend's source (another language, or still loading)
                const CompiledScript& source = found->second;

                LuauInstance& instance = _instances[static_cast<uint32>(entity)];
                const bool is_stale = instance.table_ref < 0 || instance.script_id != id
                    || instance.generation != source.generation;
                if (is_stale
                    && !instantiate(instance, source.name, source.bytecode, id, source.generation))
                    continue;

                if (!instance.is_started)
                {
                    instance.is_started = true;
                    call_script_function(instance.table_ref, "start", entity, {});
                }
                call_script_function(instance.table_ref, function_name, entity, delta_time);
            }
        }

        Result<std::string> compile_source(const std::string& name, const std::string_view source)
        {
            auto options = lua_CompileOptions {};
            size_t bytecode_size = 0;
            // C boundary: luau_compile returns a malloc'd buffer the caller must free.
            const auto bytecode = std::unique_ptr<char, decltype(&std::free)>(
                luau_compile(source.data(), source.size(), &options, &bytecode_size),
                &std::free);
            auto compiled = std::string(bytecode.get(), bytecode_size);

            // Compile errors only surface at load time — validate now so callers hear them.
            if (luau_load(_lua, name.c_str(), compiled.data(), compiled.size(), 0) != 0)
            {
                auto error =
                    fail("script '{}' failed to compile: {}", name, lua_tostring(_lua, -1));
                lua_pop(_lua, 1);
                return error;
            }
            lua_pop(_lua, 1); // the validated closure
            return compiled;
        }

        bool instantiate(
            LuauInstance& instance,
            const std::string& source_name,
            const std::string& bytecode,
            const Uuid& id,
            const uint32 generation)
        {
            if (instance.table_ref >= 0)
                lua_unref(_lua, instance.table_ref);
            instance = LuauInstance {};

            // Each instance runs its chunk in a fresh environment (falling through to the
            // globals), so scripts just define start/update/fixed_update — no module table.
            lua_newtable(_lua); // the environment
            lua_newtable(_lua); // its metatable
            lua_pushvalue(_lua, LUA_GLOBALSINDEX);
            lua_setfield(_lua, -2, "__index");
            lua_setmetatable(_lua, -2);
            const int environment = lua_gettop(_lua);

            if (luau_load(_lua, source_name.c_str(), bytecode.data(), bytecode.size(), environment)
                    != 0
                || lua_pcall(_lua, 0, 0, 0) != 0)
            {
                TBX_ERROR("script '{}': {}", source_name, lua_tostring(_lua, -1));
                lua_pop(_lua, 2); // error + environment
                return false;
            }
            instance.table_ref = lua_ref(_lua, environment);
            lua_pop(_lua, 1); // the environment
            instance.script_id = id;
            instance.generation = generation;
            instance.is_started = false;
            return true;
        }

        void call_script_function(
            const int table_ref,
            const char* function_name,
            const ToyId entity,
            const std::optional<float> delta_time)
        {
            lua_getref(_lua, table_ref);
            lua_getfield(_lua, -1, function_name);
            if (!lua_isfunction(_lua, -1))
            {
                lua_pop(_lua, 2);
                return;
            }
            push_toy(_lua, _runtime.get().sandbox, entity);
            int argument_count = 1;
            if (delta_time)
            {
                lua_pushnumber(_lua, *delta_time);
                argument_count = 2;
            }
            if (lua_pcall(_lua, argument_count, 0, 0) != 0)
            {
                TBX_ERROR("script error in {}: {}", function_name, lua_tostring(_lua, -1));
                lua_pop(_lua, 1);
            }
            lua_pop(_lua, 1); // the instance table
        }

      private:
        std::reference_wrapper<RuntimeState> _runtime;
        lua_State* _lua = nullptr; // owned; closed in the destructor (C boundary)
        std::unordered_map<Uuid, CompiledScript> _scripts_by_id;
        std::unordered_map<uint32, LuauInstance> _instances; // keyed by ToyId value
    };



    std::unique_ptr<ScriptBackend> make_luau_backend(RuntimeState& runtime)
    {
        return std::make_unique<LuauBackend>(runtime);
    }
}
