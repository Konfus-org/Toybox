#include "tbx/app/application.h"

int main()
{
    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc = {
        .name = "EcsExample",
        .logs_directory = "./logs",
        .plugins_directory = "./",
        .requested_plugins = {"EcsRuntimeRuntime"},
    };
    auto app = tbx::Application(desc);

    // Run the application main loop
    return app.run();
}
