#include "runtime.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace two_d_example
{
    static void set_world_space_transform(
        const tbx::Entity& entity,
        const tbx::Transform& world_transform)
    {
        if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
            return;

        auto local_transform = world_transform;
        auto parent = tbx::Entity {};
        if (entity.try_get_parent_entity(parent))
        {
            const auto parent_world_transform = tbx::get_world_space_transform(parent);
            local_transform =
                tbx::world_to_local_tranform(parent_world_transform, world_transform);
        }

        entity.get_component<tbx::Transform>() = local_transform;
    }

    void TwoDExampleRuntimePlugin::on_attach(tbx::IPluginHost& host)
    {
        auto& ent_registry = host.get_entity_registry();
        const std::string greeting =
            "Welcome to the 2d example! This plugin just loads a few basic plugins and "
            "makes some entities.";
        const std::string message = tbx::trim(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        _elapsed_seconds = 0.0f;

        // Setup camera
        auto cam_ent = tbx::Entity("Camera", ent_registry);
        auto& cam = cam_ent.add_component<tbx::Camera>();
        cam.set_orthographic(20, 16.0f / 9.0f, 0.1f, 100.0f);
        cam_ent.add_component<tbx::Transform>(tbx::Vec3(0.0f, 0.0f, 10.0f));

        // Setup quads with unlit material
        constexpr auto toys_to_make = 5;
        constexpr auto spacing = 2.0f;
        constexpr auto starting_x = -((toys_to_make - 1.0f) * spacing) * 0.5f;
        for (int i = 0; i < toys_to_make; i++)
        {
            auto ent = tbx::Entity(std::to_string(i), ent_registry);
            ent.add_component<tbx::Transform>(tbx::Vec3(
                starting_x
                    + static_cast<float>(i)
                          * spacing, // shift on the x axis so they are not all in the same spot
                0,
                0));
            ent.add_component<tbx::Renderer>(tbx::FlatMaterialInstance(tbx::Color::WHITE));
            ent.add_component<tbx::DynamicMesh>(tbx::quad);
        }
    }

    void TwoDExampleRuntimePlugin::on_detach()
    {
        _elapsed_seconds = 0.0f;
    }

    void TwoDExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        _elapsed_seconds += dt.seconds;

        // bob all toys in stage with transform up, then down over time
        // also change color over time...
        float offset = 0.0f;
        for (auto& entity :
             get_host().get_entity_registry().get_with<tbx::Transform, tbx::Renderer>())
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            auto updated_world_transform = world_transform;
            updated_world_transform.position.y = sin(_elapsed_seconds * 2.0f + offset);
            set_world_space_transform(entity, updated_world_transform);

            auto& renderer = entity.get_component<tbx::Renderer>();
            const float phase = world_transform.position.x;
            const float t = _elapsed_seconds * 1.5f + phase;

            const float r = 0.5f + 0.5f * sin(t);
            const float g = 0.5f + 0.5f * sin(t + 2.0f * tbx::PI / 3.0f);
            const float b = 0.5f + 0.5f * sin(t + 4.0f * tbx::PI / 3.0f);

            auto color = tbx::Color(r, g, b, 1.0f);
            renderer.material.parameters.set("color", color);

            offset += 0.1f;
        }
    }
}
