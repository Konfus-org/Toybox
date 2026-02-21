#pragma once
#include "tbx/graphics/material.h"

namespace tbx::plugins
{
    /// <summary>Resolved sky data for the active frame.</summary>
    /// <remarks>Purpose: Stores clear color and optional sky material inputs.
    /// Ownership: Value type containing copied material instance data.
    /// Thread Safety: Safe to copy; mutate on render thread.</remarks>
    struct OpenGlSky final
    {
        MaterialInstance sky_material = {};
    };
}
