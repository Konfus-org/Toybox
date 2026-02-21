#pragma once
#include "opengl_render_pipeline.h"
#include <vector>

namespace tbx::plugins
{
    /// <summary>Resolved post-processing stack for the active frame.</summary>
    /// <remarks>Purpose: Stores ordered post-process effects for renderer execution.
    /// Ownership: Owns copied effect configuration values.
    /// Thread Safety: Safe to copy; mutate on render thread.</remarks>
    struct OpenGlPostProcessing final
    {
        std::vector<OpenGlPostProcessEffect> effects = {};
    };
}
