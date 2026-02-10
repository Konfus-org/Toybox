#include "runtime.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entities.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_registry();
        std::string greeting =
            "Welcome to the ecs example! This plugin just loads a few basic plugins and "
            "makes some entities.";
        std::string message = trim(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        _elapsed_seconds = 0.0f;

        auto toys_to_make = 5;
        auto spacing = 2.0f;
        auto starting_x = -((toys_to_make - 1.0f) * spacing) * 0.5f;
        for (int i = 0; i < toys_to_make; i++)
        {
            // create and add components
            auto ent = _entity_manager->create(std::to_string(i));
            ent.add_component<Transform>();
            ent.add_component<Renderer>(quad, unlit_material);

            // shift on the x axis so they are not all in the same spot
            auto& transform = ent.get_component<Transform>();
            transform.position.x = starting_x + (static_cast<float>(i) * spacing);
        }
    }

    void ExampleRuntimePlugin::on_detach() {}

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        _elapsed_seconds += dt.seconds;

        // bob all toys in stage with transform up, then down over time
        float offset = 0;
        for (auto& entity : _entity_manager->get_with<Transform>())
        {
            auto& transform = entity.get_component<Transform>();
            transform.position.y = sin((_elapsed_seconds * 2.0f) + offset);
            offset += 0.1f;
        }
    }

    void ExampleRuntimePlugin::on_recieve_message(Message&) {}
}
