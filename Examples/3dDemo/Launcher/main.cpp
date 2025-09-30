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
    auto status = Tbx::Launcher::Launch("3d Demo");
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
