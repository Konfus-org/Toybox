#pragma once
#include "tbx/interfaces/scripting_backend.h"
#include <memory>

namespace tbx
{
    class AssetManager;
}

namespace cpp_scripting
{
    /// @brief
    /// Purpose: The C++ scripting backend. Instantiates compiled tbx::Script types (registered via
    /// codegen in whatever module compiles them), applies per-binding overrides, and binds runtime
    /// context + script references.
    /// @details
    /// The one backend that runs compiled code rather than reading source: a C++ script asset's .h is
    /// its identity, but its executable type comes from the module that compiled it. The backend carries
    /// no script types — it drives the engine asset registry (create/clone) and the engine script
    /// registry (apply/bind). Plugin-internal: the cpp_scripting plugin constructs it and registers it
    /// with the engine's ScriptingRegistry through the IScriptingBackend interface.
    class CppScriptingBackend final : public tbx::IScriptingBackend
    {
      public:
        explicit CppScriptingBackend(std::weak_ptr<tbx::AssetManager> asset_manager);

        tbx::ScriptLanguageInfo language() const override;
        std::shared_ptr<tbx::IScriptInstance> instantiate(
            const tbx::Handle& script,
            const tbx::Json& overrides) override;
        void bind(tbx::IScriptInstance& instance, tbx::ScriptContext& context) override;

      private:
        std::weak_ptr<tbx::AssetManager> _asset_manager = {};
    };
}
