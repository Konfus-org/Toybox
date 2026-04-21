#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager.h"

namespace sdl_opengl_context_manager
{
    /// @brief
    /// Purpose: Registers and feeds the SDL OpenGL context manager service.
    /// @details
    /// Ownership: The service provider owns the registered context manager instance.
    /// Thread Safety: Expected to be attached/detached on the main thread.
    class TBX_PLUGIN_API SdlOpenGlContextManagerPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        SdlOpenGlContextManager* _context_manager = nullptr;
        tbx::ServiceProvider* _service_provider = nullptr;
    };
}
