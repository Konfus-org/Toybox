#pragma once
#include "tbx/ecs/builtin_blocks.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: Registers Camera/MeshRenderer/DirectionalLight as blocks; run() calls this
    /// at boot (tests call it directly) so kits can carry them.
    void register_render_blocks();
}
