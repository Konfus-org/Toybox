#pragma once
#include "tbx/graphics/render_surface.h"
#include "tbx/messages/message.h"

namespace tbx
{
    // Request to present rendered content to a specified render surface.
    struct PresentSurfaceRequest : public Request<void>
    {
        PresentSurfaceRequest(const RenderSurface& target_surface)
            : surface(target_surface)
        {
        }

        RenderSurface surface = {};
    };
}
