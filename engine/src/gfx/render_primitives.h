#pragma once
#include "tbx/math/math.h"
#include <vector>

// Internal to the gfx renderer: CPU-side builders for the builtin primitive meshes. Each returns
// interleaved position(3) + normal(3) + uv(2) floats, ready for upload_mesh_to_gpu with the
// {3,3,2} attribute layout. No coupling to RenderState or the gpu boundary — pure geometry.
namespace tbx
{
    /// @brief
    /// Purpose: Unit cube centered at the origin (six axis-aligned faces).
    std::vector<float> build_cube_vertices();

    /// @brief
    /// Purpose: One NDC-covering triangle for fullscreen passes; uv spans [0,1] over the viewport.
    std::vector<float> build_fullscreen_vertices();

    /// @brief
    /// Purpose: 1x1 ground plane on the XZ axes, facing up.
    std::vector<float> build_plane_vertices();

    /// @brief
    /// Purpose: The blocky question-mark stand-in for a missing mesh (docs/RenderFailures.md).
    std::vector<float> build_question_mark_vertices();

    /// @brief
    /// Purpose: Unit sphere of the given ring/segment tessellation.
    std::vector<float> build_sphere_vertices(int rings, int segments);
}
