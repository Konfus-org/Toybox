#pragma once
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/utils/api.h"
#include <string>

namespace tbx::gfx
{
    /// @brief
    /// Purpose: A viewpoint: perspective settings plus where it draws — a window (by name;
    /// empty = the main window) and a normalized viewport rect within it (x, y, width,
    /// height with a bottom-left origin, glViewport-style). Position/orientation come from
    /// Transform (looks along its -Z). Every enabled camera renders.
    struct TBX_API Camera : ecs::Block
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
        std::string window = {};
        Vec4 viewport = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    };
}
