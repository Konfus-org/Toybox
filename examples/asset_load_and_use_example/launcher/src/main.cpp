#include "tbx/app/application.h"
#include <filesystem>

int main()
{
    const std::filesystem::path assets_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "assets";

    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc = {
        .name = "AssetLoadAndUseExample",
        .assets_directory = assets_root,
        .requested_plugins = {"AssetLoadAndUseExampleRuntime"},
    };
    auto app = tbx::Application(desc);

    // Run the application main loop
    return app.run();
}
