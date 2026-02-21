#pragma once
#include "tbx/graphics/messages.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugins/sdl_opengl_adapter/sdl_open_gl_adapter.h"
#include <SDL3/SDL.h>
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>Hosts SDL/OpenGL adapter code as a standalone plugin module.</summary>
    /// <remarks>Purpose: Packages SDL OpenGL bridging utilities into a dedicated plugin for reuse.
    /// Ownership: Owns no persistent resources by default.
    /// Thread Safety: Expected to be attached/detached on the main thread.</remarks>
    class SdlOpenGlAdapterPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_recieve_message(Message& msg) override;

      private:
        void ensure_open_gl_adapter();
        void on_window_native_handle_changed(
            PropertyChangedEvent<Window, NativeWindowHandle>& event);
        void handle_make_current(WindowMakeCurrentRequest& request);
        void handle_present(WindowPresentRequest& request);
        void apply_vsync_setting();

      private:
        std::unordered_map<Uuid, SDL_Window*> _native_windows;
        std::unordered_map<Uuid, Size> _window_sizes;
        bool _use_opengl = false;
        bool _vsync_enabled = false;
        std::unique_ptr<SdlOpenGlAdapter> _open_gl_adapter;
    };
}
