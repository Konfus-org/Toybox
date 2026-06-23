#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/size.h"

namespace tbx
{
    /// @brief
    /// Purpose: Carries backend-specific window handle data across systems.
    /// @details
    /// Ownership: Non-owning opaque pointer; platform backend controls lifetime.
    /// Thread Safety: Pointer value is copyable; lifetime access must be externally synchronized.
    using NativeWindowHandle = void*;

    /// @brief
    /// Purpose: Identifies a surface a frame can be rendered into — a native window or an
    /// in-memory texture — carrying everything a graphics backend needs to tell them apart.
    /// @details
    /// Ownership: Value type; callers own their copies. The backend owns realized GPU resources.
    /// Thread Safety: Safe to copy and compare across threads.
    struct TBX_API RenderTarget : public Handle
    {
      public:
        using Handle::Handle;

        RenderTarget() = default;
        explicit RenderTarget(Handle handle)
            : Handle(std::move(handle))
        {
        }

      public:
        Size size = {};
        NativeWindowHandle native_handle = nullptr;
    };
}
