#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace sdl_opengl_context_manager
{
    /// @brief
    /// Purpose: Registers and feeds the SDL OpenGL context manager service.
    /// @details
    /// Ownership: The service provider owns the registered context manager instance.
    /// Thread Safety: Expected to be attached/detached on the main thread.
    [[tbx::plugin]];
    [[tbx::name("SdlOpenGlContextManagerPlugin")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("rendering")]];
    [[tbx::dependency("SdlBaseSystemsPlugin")]];
    [[tbx::dependency("SdlWindowingPlugin")]];
    class TBX_PLUGIN_API SdlOpenGlContextManagerPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;

      private:
        std::weak_ptr<SdlOpenGlContextManager> _context_backend = {};
    };
}
