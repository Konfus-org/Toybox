#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/application.h"
#include "tbx/logging/log_macros.h"
#include "tbx/messages/dispatcher_context.h"
#include <chrono>
#include <iostream>

int main()
{
    using namespace std::chrono_literals;

    // Use the Application to load and run the plugin from the plugins directory
    tbx::AppDescription desc;
    desc.name = "PluginExample";
    desc.requested_plugins = {"Logging.SpdLogger"};

    tbx::Application app(desc);
    tbx::DispatcherScope scope(&app.get_dispatcher());

    spdlog::info("Type a message and press Enter to log. Type 'quit' to exit.");
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
    app.request_exit();
    return 0;
}
