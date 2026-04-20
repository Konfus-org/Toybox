#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Registers the OpenGL graphics backend and engine-owned rendering service.
    /// @details
    /// Ownership: Services are registered into the host service provider during plugin attach.
    /// Thread Safety: Expected to be attached and detached on the host thread.
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        ~OpenGlRenderingPlugin() override;

      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        tbx::ServiceProvider* _service_provider = nullptr;
    };
}
