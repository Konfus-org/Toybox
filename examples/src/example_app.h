#pragma once
#include "tbx/systems/app/application.h"

namespace tbx_example
{
    /// @brief
    /// Purpose: Provides the example app runtime entrypoint.
    [[tbx::app(name = "Examples", version = "1.0.0")]];
    class ExampleApp final : public tbx::Application
    {
    };
}
