#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/texture.h"
#include "tbx/graphics/window.h"
#include "tbx/math/size.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Represents a rendering target surface, such as a window or framebuffer.
    // Ownership: value type; callers own any copies created from this struct.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    struct TBX_API RenderSurface
    {
      public:
        RenderSurface() = default;

        // Create new render surface from a texture
        RenderSurface(
            const IMessageDispatcher& dispatcher,
            const Size& surface_size,
            const Uuid& target)
            : _dispatcher(&dispatcher)
            , size(surface_size)
            , id(target)
        {
        }

        // Presents the rendered content to this surface by sending a message via the dispatcher.
        void present() const;

        // Size of render surface
        Size size = {};

        // Identifier for the rendering surface (e.g., window ID or texture ID).
        Uuid id = invalid::uuid;

      private:
        const IMessageDispatcher* _dispatcher = nullptr;
    };
}
