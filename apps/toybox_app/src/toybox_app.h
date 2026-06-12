#pragma once
#include "tbx/systems/app/application.h"

namespace tbx_apps
{
    /// @brief
    /// Purpose: Generic data-driven app host: runs whatever world, assets, and plugins the
    /// supplied settings file describes, with no project-specific code.
    [[tbx::app(name = "Toybox", version = "1.0.0")]];
    class ToyboxApp final : public tbx::Application
    {
    };
}
