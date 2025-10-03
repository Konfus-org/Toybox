#include "main.h"
#include <Tbx/Launcher/Launcher.h>

int main()
{
	// TODO: Allow client to define plugins to like so...
    /*auto appPlugins =
    {
        "3dDemoRuntime",
        "ImGuiDebugUI",
        "JIMSAssetLoader",
        "OpenGLRendering",
        "SDL3Input",
        "SDL3Windowing",
        "SpdLogging"
    };*/
    auto config = Tbx::Launcher::AppConfig();
    config.Name = "3d Demo";

    auto status = Tbx::Launcher::Launch(config);
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
