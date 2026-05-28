#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/render_target.h"
#include "tbx/types/size.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Carries backend-specific window handle data across systems.
    /// @details
    /// Ownership: Non-owning opaque pointer; platform backend controls lifetime.
    /// Thread Safety: Pointer value is copyable; lifetime access must be externally synchronized.
    using NativeWindowHandle = void*;

    /// @brief
    /// Purpose: Identifies a managed window within the active window manager service.
    /// @details
    /// Ownership: Value type copied by callers; the window manager owns the underlying state.
    /// Thread Safety: Safe to copy and compare across threads.
    using Window = RenderTarget;

    // Enumerates the presentation modes that a window can be configured for.
    // Ownership: Represents value semantics only; no ownership concerns.
    // Thread-safety: Immutable enum used freely across threads.
    enum class WindowMode
    {
        WINDOWED,
        BORDERLESS,
        FULLSCREEN,
        MINIMIZED
    };

    /// @brief
    /// Purpose: Describes the initial state used when a window manager creates a window entry.
    /// @details
    /// Ownership: Owns copied configuration values used during window creation.
    /// Thread Safety: Safe to copy; mutable use must be externally synchronized.
    struct TBX_API WindowCreateInfo
    {
        std::string title = "Toybox";
        Size size = {1280, 720};
        WindowMode mode = WindowMode::WINDOWED;
    };
}
