#pragma once
#include "tbx/systems/graphics/draw_command_factory.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include <string>
#include <vector>

namespace tbx::internal
{
    static std::string make_draw_instance_key(const RenderingDrawBatchInput& input)
    {
        if (!input.debug_name.empty())
            return input.debug_name;

        return std::string("Toybox/Draw/") + std::to_string(input.batch_key);
    }
}
