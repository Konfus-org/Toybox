#include "luau_bindings.h"
#include "events/events_internal.h"
#include "tbx/debug/log.h"
#include "runtime_state.h"
#include "tbx/scripting/scripts.h"
#include <cstdlib>
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <memory>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: One compiled source this backend runs — its bytecode plus a diagnostics name.
    /// compile_script fills it once per source (again on reload); instances load the bytecode.
    struct CompiledScript
    {
        std::string bytecode = {};
        std::string name = {}; // chunk name for traces ("player.luau:12")
    };

    /// @brief
    /// Purpose: One live script attachment: the Lua environment-table instance for one toy. The
    /// coordinator owns start-once and teardown, so the instance carries no lifecycle flags — just
    /// its ref and which source built it.
    struct LuauInstance
    {
        int table_ref = -1;
        Uuid script_id = {};
    };

    /// @brief
    /// Purpose: The Luau scripting backend. VM types never escape this folder; other language
    /// backends (csharp/...) sit beside it and run simultaneously.
    class LuauBackend final : public ScriptBackend
    {
      public:
        explicit LuauBackend(internal::RuntimeState& runtime)
            : _runtime(runtime)
        {
            _lua = luaL_newstate();
            luaL_openlibs(_lua);
            open_tbx_bindings(_lua, runtime);
        }

        ~LuauBackend() override
        {
            // The coordinator fires cleanup + purge_script before shutdown (purge_scripts), so by
            // here instances are just memory. Drop every script-registered event handler before the
            // VM dies: they capture this lua_State, so any that survived to a later dispatch would
            // call into freed memory.
            internal::unsubscribe_all(_runtime.get().events, _lua);
            lua_close(_lua); // releases any instance ref still pinned with it
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

        Result<void> compile_script(
            const Uuid& id,
            const std::string& name,
            const std::string_view source) override
        {
            // Compile eagerly so a syntax error is caught here (and a bad reload keeps the old
            // bytecode, since we only overwrite on success). The coordinator restarts live
            // instances after a reload by flagging the source.
            auto compiled = compile_source(name, source);
            if (!compiled)
                return std::unexpected(compiled.error());
            _scripts_by_id[id] = CompiledScript {.bytecode = std::move(*compiled), .name = name};
            return ok();
        }

        // Each hook maps to the Luau name a script defines: "start" / "update" / "fixedUpdate" /
        // "cleanup". Another backend (C#, C++) would map the same hooks to its own convention. The
        // coordinator owns when each fires — start once on first sight, cleanup once on the way
        // out.

        void call_script_start(Toy toy, const Uuid& source_id) override
        {
            const auto found = _scripts_by_id.find(source_id);
            if (found == _scripts_by_id.end())
                return; // another language's source
            const uint32 key = static_cast<uint32>(toy.get_id());
            LuauInstance& instance = _instances[key];
            if (!instantiate(instance, found->second.name, found->second.bytecode, source_id))
            {
                _instances.erase(key); // don't leave an empty slot behind on a build failure
                return;
            }
            call_script_function(instance.table_ref, "start", toy.get_id(), {});
        }

        void call_script_update(Toy toy, const Uuid& source_id, const float delta_time) override
        {
            if (LuauInstance* instance = find_instance(toy, source_id))
                call_script_function(instance->table_ref, "update", toy.get_id(), delta_time);
        }

        void call_script_fixed_update(Toy toy, const Uuid& source_id, const float fixed_delta_time)
            override
        {
            if (LuauInstance* instance = find_instance(toy, source_id))
                call_script_function(
                    instance->table_ref,
                    "fixedUpdate",
                    toy.get_id(),
                    fixed_delta_time);
        }

        void call_script_cleanup(Toy toy, const Uuid& source_id) override
        {
            // Fire the hook only — the coordinator calls purge_script right after to free the
            // memory. A removed toy passes a dead handle here, so scripts guard with
            // toy:is_alive().
            if (LuauInstance* instance = find_instance(toy, source_id))
                call_script_function(instance->table_ref, "cleanup", toy.get_id(), {});
        }

        void purge_script(Toy toy, const Uuid& source_id) override
        {
            // Free the instance's memory: an instance pins its env table with a lua_ref (a GC
            // root), so it can never collect on its own — drop the ref, erase, and step the
            // collector. No hook runs here; call_script_cleanup already fired if it needed to.
            const auto found = _instances.find(static_cast<uint32>(toy.get_id()));
            if (found == _instances.end() || found->second.script_id != source_id)
                return;
            if (found->second.table_ref >= 0)
                lua_unref(_lua, found->second.table_ref);
            _instances.erase(found);
            lua_gc(_lua, LUA_GCSTEP, 0);
        }

      private:
        Result<std::string> compile_source(const std::string& name, const std::string_view source)
        {
            auto options = lua_CompileOptions {};
            size_t bytecode_size = 0;
            // C boundary: luau_compile returns a malloc'd buffer the caller must free.
            const auto bytecode = std::unique_ptr<char, decltype(&std::free)>(
                luau_compile(source.data(), source.size(), &options, &bytecode_size),
                &std::free);
            auto compiled = std::string(bytecode.get(), bytecode_size);

            // A syntax error only surfaces at load time — validate now so the first start reports
            // it. '@' marks the chunk name a file path, so diagnostics and print() report it as a
            // clean "player.luau:12" rather than Lua's [string "..."] wrapper.
            const std::string chunk_name = "@" + name;
            if (luau_load(_lua, chunk_name.c_str(), compiled.data(), compiled.size(), 0) != 0)
            {
                auto error =
                    fail("script '{}' failed to compile: {}", name, lua_tostring(_lua, -1));
                lua_pop(_lua, 1);
                return error;
            }
            lua_pop(_lua, 1); // the validated closure
            return compiled;
        }

        // The instance for this source on this toy, or nullptr — never instantiates
        // (call_script_start owns that). update / fixed / cleanup dispatch through it.
        LuauInstance* find_instance(Toy toy, const Uuid& source_id)
        {
            const auto found = _instances.find(static_cast<uint32>(toy.get_id()));
            if (found == _instances.end() || found->second.table_ref < 0
                || found->second.script_id != source_id)
                return nullptr;
            return &found->second;
        }

        bool instantiate(
            LuauInstance& instance,
            const std::string& source_name,
            const std::string& bytecode,
            const Uuid& id)
        {
            // Release any previous table (a re-start after a source change on this toy) before
            // rebuilding — memory only; cleanup is the coordinator's business.
            if (instance.table_ref >= 0)
                lua_unref(_lua, instance.table_ref);
            instance = LuauInstance {};

            // Each instance runs its chunk in a fresh environment (falling through to the
            // globals), so scripts just define start/update/fixedUpdate/cleanup — no module table.
            lua_newtable(_lua); // the environment
            lua_newtable(_lua); // its metatable
            lua_pushvalue(_lua, LUA_GLOBALSINDEX);
            lua_setfield(_lua, -2, "__index");
            lua_setmetatable(_lua, -2);
            const int environment = lua_gettop(_lua);

            const std::string chunk_name = "@" + source_name; // '@' => clean file source in traces
            if (luau_load(_lua, chunk_name.c_str(), bytecode.data(), bytecode.size(), environment)
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
        std::reference_wrapper<internal::RuntimeState> _runtime;
        lua_State* _lua = nullptr; // owned; closed in the destructor (C boundary)
        std::unordered_map<Uuid, CompiledScript> _scripts_by_id;
        std::unordered_map<uint32, LuauInstance> _instances; // keyed by ToyId value
    };

    std::unique_ptr<ScriptBackend> make_luau_backend(internal::RuntimeState& runtime)
    {
        return std::make_unique<LuauBackend>(runtime);
    }
}
