#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_backend.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/opengl_context_manager.h"
#include <memory>

namespace opengl_rendering
{
    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;

        auto backend = std::make_unique<OpenGlGraphicsBackend>(
            service_provider.get_service<tbx::IOpenGlContextManager>());
        _backend = backend.get();
        service_provider.register_service<tbx::IGraphicsBackend>(std::move(backend));
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        if (_backend)
            _backend->shutdown();

        if (_service_provider && _service_provider->has_service<tbx::IGraphicsBackend>())
            _service_provider->deregister_service<tbx::IGraphicsBackend>();

        _backend = nullptr;
        _service_provider = nullptr;
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&)
    {
    }

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        const auto* closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg);
        if (!closed_event || !_backend)
            return;

        _backend->destroy_context(closed_event->window);
    }
}
