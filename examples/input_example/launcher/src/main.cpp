#include "tbx/app/application.h"

int main()
{
    tbx::AppDescription desc = {
        .name = "InputExample",
        .requested_plugins = {"InputExampleRuntime"},
    };
    auto app = tbx::Application(desc);
    return app.run();
}
