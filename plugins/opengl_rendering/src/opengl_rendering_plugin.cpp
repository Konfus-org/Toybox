#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_backend.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"

namespace opengl_rendering
{
    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto context_backend = service_provider.get_service<tbx::IOpenGlContextBackend>().lock();
        TBX_ASSERT(context_backend != nullptr, "OpenGL rendering plugin requires context backend.");
        if (!context_backend)
            return;

        context_backend->initialize(4, 5, 24, 8, true, false);

        auto backend = std::make_unique<OpenGlGraphicsBackend>(*context_backend);
        service_provider.register_service<tbx::IGraphicsBackend>(std::move(backend));

        auto backend_service = service_provider.get_service<tbx::IGraphicsBackend>().lock();
        auto backend_ptr = std::dynamic_pointer_cast<OpenGlGraphicsBackend>(backend_service);
        TBX_ASSERT(backend_ptr != nullptr, "OpenGL graphics backend service has unexpected type.");
        if (!backend_ptr)
            return;
        _backend = backend_ptr;
    }

    void OpenGlRenderingPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (service_provider.has_service<tbx::IGraphicsBackend>())
            service_provider.deregister_service<tbx::IGraphicsBackend>();

        _backend = {};
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&) {}

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        auto backend = _backend.lock();
        if (const auto closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg);
            closed_event.has_value() && backend)
        {
            backend->destroy_context(closed_event->get().window);
        }
    }
}
