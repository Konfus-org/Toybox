#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include <string>
#include <vector>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Stores texture bindings for OpenGL material rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the binding name; references GPU textures by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlMaterialTexture
    {
        std::string name = "";
        Uuid texture_id = {};
    };

    /// <summary>
    /// Purpose: Represents a GPU-ready material cache for OpenGL rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns parameter and binding data; references GPU resources by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlMaterial
    {
        std::vector<Uuid> shader_programs = {};
        std::vector<MaterialParameter> parameters = {};
        std::vector<OpenGlMaterialTexture> textures = {};
    };
}
