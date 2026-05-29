#include "tbx/systems/app/application.h"

int main()
{
    tbx::AppDescription desc = {
        .name = "3DExample",
        .requested_plugins = {"ThreeDExampleRuntime"},
        .startup_world = tbx::Handle("Worlds/Example.world"),
    };
    auto app = tbx::Application(desc);
    return app.run();
}
