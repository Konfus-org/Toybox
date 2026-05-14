#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager_plugin.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include "tbx/systems/debugging/macros.h"
#include <memory>

namespace sdl_opengl_context_manager
{
    void SdlOpenGlContextManagerPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto window_manager = service_provider.get_service<tbx::IWindowManager>().lock();
        if (!window_manager)
            return;

        auto context_manager = std::make_unique<SdlOpenGlContextManager>(*window_manager);
        service_provider.register_service<tbx::IOpenGlContextManager>(std::move(context_manager));

        auto context_manager_service =
            service_provider.get_service<tbx::IOpenGlContextManager>().lock();
        auto context_manager_ptr =
            std::dynamic_pointer_cast<SdlOpenGlContextManager>(context_manager_service);
        TBX_ASSERT(
            context_manager_ptr != nullptr,
            "SDL OpenGL context manager service has unexpected type.");
        _context_manager = context_manager_ptr;
    }

    void SdlOpenGlContextManagerPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (auto context_manager = _context_manager.lock())
            context_manager->shutdown();

        if (service_provider.has_service<tbx::IOpenGlContextManager>())
            service_provider.deregister_service<tbx::IOpenGlContextManager>();

        _context_manager = {};
    }
}
