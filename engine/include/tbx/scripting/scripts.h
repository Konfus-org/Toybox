#pragma once
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <memory>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// module exposes start(toy) and update(toy, delta_time).
    struct Script
    {
        std::string source = {};
    };

    /// @brief
    /// Purpose: THE scripting boundary (see cmake/tbx_backend.cmake): the selected scripting
    /// backend (scripting/luau/, later maybe csharp/) implements it, and its VM types never
    /// escape it. Owns the VM and per-toy script instances.
    /// @details
    /// Ownership: Owns the backend State (RAII). Thread Safety: Main thread only.
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

      public:
        struct State; // defined by the selected scripting backend's .cpp (public so backend
                      // helpers can take State& — VM types still never leak into this header)

      private:
        std::reference_wrapper<Sandbox> _sandbox;
        std::reference_wrapper<Events> _events;
        std::unique_ptr<State> _state;
    };
}
