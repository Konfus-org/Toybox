#pragma once
#include "tbx/core/color.h"
#include "tbx/core/math.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: A viewpoint: perspective settings; position/orientation come from Transform
    /// (looks along its -Z). The first active camera renders.
    struct Camera
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
    };

    /// @brief
    /// Purpose: Makes a toy visible: a named mesh ("cube" or "plane" today; asset meshes come
    /// with model loading) with a tint.
    struct MeshRenderer
    {
        std::string mesh = "cube";
        Color tint = {};
    };

    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct DirectionalLight
    {
        Color color = {};
        float intensity = 1.0f;
    };

    namespace gpu
    {
        /// @brief
        /// Purpose: Registers Camera/MeshRenderer/DirectionalLight as blocks; run() calls this
        /// at boot (tests call it directly) so kits can carry them.
        void register_render_blocks();
    }
}
