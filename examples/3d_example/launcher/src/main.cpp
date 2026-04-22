#include "tbx/core/systems/app/application.h"

int main()
{
    tbx::AppDescription desc = {
        .name = "3DExample",
        .requested_plugins = {"ThreeDExampleRuntime"},
    };
    auto app = tbx::Application(desc);
    return app.run();
}
