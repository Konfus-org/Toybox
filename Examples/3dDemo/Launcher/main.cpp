#include "main.h"
#include <Tbx/Launcher/Launcher.h>

int main()
{
    auto config = Tbx::Launcher::AppConfig();
    config.Name = "3d Demo";
    config.Settings.Vsync = Tbx::VsyncMode::On;
    config.Settings.Resolution = {720, 480};
    config.Settings.RenderingApi = Tbx::GraphicsApi::OpenGL;
    config.Settings.ClearColor = Tbx::RgbaColor::Red;

    auto status = Tbx::Launcher::Launch(config);
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
