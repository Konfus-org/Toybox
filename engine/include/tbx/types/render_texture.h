#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/render_target.h"

namespace tbx
{
    /// @brief
    /// Purpose: Identifies an in-memory texture used as a frame output target instead of a
    /// window, letting hosts (such as editors) consume rendered frames without any presentation.
    /// @details
    /// Ownership: Value type; the graphics backend owns the realized GPU resources.
    /// Thread Safety: Safe to copy; backend access happens on the render lane.
    struct TBX_API RenderTexture : public RenderTarget
    {
      public:
        using RenderTarget::RenderTarget;
        RenderTexture() = default;
    };
}
