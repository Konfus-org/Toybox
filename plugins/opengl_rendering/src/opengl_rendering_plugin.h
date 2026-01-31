#pragma once
#include "opengl_render_pipeline.h"
#include "tbx/app/application.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/window.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>OpenGL rendering backend plugin.</summary>
    /// <remarks>
    /// Purpose: Implements GPU resource creation and frame rendering for OpenGL.
    /// Ownership: Owns OpenGL resources created through this backend.
    /// Thread Safety: Not thread-safe; call from the render thread.
    /// </remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_recieve_message(Message& msg) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        void handle_window_ready(WindowContextReadyEvent& event);
        void handle_window_open_changed(PropertyChangedEvent<Window, bool>& event);
        void handle_window_resized(PropertyChangedEvent<Window, Size>& event);
        void handle_resolution_changed(PropertyChangedEvent<AppSettings, Size>& event);
        void initialize_opengl() const;
        void remove_window_state(const Uuid& window_id, bool try_release);

      private:
        bool _is_gl_ready = false;
        std::unordered_map<Uuid, Size> _window_sizes = {};
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = {};
    };
}
