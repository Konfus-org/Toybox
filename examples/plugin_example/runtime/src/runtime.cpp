#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include <string>

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(Application& context)
    {
        std::string greeting =
            "Welcome to the plugin example! This plugin just loads a logger and a window.";
        std::string message = trim(greeting);
        TBX_TRACE_INFO("{}", message.c_str());
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt) {}

    void ExampleRuntimePlugin::on_recieve_message(Message&) {}
}
