#include "runtime.h"
#include "material_descriptions.generated.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/color.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/trig.h"
#include "tbx/utils/string_utils.h"
#include <memory>

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
            local_transform = tbx::world_to_local_tranform(parent_world_transform, world_transform);
        }

        entity.get_component<tbx::Transform>() = local_transform;
    }

    void TwoDExampleRuntimePlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        // TODO: fix, currently the rendering expects the world to be an asset....
        _world = std::unique_ptr<tbx::World>();

        const std::string greeting =
            "Welcome to the 2d example! This plugin just loads a few basic plugins and "
            "makes some entities.";
        const std::string message = tbx::trim(greeting);
        TBX_TRACE_INFO("{}", message.c_str());

        _elapsed_seconds = 0.0f;

        // Setup camera
        auto cam_ent = _world->create_entity("Camera");
        auto& cam = cam_ent.add_component<tbx::Camera>();
        cam.set_orthographic(20, 16.0f / 9.0f, 0.1f, 100.0f);
        cam_ent.add_component<tbx::Transform>(tbx::Vec3(0.0f, 0.0f, 10.0f));

        // Setup quads with unlit material
        constexpr auto toys_to_make = 5;
        constexpr auto spacing = 2.0f;
        constexpr auto starting_x = -((toys_to_make - 1.0f) * spacing) * 0.5f;
        for (int i = 0; i < toys_to_make; i++)
        {
            auto ent = _world->create_entity(std::to_string(i));
            ent.add_component<tbx::Transform>(tbx::Vec3(
                starting_x
                    + static_cast<float>(i)
                          * spacing, // shift on the x axis so they are not all in the same spot
                0,
                0));
            auto material = tbx::MaterialInstance(tbx::FlatMaterial::HANDLE);
            material.set_parameter(tbx::FlatMaterial::ALBEDO_COLOR, tbx::Color::WHITE);
            ent.add_component<tbx::MaterialInstance>(material);
            ent.add_component<tbx::DynamicMesh>(tbx::Mesh::QUAD);
        }
    }

    void TwoDExampleRuntimePlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        _world = {};
        _elapsed_seconds = 0.0f;
    }

    void TwoDExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        _elapsed_seconds += dt.seconds;

        // bob all toys in stage with transform up, then down over time
        // also change color over time...
        float offset = 0.0f;
        for (auto& entity : _world->get_with<tbx::Transform, tbx::MaterialInstance>())
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            auto updated_world_transform = world_transform;
            updated_world_transform.position.y = sin(_elapsed_seconds * 2.0f + offset);
            set_world_space_transform(entity, updated_world_transform);

            auto& material = entity.get_component<tbx::MaterialInstance>();
            const float phase = world_transform.position.x;
            const float t = _elapsed_seconds * 1.5f + phase;

            const float r = 0.5f + 0.5f * sin(t);
            const float g = 0.5f + 0.5f * sin(t + 2.0f * tbx::PI / 3.0f);
            const float b = 0.5f + 0.5f * sin(t + 4.0f * tbx::PI / 3.0f);

            auto color = tbx::Color(r, g, b, 1.0f);
            material.set_parameter(tbx::FlatMaterial::ALBEDO_COLOR, color);

            offset += 0.1f;
        }
    }
}
