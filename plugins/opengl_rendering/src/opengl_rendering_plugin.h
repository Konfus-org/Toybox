#pragma once
#include "opengl_backend.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Registers the OpenGL implementation of the graphics backend service.
    /// @details
    /// Ownership: Shares the registered backend with the service provider.
    /// Thread Safety: Plugin lifecycle methods are expected to run on the host thread.
    [[tbx::plugin(
        name = "OpenGlRenderingPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::RENDERING,
        dependencies = {"SdlOpenGlContextManagerPlugin"})]];
    [[tbx::register(tbx::IGraphicsBackend, create_graphics_backend)]];
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      public:
        std::shared_ptr<tbx::IGraphicsBackend> create_graphics_backend(
            tbx::ServiceProvider& service_provider);

      private:
        std::shared_ptr<OpenGlGraphicsBackend> _backend = {};
    };
}
