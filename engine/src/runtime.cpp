#include "runtime_state.h"
#include "tbx/utils/cmdline_handler.h"

namespace tbx
{
    // Out-of-line here (where internal::RuntimeState is complete) so the pImpl unique_ptr can
    // destroy/move it — the public runtime.h only forward-declares the state.
    Runtime::~Runtime() = default;
    Runtime::Runtime(Runtime&&) noexcept = default;
    Runtime& Runtime::operator=(Runtime&&) noexcept = default;

    Runtime::Runtime()
        : state(std::make_unique<internal::RuntimeState>())
    {
    }

    Runtime::Runtime(App app)
        : state(std::make_unique<internal::RuntimeState>())
    {
        // Parse-time command handling (-w/-h size overrides) before the window exists.
        apply_cmdline(app);
        if (!app.config.is_headless)
        {
            auto window = Window();
            window.title = app.config.title;
            window.width = static_cast<uint32>(app.config.width);
            window.height = static_cast<uint32>(app.config.height);
            state->windows.open_windows.push_back(std::move(window));
        }
        state->app = std::move(app);
    }
}
