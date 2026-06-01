#pragma once
#include "sdl_opengl_context_manager.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace sdl_opengl_context_manager
{
    /// @brief
    /// Purpose: Registers and feeds the SDL OpenGL context manager service.
    /// @details
    /// Ownership: Shares the registered context manager with the service provider.
    /// Thread Safety: Expected to be attached/detached on the main thread.
    [[tbx::plugin(
        name = "SdlOpenGlContextManagerPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::RENDERING,
        dependencies = {"SdlBaseSystemsPlugin", "SdlWindowingPlugin"})]];
    [[tbx::register(tbx::IOpenGlContextBackend, create_context_backend)]];
    class TBX_PLUGIN_API SdlOpenGlContextManagerPlugin final : public tbx::Plugin
    {
      public:
        void on_detach() override;

      public:
        std::shared_ptr<tbx::IOpenGlContextBackend> create_context_backend(
            tbx::ServiceProvider& service_provider);

      private:
        std::weak_ptr<SdlOpenGlContextManager> _context_backend = {};
    };
}
