#include "tbx/application.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx
{
    Application::Application(const AppDescription& desc)
    {
        _desc = desc;
        initialize();
    }

    Application::~Application()
    {
        shutdown();
    }

    int Application::run()
    {
        // Placeholder implementation; integrate your main loop here.
        return 0;
    }
}

