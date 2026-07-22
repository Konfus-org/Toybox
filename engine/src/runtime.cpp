#include "tbx/runtime.h"
#include "tbx/cmdline_handler.h"

namespace tbx
{
    Runtime::Runtime()
        : state(std::make_unique<RuntimeState>())
    {
    }

    Runtime::Runtime(App app)
        : state(std::make_unique<RuntimeState>())
    {
        // Parse-time command handling (-w/-h size overrides) before the window exists.
        cmdline::apply(app);
        if (!app.config.is_headless)
        {
            auto window = windows::Window();
            window.title = app.config.title;
            window.width = app.config.width;
            window.height = app.config.height;
            state->windows.windows.push_back(std::move(window));
        }
        state->app = std::move(app);
    }
}
