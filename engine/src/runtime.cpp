#include "runtime_state.h"

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

    // Both ctors only stash inputs; boot() (first run()) owns all init — assets/reflection stand-up,
    // the window, command-line overrides — so hosts never init or create windows themselves.
    Runtime::Runtime(App app)
        : state(std::make_unique<internal::RuntimeState>())
    {
        state->app = std::move(app);
    }

    Runtime::Runtime(AssetHandle<App> app, CommandList commands)
        : state(std::make_unique<internal::RuntimeState>())
    {
        state->app_source = std::move(app);
        state->app.commands = std::move(commands);
    }
}
