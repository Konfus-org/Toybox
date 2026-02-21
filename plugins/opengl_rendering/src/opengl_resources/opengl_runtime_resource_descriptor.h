#pragma once
#include "tbx/math/size.h"

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Enumerates runtime OpenGL resource kinds that can be created from entities.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy and read concurrently.
    /// </remarks>
    enum class OpenGlRuntimeResourceKind
    {
        GBUFFER,
        FRAMEBUFFER,
        SHADOW_MAP,
    };

    /// <summary>
    /// Purpose: Describes one runtime OpenGL resource to be created for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type stored on ECS entities.
    /// Thread Safety: Safe to read concurrently; synchronize writes externally.
    /// </remarks>
    struct OpenGlRuntimeResourceDescriptor final
    {
        OpenGlRuntimeResourceKind kind = OpenGlRuntimeResourceKind::FRAMEBUFFER;
        Size shadow_map_resolution = {};
    };
}
