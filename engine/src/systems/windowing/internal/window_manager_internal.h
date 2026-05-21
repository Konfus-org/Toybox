#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/windowing/window_manager.h"
#include <algorithm>
#include <string_view>
#include <utility>

namespace tbx::internal
{
    static bool are_sizes_equal(const Size& left, const Size& right)
    {
        return left.width == right.width && left.height == right.height;
    }

    static std::string sanitize_window_handle_name(std::string title)
    {
        if (title.empty())
            return "Toybox";

        return title;
    }
}
