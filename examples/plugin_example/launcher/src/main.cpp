#include "tbx/application.h"

int main()
{
    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc =
    {
        .name = "PluginExample",
        .plugins_directory = "./",
        .requested_plugins = {"Runtime.Example"}
    };
    auto app = tbx::Application(desc);

    // Run the application main loop
    return app.run();
}
