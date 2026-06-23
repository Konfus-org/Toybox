#pragma once
#include "tbx/interfaces/script_instance.h"
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    class ScriptContext;

    /// @brief
    /// Purpose: Identifies a scripting language and the source-declaration file extensions it owns so
    /// the ScriptingRegistry can resolve a script asset (or source file) to its backend.
    /// @details
    /// Extensions are declaration files only (e.g. {".h"} for C++, {".lua"}), never every file
    /// involved in a build — a C++ script's .cpp is a compile input, not a tracked asset.
    struct ScriptLanguageInfo
    {
        std::string name = {};
        std::vector<std::string> extensions = {};
    };

    /// @brief
    /// Purpose: Pluggable scripting runtime. One backend per language (C++ first; Lua/C# later via
    /// plugins). Engine core knows only this interface; everything language-specific lives behind it.
    /// @details
    /// Ownership: Held by the ScriptingRegistry. A backend living in a plugin DLL must be unregistered
    /// before that plugin unloads.
    /// Thread Safety: Called from the main update thread.
    class TBX_API IScriptingBackend
    {
      public:
        virtual ~IScriptingBackend() noexcept = default;

        /// @brief The language name and declaration-file extensions this backend owns.
        virtual ScriptLanguageInfo language() const = 0;

        /// @brief Creates a runtime instance for a script asset, applying the binding's override values.
        /// Returns null when this backend does not own the asset, so the registry can try the next one.
        virtual std::shared_ptr<IScriptInstance> instantiate(
            const Handle& script,
            const Json& overrides) = 0;

        /// @brief Binds the runtime context (entity/world/services + script references) to an instance.
        /// Called each tick before the lifecycle hooks so references resolve once their targets exist.
        virtual void bind(IScriptInstance& instance, ScriptContext& context) = 0;
    };
}
