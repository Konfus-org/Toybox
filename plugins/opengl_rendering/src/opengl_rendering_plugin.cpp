#include "opengl_rendering_plugin.h"
#include "opengl_backend.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"

namespace opengl_rendering
{
    std::shared_ptr<tbx::IGraphicsBackend> OpenGlRenderingPlugin::create_graphics_backend(
        tbx::ServiceProvider& service_provider)
    {
        auto context_backend =
            service_provider.try_get_service<tbx::IOpenGlContextBackend>().lock();
        if (!context_backend)
        {
            TBX_ASSERT(false, "OpenGL rendering plugin requires context backend.");
            return nullptr;
        }

        context_backend->initialize(
            OPENGL_MAJOR_VERSION,
            OPENGL_MINOR_VERSION,
            24,
            8,
            true,
#ifdef TBX_DEBUG
            true
#else
            false
#endif
        );

        auto backend = std::make_shared<OpenGlGraphicsBackend>(context_backend);
        _backend = backend;
        return backend;
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        _backend = {};
    }

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (const auto closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg);
            closed_event.has_value())
        {
            if (auto backend = _backend.lock())
                backend->destroy_context(closed_event->get().window);
        }
    }
}
