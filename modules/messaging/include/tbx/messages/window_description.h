#pragma once
#include "tbx/tbx_api.h"
#include "tbx/tsl/size.h"
#include "tbx/tsl/uuid.h"
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

    // Describes a platform window in a way that can be serialized through messages.
    // Ownership: Holds value types only; callers retain ownership of referenced data.
    // Thread-safety: Mutable struct expected to be used from the main thread when configuring
    // windows; no internal synchronization is provided.
    struct TBX_API WindowDescription
    {
        Size size = {1280, 720};
        WindowMode mode = WindowMode::Windowed;
        std::string title = "Toybox";
        Uuid id = Uuid::generate();
    };
}
