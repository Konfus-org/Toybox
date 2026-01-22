#pragma once
#include "tbx/plugin_api/plugin.h"

namespace tbx
{
    struct WindowContextReadyEvent;
}

namespace tbx::plugins
{
    /// <summary>SDL/OpenGL bridge plugin.</summary>
    /// <remarks>Purpose: Configures SDL OpenGL defaults and loads GL entry points for SDL contexts.
    /// Ownership: Does not own SDL or OpenGL resources; configures global state only.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.</remarks>
    class SdlOpenGlAdapterPlugin final : public Plugin
    {
      public:
        /// <summary>Initializes SDL/OpenGL bridge state.</summary>
        /// <remarks>Purpose: Applies SDL OpenGL attribute defaults needed before context creation.
        /// Ownership: Does not take ownership of the host application.
        /// Thread Safety: Called on the main thread during plugin attach.</remarks>
        void on_attach(Application& host) override;

        /// <summary>Releases adapter tracking state.</summary>
        /// <remarks>Purpose: Resets local tracking for GL loader configuration.
        /// Ownership: Does not release SDL or OpenGL resources.
        /// Thread Safety: Called on the main thread during plugin detach.</remarks>
        void on_detach() override;

        /// <summary>Handles window context readiness notifications.</summary>
        /// <remarks>Purpose: Loads GL entry points using SDL's proc address when available.
        /// Ownership: Does not take ownership of the message or loader pointer.
        /// Thread Safety: Invoked on the dispatcher thread (typically main).</remarks>
        void on_recieve_message(Message& msg) override;

      private:
        void configure_opengl_attributes() const;
        bool try_load_glad_with_sdl() const;
        void handle_window_ready(WindowContextReadyEvent& event);

        bool _glad_loaded = false;
        bool _needs_glad_load = true;
    };
}
