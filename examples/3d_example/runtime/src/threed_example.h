#pragma once
#include "tbx/interfaces/plugin.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Provides plugin dependencies and registered C++ script asset types.
    [[tbx::plugin(
        name = "ThreeDExampleRuntime",
        version = "1.0.0",
        category = tbx::PluginCategory::GAMEPLAY)]];
    class ThreeDExampleRuntimePlugin final : public tbx::Plugin
    {
    };
}
