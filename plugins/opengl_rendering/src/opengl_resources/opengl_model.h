#pragma once
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/math/matrices.h"
#include <vector>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Stores OpenGL-resolved data for a model part.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns transform and child index data; references GPU resources by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlModelPart
    {
        tbx::Mat4 transform = tbx::Mat4(1.0f);
        Uuid mesh_id = {};
        Uuid material_id = {};
        std::vector<uint32> children = {};
    };

    /// <summary>
    /// Purpose: Represents a GPU-ready model cache for OpenGL rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns part hierarchy data and mesh references by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlModel
    {
        Uuid model_id = {};
        std::vector<Uuid> meshes = {};
        std::vector<OpenGlModelPart> parts = {};
    };
}
