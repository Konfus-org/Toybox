#pragma once
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <string>
#include <unordered_map>

struct lua_State;

namespace tbx
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded .luau source. A script
    /// module returns a table: { start(toy), update(toy, delta_time) }.
    struct Script
    {
        std::string source = {};
    };

    /// @brief
    /// Purpose: THE scripting system — Luau is the game scripting language; C++ is for systems
    /// in the game executable. Owns the VM and per-toy script instances.
    /// @details
    /// Ownership: Owns the lua_State (RAII). Thread Safety: Main thread only.
    class Scripts final
    {
      public:
        Scripts(Sandbox& sandbox, Events& events);
        ~Scripts();

      public:
        Scripts(const Scripts&) = delete;
        Scripts& operator=(const Scripts&) = delete;

      public:
        /// @brief
        /// Purpose: Compiles and caches a script source under a name toys reference via their
        /// Script block.
        Result<void> load_source(const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Recompiles a source in place: live instances restart (fresh start()) on
        /// their next update, and script_reloaded fires. This IS hot reload — the file watcher
        /// simply calls it.
        Result<void> reload_source(const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Runs every scripted toy: start() once, then update(toy, delta_time) each
        /// frame. Called by Engine::update().
        void update(float delta_time);

      private:
        struct Instance
        {
            int table_ref = -1;
            uint64 script_hash = 0;
            uint32 generation = 0;
            bool is_started = false;
        };

      private:
        Result<std::string> compile(const std::string& name, std::string_view source);
        void drop_instance(Instance& instance);

      private:
        std::reference_wrapper<Sandbox> _sandbox;
        std::reference_wrapper<Events> _events;
        lua_State* _lua = nullptr; // owned; closed in the destructor
        std::unordered_map<uint64, std::string> _bytecode_by_hash;
        std::unordered_map<uint64, uint32> _generation_by_hash;
        std::unordered_map<uint32, Instance> _instances; // keyed by entt::entity value
    };
}
