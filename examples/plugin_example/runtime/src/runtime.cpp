#include "runtime_plugin.h"
#include "tbx/debug/macros.h"
#include "tbx/messages/commands/app_commands.h"
#include <iostream>

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(const ApplicationContext&)
    {
        TBX_TRACE_INFO(
            "Welcome to the plugin example! "
            "This plugin just loads a logger and a window");
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt) {}

    void ExampleRuntimePlugin::on_message(const Message&) {}
}
