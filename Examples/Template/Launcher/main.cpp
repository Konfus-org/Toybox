#include "main.h"
#include <Tbx/Launcher/Launcher.h>

int main()
{
    auto config = Tbx::Launcher::AppConfig();
    config.Name = "YOUR APP NAME HERE";
    config.Settings.RenderingApi = Tbx::GraphicsApi::OpenGL;

    auto status = Tbx::Launcher::Launch(config);
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
