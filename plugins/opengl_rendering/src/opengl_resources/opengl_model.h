#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/common/uuid.h"
#include "tbx/math/matrices.h"
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores OpenGL-resolved data for a model part.
    /// @details
    /// Ownership: Owns transform and child index data; references GPU resources by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    struct OpenGlModelPart
    {
        tbx::Mat4 transform = tbx::Mat4(1.0f);
        tbx::Uuid mesh_id = {};
        tbx::Uuid material_id = {};
        std::vector<uint32> children = {};
    };

    /// @brief
    /// Purpose: Represents a GPU-ready model cache for OpenGL rendering.
    /// @details
    /// Ownership: Owns part hierarchy data and mesh references by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    struct OpenGlModel
    {
        tbx::Uuid model_id = {};
        std::vector<tbx::Uuid> meshes = {};
        std::vector<OpenGlModelPart> parts = {};
    };
}
