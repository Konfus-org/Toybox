#include "sdl_opengl_context_manager_plugin.h"
#include "tbx/interfaces/opengl_context_backend.h"

namespace sdl_opengl_context_manager
{
    std::shared_ptr<tbx::IOpenGlContextBackend> SdlOpenGlContextManager::
        create_context_backend(tbx::ServiceProvider&)
    {
        auto context_backend = std::make_shared<SdlOpenGlContextBackend>();
        _context_backend = context_backend;
        return context_backend;
    }

    void SdlOpenGlContextManager::on_detach()
    {
        if (auto context_backend = _context_backend.lock())
            context_backend->shutdown();
        _context_backend = {};
    }
}
