#include "tbx/examples/runtime_plugin.h"
#include "tbx/debug/log_macros.h"
#include "tbx/messages/commands/app_commands.h"
#include <iostream>

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(const ApplicationContext&)
    {
        TBX_TRACE_INFO(
            "Welcome to the plugin example! "
            "This plugin just loads a logger and will parrot whatever you type, with two "
            "exceptions. "
            "Those being: 'quit' or 'exit' to kill the app and 'assert' to throw an "
            "assertion.");
    }

    void ExampleRuntimePlugin::on_detach()
    {
    }

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        std::string line;
        std::cout << "> ";

        // if (!std::getline(std::cin, line)) return;
        if (line == "quit" || line == "exit")
        {
            send_message(ExitApplicationCommand());
            return;
        }

        TBX_TRACE_INFO(to_string(dt) + " " + line);
        if (line == "assert")
        {
            TBX_ASSERT(false, "User triggered assert");
        }
    }

    void ExampleRuntimePlugin::on_message(const Message&)
    {
    }
}
