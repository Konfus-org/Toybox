#pragma once
#include "tbx/common/uuid.h"
#include "tbx/math/size.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/observable.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    // Enumerates the presentation modes that a window can be configured for.
    // Ownership: Represents value semantics only; no ownership concerns.
    // Thread-safety: Immutable enum used freely across threads.
    enum class TBX_API WindowMode
    {
        Windowed,
        Borderless,
        Fullscreen,
        Minimized
    };

    // Window is a lightweight wrapper around a platform specific implementation managed through
    // messages.
    // Ownership: Does not own the underlying platform window; keeps no platform state locally.
    // Thread-safety: Thread-safe though message dispatcher.
    struct TBX_API Window
    {
        Window(
            IMessageDispatcher& dispatcher,
            std::string title = "Toybox",
            Size size = {1280, 720},
            WindowMode mode = WindowMode::Windowed,
            bool open_on_creation = true);
        ~Window();

        Observable<Window, std::string> title;
        Observable<Window, Size> size;
        Observable<Window, WindowMode> mode;
        Observable<Window, bool> is_open;

        Uuid id = Uuid::generate();
    };
}
