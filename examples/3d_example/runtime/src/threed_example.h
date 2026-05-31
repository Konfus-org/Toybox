#pragma once
#include "tbx/interfaces/plugin.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Provides plugin dependencies and registered C++ script asset types.
    [[tbx::plugin]];
    [[tbx::name("ThreeDExampleRuntime")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("gameplay")]];
    class ThreeDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
    };
}
