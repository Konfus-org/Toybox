#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager_plugin.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_opengl_context_manager
{
    void SdlOpenGlContextManagerPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto window_manager = service_provider.get_service<tbx::IWindowManager>().lock();
        if (!window_manager)
            return;

        auto context_backend = std::make_unique<SdlOpenGlContextManager>(
            service_provider.get_service<tbx::IWindowManager>());
        service_provider.register_service<tbx::IOpenGlContextBackend>(std::move(context_backend));

        auto context_backend_service =
            service_provider.get_service<tbx::IOpenGlContextBackend>().lock();
        auto context_backend_ptr =
            std::dynamic_pointer_cast<SdlOpenGlContextManager>(context_backend_service);
        TBX_ASSERT(
            context_backend_ptr != nullptr,
            "SDL OpenGL context backend service has unexpected type.");
        _context_backend = context_backend_ptr;
    }

    void SdlOpenGlContextManagerPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (auto context_backend = _context_backend.lock())
            context_backend->shutdown();

        if (service_provider.has_service<tbx::IOpenGlContextBackend>())
            service_provider.deregister_service<tbx::IOpenGlContextBackend>();

        _context_backend = {};
    }
}
