#include "runtime_plugin.h"
#include "tbx/common/string_extensions.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/stage.h"
#include <string>

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(Application& context)
    {
        std::string greeting =
            "Welcome to the plugin example! This plugin just loads a logger and a window.";
        std::string message = trim_string(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        // Example ECS usage
        auto stage = Stage();
        stage.name = "Example Stage";
        TBX_TRACE_INFO(
            "Created stage '{}' with ID {}",
            stage.name.c_str(),
            to_string(stage.id).c_str());

        auto toy = add_toy(stage, "test");
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt) {}

    void ExampleRuntimePlugin::on_message(Message&) {}
}
