#include "tbx/application.h"
#include "tbx/logging/log_macros.h"
#include <iostream>

int main()
{
    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc;
    desc.name = "PluginExample";
    desc.requested_plugins = {"Logging.SpdLogger"};

    tbx::Application app(desc);

    std::string line;
    while (true)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, line))
            break;
        if (line == "quit" || line == "exit")
            break;

        TBX_TRACE_INFO(line);
        if (line == "assert")
        {
            TBX_ASSERT(false, "User triggered assert");
        }
    }
    return 0;
}
