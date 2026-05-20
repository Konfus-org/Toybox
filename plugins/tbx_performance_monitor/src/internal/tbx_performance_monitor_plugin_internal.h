#pragma once
#include "tbx/plugins/tbx_performance_monitor/tbx_performance_monitor_plugin.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/api.h"
#include <algorithm>
#include <cmath>

namespace tbx::performance_monitor::internal
{
#if defined(TBX_DEBUG)
    static std::string build_debug_window_title(
        const std::string& base_title,
        tbx::GraphicsApi graphics_api,
        uint average_fps)
    {
        auto title = base_title;
        title += " [";
        title += tbx::to_string(graphics_api);
        title += ", FPS: ";
        title += std::to_string(average_fps);
        title += "]";
        return title;
    }
#endif
}
