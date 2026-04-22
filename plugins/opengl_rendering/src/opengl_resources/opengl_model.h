#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/math/matrices.h"
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores OpenGL-resolved data for a model part.
    /// @details
    /// Ownership: Owns transform and child index data; references GPU resources by OpenGL ids.
    /// Thread Safety: Not thread-safe; use on the render thread.
    struct OpenGlModelPart
    {
        tbx::Mat4 transform = tbx::Mat4(1.0f);
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
        std::vector<uint32> children = {};
    };

    /// @brief
    /// Purpose: Represents a GPU-ready model cache for OpenGL rendering.
    /// @details
    /// Ownership: Owns part hierarchy data and mesh references by OpenGL ids.
    /// Thread Safety: Not thread-safe; use on the render thread.
    struct OpenGlModel
    {
        uint32 model_id = 0U;
        std::vector<uint32> meshes = {};
        std::vector<OpenGlModelPart> parts = {};
    };
}
