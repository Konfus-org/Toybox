#include "luau_bindings.h"
#include "tbx/core/log.h"
#include "tbx/scripting/scripts.h"
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <cstdlib>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: One live script attachment: the Lua module-table instance for one toy.
    struct LuauInstance
    {
        int table_ref = -1;
        uint64 script_hash = 0;
        uint32 generation = 0;
        bool is_started = false;
    };

    /// @brief
    /// Purpose: The Luau scripting backend. VM types never escape this folder; other language
    /// backends (csharp/...) sit beside it and run simultaneously.
    class LuauBackend final : public ScriptBackend
    {
      public:
        LuauBackend(Sandbox& sandbox, Events& events)
            : _sandbox(sandbox)
            , _events(events)
        {
            _lua = luaL_newstate();
            luaL_openlibs(_lua);
            open_tbx_bindings(_lua, sandbox);
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

        Result<void> load_source(const std::string& name, const std::string_view source) override
        {
            auto compiled = compile_source(name, source);
            if (!compiled)
                return std::unexpected(compiled.error());
            const uint64 hash = hash_name(name);
            _bytecode_by_hash[hash] = std::move(*compiled);
            _generation_by_hash.try_emplace(hash, 1);
            return ok();
        }

        Result<void> reload_source(const std::string& name, const std::string_view source) override
        {
            auto compiled = compile_source(name, source);
            if (!compiled)
                return std::unexpected(compiled.error());
            const uint64 hash = hash_name(name);
            _bytecode_by_hash[hash] = std::move(*compiled);
            ++_generation_by_hash[hash]; // live instances restart on their next update
            _events.get().script_reloaded.emit({.script_hash = hash});
            return ok();
        }

        void update(const float delta_time) override
        {
            auto& registry = _sandbox.get().get_registry();
            for (const auto [entity, script] : registry.view<Script>().each())
            {
                const uint64 hash = hash_name(script.source);
                const auto bytecode = _bytecode_by_hash.find(hash);
                if (bytecode == _bytecode_by_hash.end())
                    continue; // not this backend's source (another language, or still loading)
                const uint32 generation = _generation_by_hash[hash];

                LuauInstance& instance = _instances[static_cast<uint32>(entity)];
                const bool is_stale = instance.table_ref < 0 || instance.script_hash != hash
                    || instance.generation != generation;
                if (is_stale && !instantiate(instance, script.source, bytecode->second, hash, generation))
                    continue;

                if (!instance.is_started)
                {
                    instance.is_started = true;
                    call_script_function(instance.table_ref, "start", entity, {});
                }
                call_script_function(instance.table_ref, "update", entity, delta_time);
            }
        }

      private:
        Result<std::string> compile_source(const std::string& name, const std::string_view source)
        {
            auto options = lua_CompileOptions {};
            size_t bytecode_size = 0;
            char* bytecode = luau_compile(source.data(), source.size(), &options, &bytecode_size);
            auto compiled = std::string(bytecode, bytecode_size);
            std::free(bytecode);

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
            const uint64 hash,
            const uint32 generation)
        {
            if (instance.table_ref >= 0)
                lua_unref(_lua, instance.table_ref);
            instance = LuauInstance {};

            if (luau_load(_lua, source_name.c_str(), bytecode.data(), bytecode.size(), 0) != 0
                || lua_pcall(_lua, 0, 1, 0) != 0)
            {
                log_error("script '{}': {}", source_name, lua_tostring(_lua, -1));
                lua_pop(_lua, 1);
                return false;
            }
            if (!lua_istable(_lua, -1))
            {
                log_error("script '{}' must return a table", source_name);
                lua_pop(_lua, 1);
                return false;
            }
            instance.table_ref = lua_ref(_lua, -1);
            lua_pop(_lua, 1);
            instance.script_hash = hash;
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
            push_toy(_lua, _sandbox.get(), entity);
            int argument_count = 1;
            if (delta_time)
            {
                lua_pushnumber(_lua, *delta_time);
                argument_count = 2;
            }
            if (lua_pcall(_lua, argument_count, 0, 0) != 0)
            {
                log_error("script error in {}: {}", function_name, lua_tostring(_lua, -1));
                lua_pop(_lua, 1);
            }
            lua_pop(_lua, 1); // the instance table
        }

      private:
        std::reference_wrapper<Sandbox> _sandbox;
        std::reference_wrapper<Events> _events;
        lua_State* _lua = nullptr; // owned; closed in the destructor (C boundary)
        std::unordered_map<uint64, std::string> _bytecode_by_hash;
        std::unordered_map<uint64, uint32> _generation_by_hash;
        std::unordered_map<uint32, LuauInstance> _instances; // keyed by ToyId value
    };

    std::unique_ptr<ScriptBackend> make_luau_backend(Sandbox& sandbox, Events& events)
    {
        return std::make_unique<LuauBackend>(sandbox, events);
    }
}
