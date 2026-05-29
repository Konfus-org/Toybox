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
    // TODO: Make it to where if we don't specify dependencies defaults are used
    [[tbx::dependency("SdlBaseSystemsPlugin")]];
    [[tbx::dependency("SdlWindowingPlugin")]];
    [[tbx::dependency("SdlOpenGlContextManagerPlugin")]];
    [[tbx::dependency("OpenGlRenderingPlugin")]];
    [[tbx::dependency("SdlInputPlugin")]];
    [[tbx::dependency("JoltPhysicsPlugin")]];
    [[tbx::dependency("AssimpModelLoaderPlugin")]];
    [[tbx::dependency("StbImageLoaderPlugin")]];
    [[tbx::dependency("ShaderIncludeLoader")]];
    [[tbx::dependency("PerformanceMonitor")]];
    class ThreeDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
    };
}
