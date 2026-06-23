#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/tbx_api.h"
#include "tbx/types/render_target.h"
#include "tbx/types/size.h"
#include "tbx/types/window.generated.h"
#include <filesystem>
#include <string>
#include <utility>

namespace tbx
{
    // Enumerates the presentation modes that a window can be configured for.
    // Ownership: Represents value semantics only; no ownership concerns.
    // Thread-safety: Immutable enum used freely across threads.
    enum class WindowMode
    {
        WINDOWED,
        BORDERLESS,
        FULLSCREEN,
        MINIMIZED,
        HIDDEN
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
        GraphicsApi api = GraphicsApi::OPEN_GL;
        std::filesystem::path icon_path = {};
    };

    /// @brief
    /// Purpose: Identifies a managed window within the active window manager service.
    /// @details
    /// Ownership: Value type copied by callers; the window manager owns the underlying state.
    /// Thread Safety: Safe to copy and compare across threads.
    [[printable("[Name: {}, Id: {}]", name, id)]];
    [[hash(name, id)]];
    struct TBX_API Window : public RenderTarget
    {
      public:
        using RenderTarget::RenderTarget;

      public:
        Window() = default;
        explicit Window(RenderTarget render_target)
            : RenderTarget(std::move(render_target))
        {
        }
    };
}
