#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/scripting/scripting_registry.h"
#include <memory>

namespace cpp_scripting
{
    /// @brief
    /// Purpose: Registers the C++ scripting backend with the engine's ScriptingRegistry on attach so
    /// ScriptSystem can run compiled tbx::Script types. The first scripting-language backend; Lua/C#
    /// follow the same pattern.
    /// @details
    /// Ownership: Owns the backend instance for the plugin's lifetime; deregisters on detach so the
    /// engine drops it before this module unloads.
    /// Thread Safety: Attach/detach run on the main thread.
    [[tbx::plugin(
        name = "CppScripting",
        version = "1.0.0",
        category = tbx::PluginCategory::SCRIPTING)]];
    class TBX_PLUGIN_API CppScripting final : public tbx::Plugin
    {
      protected:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::inject]]
        std::weak_ptr<tbx::ScriptingRegistry> scripting_registry = {};

        [[tbx::inject]]
        std::weak_ptr<tbx::AssetManager> asset_manager = {};
    };
}
