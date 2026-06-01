#include "sdl_opengl_context_manager_plugin.h"
#include "tbx/interfaces/opengl_context_backend.h"

namespace sdl_opengl_context_manager
{
    std::shared_ptr<tbx::IOpenGlContextBackend> SdlOpenGlContextManagerPlugin::
        create_context_backend(tbx::ServiceProvider&)
    {
        _context_backend = std::make_shared<SdlOpenGlContextManager>();
        return _context_backend;
    }

    void SdlOpenGlContextManagerPlugin::on_detach()
    {
        if (_context_backend)
            _context_backend->shutdown();
        _context_backend = {};
    }
}
