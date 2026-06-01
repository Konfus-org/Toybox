#pragma once
#include "sdl_window_backend.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace sdl_windowing
{
    [[tbx::plugin(
        name = "SdlWindowingPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::INPUT)]];
    class TBX_PLUGIN_API SdlWindowingPlugin final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::register(tbx::IWindowBackend)]]
        std::shared_ptr<SdlWindowBackend> window_backend = {};
    };
}
