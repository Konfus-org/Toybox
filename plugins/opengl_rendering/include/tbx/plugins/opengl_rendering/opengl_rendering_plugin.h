#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <functional>
#include <optional>

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
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        std::optional<std::reference_wrapper<OpenGlGraphicsBackend>> _backend = std::nullopt;
        std::optional<std::reference_wrapper<tbx::ServiceProvider>> _service_provider =
            std::nullopt;
    };
}
