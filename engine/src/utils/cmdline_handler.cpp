#include "runtime_state.h"
#include "tbx/utils/cmdline_handler.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/texture.h"
#include "tbx/serialization/read_write.h"
#include "tbx/platform/window.h"
#include <algorithm>
#include <filesystem>
#include <string>

namespace tbx
{
    void apply_cmdline(App& app)
    {
        // -w/-h override the configured window size per launch (screenshot tooling, quick
        // resolution checks).
        app.config.width = app.commands.get<int>("w", app.config.width);
        app.config.height = app.commands.get<int>("h", app.config.height);
    }

    void internal::update_cmdline(RuntimeState& state)
    {
        // --screenshot[=path] -number N -delay F: pre-present captures of real rendered
        // frames (at run() entry the backbuffer still holds the frame the previous call
        // drew). Every F frames one lands, N in total, then the app quits.
        App& app = state.app;
        if (!app.commands.has("screenshot") || state.windows.open_windows.empty())
            return;
        const auto delay = static_cast<uint64>(std::max(app.commands.get<int>("delay", 8), 1));
        const auto number = static_cast<uint64>(std::max(app.commands.get<int>("number", 1), 1));
        const uint64 shot = state.frame.index / delay;
        Window& window = state.windows.open_windows.front();
        if (state.frame.index % delay != 0 || shot < 1 || shot > number
            || window.status != WindowStatus::OPEN || !window.backend)
            return;

        auto path =
            std::filesystem::path(app.commands.get<std::string>("screenshot", "screenshot.bmp"));
        if (path == "true") // a bare --screenshot flag carries no path
            path = "screenshot.bmp";
        if (number > 1)
            path.replace_filename(
                path.stem().string() + "_" + std::to_string(shot) + path.extension().string());

        internal::make_current(window);
        set_render_viewport(window.width, window.height);
        auto capture = Texture();
        if (const auto read = render_screenshot(capture); !read)
            TBX_ERROR("screenshot: {}", read.error());
        else if (const auto saved = serialize(capture, path); !saved)
            TBX_ERROR("screenshot '{}': {}", path.string(), saved.error());
        else
            TBX_INFO("saved screenshot '{}'", path.string());
        if (shot == number)
            app.status = AppStatus::QUIT_REQUESTED;
    }
}
