#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager_plugin.h"
#include "tbx/graphics/opengl_context_manager.h"
#include <memory>

namespace sdl_opengl_context_manager
{
    void SdlOpenGlContextManagerPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;
        auto& window_manager = service_provider.get_service<tbx::IWindowManager>();

        auto context_manager = std::make_unique<SdlOpenGlContextManager>(window_manager);
        _context_manager = context_manager.get();
        service_provider.register_service<tbx::IOpenGlContextManager>(std::move(context_manager));
    }

    void SdlOpenGlContextManagerPlugin::on_detach()
    {
        if (_context_manager)
            _context_manager->shutdown();

        if (_service_provider)
            _service_provider->deregister_service<tbx::IOpenGlContextManager>();

        _context_manager = nullptr;
        _service_provider = nullptr;
    }
}
