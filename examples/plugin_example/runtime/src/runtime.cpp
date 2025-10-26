#include "tbx/plugin_api/plugin_loader.h"
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
        void on_attach(const ApplicationContext&, IMessageDispatcher&) override
        {
        }

        void on_detach() override
        {
        }

        void on_update(const DeltaTime&) override
        {
            std::string line;
            std::cout << "> ";

            if (!std::getline(std::cin, line)) return;
            if (line == "quit" || line == "exit") return;

            TBX_TRACE_INFO(line);
            if (line == "assert")
            {
                TBX_ASSERT(false, "User triggered assert");
            }
        }

        void on_message(const Message& msg) override
        {
        }
    };
}

TBX_REGISTER_PLUGIN(CreateExampleRuntime, tbx::examples::ExampleRuntimePlugin);
