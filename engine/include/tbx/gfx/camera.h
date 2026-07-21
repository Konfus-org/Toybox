#pragma once
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: A viewpoint: perspective settings; position/orientation come from Transform
    /// (looks along its -Z). The first camera renders.
    struct TBX_API Camera
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
    };
}
