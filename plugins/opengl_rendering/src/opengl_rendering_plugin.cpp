#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_renderer.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"

namespace opengl_rendering
{
    OpenGlRenderingPlugin::~OpenGlRenderingPlugin() = default;

    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;

        auto& asset_manager = service_provider.get_service<tbx::AssetManager>();
        auto& job_system = service_provider.get_service<tbx::JobSystem>();
        service_provider.register_service<tbx::IGraphicsBackend>(
            std::make_unique<OpenGlRenderer>(asset_manager, job_system));
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        if (_service_provider && _service_provider->has_service<tbx::IGraphicsBackend>())
            _service_provider->deregister_service<tbx::IGraphicsBackend>();

        _service_provider = nullptr;
    }
}
