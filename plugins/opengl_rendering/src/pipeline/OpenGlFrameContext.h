#pragma once
#include "tbx/ecs/entity.h"
#include <vector>

namespace opengl_rendering
{
    class OpenGlFrameContext
    {
        std::vector<tbx::Entity> entities_to_render;
    };
}
