#pragma once
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <memory>
#include <unordered_map>


namespace opengl_rendering
{
    class OpenGlRenderer;

    /// <summary>
    /// Purpose: Hosts OpenGL renderer instances for each window that publishes a ready context.
    /// Ownership: Plugin owns renderer instances and borrows host systems during attach/detach.
    /// Thread Safety: Public callbacks are expected to run on the host thread; render work is posted to
    /// a dedicated render lane.
    /// </summary>
    class TBX_PLUGIN_API OpenGlRenderingPlugin final : public tbx::Plugin
    {
      public:
        ~OpenGlRenderingPlugin() override;

        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void teardown_renderer(const tbx::Uuid& window_id);

      private:
        std::unordered_map<tbx::Uuid, std::unique_ptr<OpenGlRenderer>> _renderers = {};
    };
}
