#include "tbx/systems/app/application.h"

// TODO: create a launcher that does this for us, it knows how to configure the app based off a
// launcher settings asset. This new launcher should also take in command line args and pass them
// along to the app
int main()
{
    tbx::AppDescription desc = {
        .name = "3DExample",
        .requested_plugins = {"ThreeDExampleRuntime"},
        .startup_world = tbx::Handle("Worlds/Example.world"),
    };

    TBX_TRY_CATCH_ASSERT(
        {
            auto app = tbx::Application(desc);
            return app.run();
        },
        "Application error occured!");
}
