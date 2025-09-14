#include "Tbx/Launcher/Launcher.h"
#include "Tbx/App/App.h"

namespace Tbx::Launcher
{
    AppStatus Launch(const std::string& name, const std::string& path)
    {
        auto status = AppStatus::None;
        auto running = true;
        while (running)
        {
            // Creating the app will launch it
            auto app = App(name);
            // After we've closed check if the app is asking for a reload
            // or if we should fully shutdown
            auto status = app.GetStatus();
            running =
                status != AppStatus::Error &&
                status != AppStatus::Closed;
        }
        return status;
    }
}