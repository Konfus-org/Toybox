#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/messages/commands/app_commands.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/application_context.h"
#include "tbx/time/delta_time.h"
#include "tbx/logging/log_macros.h"
#include <iostream>

namespace tbx::examples
{
    class ExampleRuntimePlugin final : public Plugin
    {
    public:
        void on_attach(const ApplicationContext&, IMessageDispatcher& dispatcher) override
        {
            _dispatcher = &dispatcher;
            TBX_TRACE_INFO("Welcome to the plugin example! "
                           "This plugin just loads a logger and will parrot whatever you type, with two exceptions. "
                           "Those being: 'quit' or 'exit' to kill the app and 'assert' to throw an assertion.");
        }

        void on_detach() override
        {
        }

        void on_update(const DeltaTime& dt) override
        {
            std::string line;
            std::cout << "> ";

            if (!std::getline(std::cin, line)) return;
            if (line == "quit" || line == "exit")
            {
                _dispatcher->send(ExitApplicationCommand());
                return;
            }

            TBX_TRACE_INFO(to_string(dt) + " " + line);
            if (line == "assert")
            {
                TBX_ASSERT(false, "User triggered assert");
            }
        }

        void on_message(const Message& msg) override
        {
        }

    private:
        const IMessageDispatcher* _dispatcher = nullptr;
    };
}

TBX_REGISTER_PLUGIN(CreateExampleRuntime, tbx::examples::ExampleRuntimePlugin);
