#pragma once
#include "opengl_renderer.h"
#include "tbx/app/settings.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/opengl_context_manager.h"
#include "tbx/graphics/window.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <memory>
#include <unordered_map>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Hosts OpenGL renderer instances for each window that publishes a ready context.
    /// @details
    /// Ownership: Plugin owns renderer instances and borrows host systems during attach/detach.
    /// Thread Safety: Public callbacks are expected to run on the host thread; render work is
    /// posted to a dedicated render lane.
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        ~OpenGlRenderingPlugin() override;

        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void create_renderer(const tbx::Window& window_id, const tbx::Size& viewport_size);
        void initialize_context_manager();
        void shutdown_context_manager();
        void teardown_renderer(const tbx::Window& window_id);

      private:
        std::unordered_map<tbx::Window, std::unique_ptr<OpenGlRenderer>> _renderers = {};
        tbx::AssetManager* _asset_manager = nullptr;
        tbx::EntityRegistry* _entity_registry = nullptr;
        tbx::JobSystem* _job_system = nullptr;
        tbx::IOpenGlContextManager* _open_gl_context_manager = nullptr;
        tbx::ServiceProvider* _service_provider = nullptr;
        tbx::AppSettings* _settings = nullptr;
        tbx::ThreadManager* _thread_manager = nullptr;
        tbx::IWindowManager* _window_manager = nullptr;
    };
}
