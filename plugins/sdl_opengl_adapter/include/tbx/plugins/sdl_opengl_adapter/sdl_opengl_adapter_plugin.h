#pragma once
#include "tbx/graphics/events.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include "tbx/plugins/sdl_opengl_adapter/sdl_open_gl_adapter.h"

namespace sdl_opengl_adapter
{
    /// @brief
    /// Purpose: Registers the SDL-backed OpenGL context manager service for rendering plugins.
    /// @details
    /// Ownership: Registers the adapter into the host service provider and borrows it thereafter.
    /// Thread Safety: Expected to be attached, updated, and detached on the host thread.
    class TBX_PLUGIN_API SdlOpenGlAdapterPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void configure_adapter();
        void sync_open_windows() const;

      private:
        tbx::ServiceProvider* _service_provider = nullptr;
        tbx::IWindowManager* _window_manager = nullptr;
        SdlOpenGlAdapter* _open_gl_adapter = nullptr;
        bool _use_opengl = false;
        bool _vsync_enabled = false;
    };
}
