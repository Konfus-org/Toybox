#include "tbx/systems/app/application.h"

int main()
{
    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc = {
        .name = "2DExample",
        .requested_plugins = {"TwoDExampleRuntime"},
    };

    TBX_TRY_CATCH_ASSERT(
        {
            auto app = tbx::Application(desc);

            // Run the application main loop
            return app.run();
        },
        "Application error occured!");
}
