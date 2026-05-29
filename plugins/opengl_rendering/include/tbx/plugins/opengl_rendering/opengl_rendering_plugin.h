#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"

namespace tbx
{
    class IGraphicsBackend;
}

namespace opengl_rendering
{
    class OpenGlGraphicsBackend;

    /// @brief
    /// Purpose: Registers the OpenGL implementation of the graphics backend service.
    /// @details
    /// Ownership: Borrows the host service provider and lets it own the registered backend.
    /// Thread Safety: Plugin lifecycle methods are expected to run on the host thread.
    [[tbx::plugin]] [[tbx::name("OpenGlRenderingPlugin")]] [[tbx::version("1.0.0")]]
    [[tbx::category("rendering")]] [[tbx::priority(100)]] [[tbx::dependency(
        "SdlOpenGlContextManagerPlugin")]];
    class TBX_PLUGIN_API OpenGlRenderingPlugin final
        : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        std::weak_ptr<OpenGlGraphicsBackend> _backend = {};
    };
}
