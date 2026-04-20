#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_renderer.h"
#include "tbx/app/settings.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/messages/dispatcher.h"

namespace opengl_rendering
{
    OpenGlRenderingPlugin::~OpenGlRenderingPlugin() = default;

    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;

        auto& message_coordinator = service_provider.get_service<tbx::IMessageCoordinator>();
        auto& thread_manager = service_provider.get_service<tbx::ThreadManager>();
        auto& entity_registry = service_provider.get_service<tbx::EntityRegistry>();
        auto& asset_manager = service_provider.get_service<tbx::AssetManager>();
        auto& job_system = service_provider.get_service<tbx::JobSystem>();
        auto& settings = service_provider.get_service<tbx::AppSettings>().graphics;
        auto& window_manager = service_provider.get_service<tbx::IWindowManager>();
        service_provider.register_service<tbx::IGraphicsBackend>(
            std::make_unique<OpenGlRenderer>(asset_manager, job_system));

        auto& backend = service_provider.get_service<tbx::IGraphicsBackend>();
        service_provider.register_service<tbx::IRendering>(std::make_unique<tbx::Rendering>(
            message_coordinator,
            thread_manager,
            entity_registry,
            asset_manager,
            job_system,
            settings,
            window_manager,
            service_provider.get_service<tbx::IGraphicsContextManager>(),
            backend));
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        if (_service_provider && _service_provider->has_service<tbx::IRendering>())
            _service_provider->deregister_service<tbx::IRendering>();
        if (_service_provider && _service_provider->has_service<tbx::IGraphicsBackend>())
            _service_provider->deregister_service<tbx::IGraphicsBackend>();

        _service_provider = nullptr;
    }
}
