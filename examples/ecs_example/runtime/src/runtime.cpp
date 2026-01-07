#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/common/string_utils.h"
#include <string>
#include "tbx/debugging/macros.h"
#include "tbx/ecs/toys.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace tbx::examples
{
    Stage _ex_stage = Stage("Example Stage");

    void ExampleRuntimePlugin::on_attach(Application& context)
    {
        std::string greeting =
            "Welcome to the ecs example! This plugin just loads a few basic plugins and makes some "
            "entities.";
        std::string message = TrimString(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        auto toys_to_make = 5;
        for (int i = 0; i < toys_to_make; i++)
        {
            auto t = _ex_stage.add_toy(std::to_string(i));
            t.add_block<Transform>();
        }
        auto no_it = _ex_stage.add_toy("should_not_iterate");
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        // bob all toys in stage with transform up, then down over time
        for (auto& toy : _ex_stage.view_with_type<Transform>())
        {
            auto& transform = toy.get_block<Transform>();
            transform.position.y = sin(dt.seconds * 2.0) * 0.5f;

            TBX_TRACE_INFO("Toy {}:", toy.get_description().name);
            TBX_TRACE_INFO(
                "Position: ({}, {}, {})",
                transform.position.x,
                transform.position.y,
                transform.position.z);
        }
    }

    void ExampleRuntimePlugin::on_recieve_message(Message&) {}
}
