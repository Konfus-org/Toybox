#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_backend.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/debugging/macros.h"
#include <memory>

namespace opengl_rendering
{
    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = std::ref(service_provider);

        auto context_manager = service_provider.get_service<tbx::IOpenGlContextManager>().lock();
        TBX_ASSERT(context_manager != nullptr, "OpenGL rendering plugin requires context manager.");
        if (!context_manager)
            return;

        auto backend = std::make_unique<OpenGlGraphicsBackend>(
            *context_manager);
        service_provider.register_service<tbx::IGraphicsBackend>(std::move(backend));

        auto backend_service = service_provider.get_service<tbx::IGraphicsBackend>().lock();
        auto backend_ptr = std::dynamic_pointer_cast<OpenGlGraphicsBackend>(backend_service);
        TBX_ASSERT(backend_ptr != nullptr, "OpenGL graphics backend service has unexpected type.");
        if (!backend_ptr)
            return;
        _backend = backend_ptr;
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        if (auto backend = _backend.lock())
            backend->shutdown();

        if (_service_provider.has_value()
            && _service_provider->get().has_service<tbx::IGraphicsBackend>())
            _service_provider->get().deregister_service<tbx::IGraphicsBackend>();

        _backend = {};
        _service_provider = std::nullopt;
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&) {}

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        const auto closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg);
        auto backend = _backend.lock();
        if (!closed_event.has_value() || !backend)
            return;

        backend->destroy_context(closed_event->get().window);
    }
}
