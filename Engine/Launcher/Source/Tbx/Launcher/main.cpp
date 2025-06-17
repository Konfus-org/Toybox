#include "Tbx/Launcher/main.h"
#include "Tbx/Launcher/AppHost.h"

#ifndef PATH_TO_PLUGINS
    #define PATH_TO_PLUGINS "./Plugins"
#endif

int main()
{
    auto status = Tbx::RunHost(PATH_TO_PLUGINS);
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
