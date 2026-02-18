#include "runtime.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entities.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <array>
#include <memory>

namespace tbx::examples
{
    void ExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _ent_registry = &context.get_entity_registry();
        std::string greeting =
            "Welcome to the ecs example! This plugin just loads a few basic plugins and "
            "makes some entities.";
        std::string message = trim(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        _elapsed_seconds = 0.0f;

        // Setup camera
        auto cam_ent = Entity("Camera", *_ent_registry);
        cam_ent.add_component<Camera>();
        cam_ent.add_component<Transform>(Vec3(0.0f, 0.0f, 10.0f));

        // Setup quads with unlit material
        auto toys_to_make = 5;
        auto spacing = 2.0f;
        auto starting_x = -((toys_to_make - 1.0f) * spacing) * 0.5f;
        for (int i = 0; i < toys_to_make; i++)
        {
            auto ent = Entity(std::to_string(i), *_ent_registry);
            ent.add_component<Transform>(Vec3(
                starting_x
                    + (static_cast<float>(i)
                       * spacing), // shift on the x axis so they are not all in the same spot
                0,
                0));
            ent.add_component<Renderer>(unlit_material);
            ent.add_component<DynamicMesh>(quad);
        }
    }

    void ExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        _elapsed_seconds += dt.seconds;

        // bob all toys in stage with transform up, then down over time
        // also change color over time...
        float offset = 0.0f;
        for (auto& entity : _ent_registry->get_with<Transform, Renderer>())
        {
            auto& transform = entity.get_component<Transform>();
            transform.position.y = sin((_elapsed_seconds * 2.0f) + offset);

            auto& renderer = entity.get_component<Renderer>();
            float phase = transform.position.x;
            float t = (_elapsed_seconds * 1.5f) + phase;

            float r = 0.5f + (0.5f * sin(t));
            float g = 0.5f + (0.5f * sin(t + (2.0f * PI / 3.0f)));
            float b = 0.5f + (0.5f * sin(t + (4.0f * PI / 3.0f)));

            RgbaColor color = RgbaColor(r, g, b, 1.0f);
            renderer.material.parameters.set("color", color);

            offset += 0.1f;
        }
    }
}
