#pragma once
#include "opengl_renderer.h"
#include "tbx/app/settings.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/render_pipeline.h"
#include "tbx/graphics/window.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <memory>
#include <unordered_map>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Hosts OpenGL renderer instances for each open window while OpenGL is active.
    /// @details
    /// Ownership: Plugin owns renderer instances and borrows host systems during attach/detach.
    /// Thread Safety: Public callbacks are expected to run on the host thread; render work is
    /// posted to a dedicated render lane.
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        ~OpenGlRenderingPlugin() override;

      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void ensure_renderer(const tbx::Window& window_id);
        void rebuild_renderer(const tbx::Window& window_id);
        void sync_open_windows();
        void teardown_renderer(const tbx::Window& window_id);

      private:
        std::unordered_map<tbx::Window, std::unique_ptr<OpenGlRenderer>> _renderers = {};
        tbx::AssetManager* _asset_manager = nullptr;
        tbx::IGraphicsContextManager* _context_manager = nullptr;
        tbx::EntityRegistry* _entity_registry = nullptr;
        tbx::JobSystem* _job_system = nullptr;
        tbx::AppSettings* _settings = nullptr;
        tbx::ThreadManager* _thread_manager = nullptr;
        tbx::IWindowManager* _window_manager = nullptr;
    };
}
