#include "runtime_plugin.h"
#include "tbx/debugging/macros.h"
#include "tbx/std/string.h"

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(Application&)
    {
        tbx::String greeting = tbx::String(
            "   Welcome to the plugin example! "
            "This plugin just loads a logger and a window.   ");
        tbx::String message = tbx::get_trimmed(greeting);

        TBX_TRACE_INFO("%s", message.get_raw());
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt) {}

    void ExampleRuntimePlugin::on_message(Message&) {}
}
