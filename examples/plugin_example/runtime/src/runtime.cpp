#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/common/string.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/toys.h"

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(Application& context)
    {
        String greeting =
            "Welcome to the plugin example! This plugin just loads a logger and a window.";
        String message = greeting.trim();
        TBX_TRACE_INFO("{}", message.c_str());
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt) {}

    void ExampleRuntimePlugin::on_recieve_message(Message&) {}
}
