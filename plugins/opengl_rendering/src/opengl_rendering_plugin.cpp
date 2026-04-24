#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_backend.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include "tbx/systems/graphics/messages.h"
#include <memory>

namespace opengl_rendering
{
    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = std::ref(service_provider);

        auto backend = std::make_unique<OpenGlGraphicsBackend>(
            service_provider.get_service<tbx::IOpenGlContextManager>());
        _backend = std::ref(*backend);
        service_provider.register_service<tbx::IGraphicsBackend>(std::move(backend));
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        if (_backend.has_value())
            _backend->get().shutdown();

        if (_service_provider.has_value()
            && _service_provider->get().has_service<tbx::IGraphicsBackend>())
            _service_provider->get().deregister_service<tbx::IGraphicsBackend>();

        _backend = std::nullopt;
        _service_provider = std::nullopt;
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&) {}

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        const auto closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg);
        if (!closed_event.has_value() || !_backend.has_value())
            return;

        _backend->get().destroy_context(closed_event->get().window);
    }
}
