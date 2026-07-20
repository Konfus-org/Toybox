#pragma once
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// module exposes start(toy) and update(toy, delta_time) in its own language.
    struct Script
    {
        std::string source = {};
    };

    /// @brief
    /// Purpose: One scripting language. Backends coexist — C++, Lua, and C# can all run at
    /// once — so this is a real interface, not a link-time swap: each compiled-in backend
    /// (scripting/luau/, later csharp/...) implements it and claims sources by extension.
    /// @details
    /// Ownership: Owned by Scripts. Thread Safety: Main thread only.
    class ScriptBackend
    {
      public:
        virtual ~ScriptBackend() = default;

      public:
        /// @brief
        /// Purpose: The backend's display name ("luau", "csharp", ...).
        virtual std::string_view get_name() const = 0;

        /// @brief
        /// Purpose: True when this backend runs sources with the given extension (".luau").
        virtual bool owns_extension(std::string_view extension) const = 0;

        /// @brief
        /// Purpose: Compiles and caches a source under a name toys reference via Script blocks.
        virtual Result<void> load_source(const std::string& name, std::string_view source) = 0;

        /// @brief
        /// Purpose: Recompiles a source in place; live instances restart on their next update
        /// and script_reloaded fires. This IS hot reload.
        virtual Result<void> reload_source(const std::string& name, std::string_view source) = 0;

        /// @brief
        /// Purpose: Runs every scripted toy whose source this backend loaded.
        virtual void update(float delta_time) = 0;
    };

    /// @brief
    /// Purpose: The scripting coordinator: routes sources to the backend that owns their
    /// extension (extensionless names go to the first backend) and fans update() out to all.
    /// @details
    /// Ownership: Owns every backend (compiled-in ones from TBX_SCRIPTING_BACKENDS plus any the
    /// game adds). Thread Safety: Main thread only.
    class Scripts final
    {
      public:
        Scripts(Sandbox& sandbox, Events& events);

      public:
        Scripts(const Scripts&) = delete;
        Scripts& operator=(const Scripts&) = delete;

      public:
        /// @brief
        /// Purpose: Registers an additional backend (e.g. the game exe's own C++ "scripting").
        void add_backend(std::unique_ptr<ScriptBackend> backend);

        /// @brief
        /// Purpose: Routes a source to its backend by the extension in its name.
        Result<void> load_source(const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Routes a hot reload to its backend; instances restart on their next update.
        Result<void> reload_source(const std::string& name, std::string_view source);

        /// @brief
        /// Purpose: Runs every scripted toy across every backend. Called by tbx::run().
        void update(float delta_time);

      private:
        std::optional<std::reference_wrapper<ScriptBackend>> route(const std::string& name);

      private:
        std::vector<std::unique_ptr<ScriptBackend>> _backends;
    };
}
