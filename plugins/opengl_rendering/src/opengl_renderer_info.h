#pragma once
#include <string>

namespace tbx::plugins
{
    /// <summary>Describes discovered OpenGL implementation capabilities.</summary>
    /// <remarks>Purpose: Exposes renderer/vendor/version information for diagnostics.
    /// Ownership: Value type storing copied OpenGL strings.
    /// Thread Safety: Safe to copy; mutate on render thread during initialization.</remarks>
    struct OpenGlRendererInfo final
    {
        std::string vendor = {};
        std::string renderer = {};
        std::string version = {};
        std::string shading_language_version = {};
        int major_version = 0;
        int minor_version = 0;
    };
}
