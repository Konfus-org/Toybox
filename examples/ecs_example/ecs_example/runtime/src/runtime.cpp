#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/common/string.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entities.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace tbx::examples
{
    EntityDirector & _director;

    void ExampleRuntimePlugin::on_attach(Application& context)
    {
        _director = context.get_director();

        std::string greeting =
            "Welcome to the ecs example! This plugin just loads a few basic plugins and makes some "
            "entities.";
        std::string message = greeting.trim();
        TBX_TRACE_INFO("{}", message);

        auto toys_to_make = 5;
        for (int i = 0; i < toys_to_make; i++)
        {
            std::string ent_name = i;
            auto e = director.create_entity(ent_name);
            e.add_component<Transform>();
        }
        auto no_it = _ex_stage.add_toy("should_not_iterate");
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        // bob all toys in stage with transform up, then down over time
        for (auto& toy : _ex_stage.get_entities<Transform>())
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
