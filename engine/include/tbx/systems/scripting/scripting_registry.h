#pragma once
#include "tbx/interfaces/scripting_backend.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <memory>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Engine-owned directory of scripting backends. Backends (engine-first-class or
    /// plugin-provided) register here keyed by an owner id so a plugin can bulk-deregister on detach,
    /// mirroring the IRpcRouter registration pattern.
    /// @details
    /// Ownership: Holds shared_ptr<IScriptingBackend>. A backend whose code lives in a plugin DLL must
    /// be unregistered (unregister_all in on_detach) before that plugin unloads, and ScriptSystem must
    /// drop any instances it created beforehand.
    /// Thread Safety: Not thread-safe; mutated and queried from the main thread.
    class TBX_API ScriptingRegistry
    {
      public:
        // Registers a backend. Fails on a null backend or a duplicate language name.
        Result register_backend(std::shared_ptr<IScriptingBackend> backend, Uuid owner);

        // Drops every backend registered under the given owner id.
        void unregister_all(Uuid owner);

        std::vector<std::shared_ptr<IScriptingBackend>> backends() const;
        std::weak_ptr<IScriptingBackend> for_language(std::string_view name) const;
        std::weak_ptr<IScriptingBackend> for_extension(std::string_view extension) const;

      private:
        struct Entry
        {
            Uuid owner = {};
            std::shared_ptr<IScriptingBackend> backend = {};
        };

        std::vector<Entry> _backends = {};
    };
}
