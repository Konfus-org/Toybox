#include "tbx/core/systems/app/application.h"

int main()
{
    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc = {
        .name = "2DExample",
        .requested_plugins = {"TwoDExampleRuntime"},
    };
    auto app = tbx::Application(desc);

    // Run the application main loop
    return app.run();
}
