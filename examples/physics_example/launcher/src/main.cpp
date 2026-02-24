#include "tbx/app/application.h"

int main()
{
    tbx::AppDescription desc = {
        .name = "PhysicsExample",
        .requested_plugins = {"PhysicsExampleRuntime"},
    };
    auto app = tbx::Application(desc);
    return app.run();
}
