#include "tbx/app/application.h"

int main()
{
    tbx::AppDescription desc = {
        .name = "LightingExample",
        .requested_plugins = {"LightingExampleRuntime"},
    };
    auto app = tbx::Application(desc);
    return app.run();
}
