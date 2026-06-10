#include "flashlight_controller.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include <algorithm>
#include <glm/ext/quaternion_common.hpp>

namespace tbx_example
{
    void FlashlightController::on_destroy()
    {
        if (auto input_manager = input.lock())
        {
            if (auto scheme = input_manager->get_scheme(_scheme_name); scheme.has_value())
                scheme->get().remove_action("ToggleFlashlight");
        }

        _camera_entity = {};
        _toggle_requested = false;
        _is_enabled = false;
    }

    void FlashlightController::on_start()
    {
        auto world = get_world_ptr().lock();
        auto input_manager = input.lock();
        if (!world || !input_manager)
        {
            TBX_TRACE_WARNING(
                "FlashlightController could not start because the world or input service is "
                "missing.");
            return;
        }

        _camera_entity = world->get(camera_entity);
        if (!_camera_entity.get_id().is_valid())
        {
            TBX_TRACE_WARNING("FlashlightController could not find the authored camera entity.");
            return;
        }

        auto& flashlight = get_entity();
        if (!flashlight.has_component<tbx::Transform>())
            flashlight.add_component<tbx::Transform>();

        if (!flashlight.has_component<tbx::SpotLight>())
        {
            // No authored light on this entity: pick room-scale defaults (a 10u range would leave
            // the floor unlit). When the scene authors a SpotLight we respect its range/cone.
            auto& spot = flashlight.add_component<tbx::SpotLight>();
            spot.range = 40.0F;
            spot.inner_angle = 22.0F;
            spot.outer_angle = 38.0F;
        }

        // Start off; the toggle drives intensity from here.
        flashlight.get_component<tbx::SpotLight>().intensity = 0.0F;

        setup_input();
        sync_to_camera_now();
    }

    void FlashlightController::on_update(const tbx::DeltaTime& dt)
    {
        update_actions();
        update_transform(dt);
    }

    tbx::InputAction FlashlightController::create_toggle_action()
    {
        return tbx::InputAction(
            "ToggleFlashlight",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control = tbx::KeyboardInputControl {.key = tbx::InputKey::F},
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _toggle_requested = true;
                        },
                    },
            });
    }

    void FlashlightController::setup_input()
    {
        auto input_manager = input.lock();
        if (!input_manager)
            return;

        if (!input_manager->get_scheme(_scheme_name).has_value())
            input_manager->add_scheme(tbx::InputScheme(_scheme_name));

        auto scheme = input_manager->get_scheme(_scheme_name);
        if (!scheme.has_value())
            return;

        scheme->get().remove_action("ToggleFlashlight");
        scheme->get().add_action(create_toggle_action());
        input_manager->activate_scheme(_scheme_name);
    }

    void FlashlightController::sync_to_camera_now()
    {
        auto& flashlight = get_entity();
        if (!_camera_entity.get_id().is_valid() || !flashlight.get_id().is_valid()
            || !_camera_entity.has_component<tbx::Transform>())
            return;
        if (!flashlight.has_component<tbx::Transform>())
            return;

        flashlight.get_component<tbx::Transform>() =
            _camera_entity.get_component<tbx::Transform>().to_world_space(_camera_entity);
    }

    void FlashlightController::update_actions()
    {
        if (!_toggle_requested)
            return;

        _toggle_requested = false;
        _is_enabled = !_is_enabled;

        auto& flashlight = get_entity();
        if (!flashlight.get_id().is_valid() || !flashlight.has_component<tbx::SpotLight>())
            return;

        flashlight.get_component<tbx::SpotLight>().intensity = _is_enabled ? intensity : 0.0F;
    }

    void FlashlightController::update_transform(const tbx::DeltaTime& dt)
    {
        auto& flashlight = get_entity();
        if (!_camera_entity.get_id().is_valid() || !flashlight.get_id().is_valid()
            || !_camera_entity.has_component<tbx::Transform>())
            return;
        if (!flashlight.has_component<tbx::Transform>())
            return;

        const auto camera_transform =
            _camera_entity.get_component<tbx::Transform>().to_world_space(_camera_entity);
        auto& flashlight_transform = flashlight.get_component<tbx::Transform>();
        const auto blend = std::clamp(follow_speed * static_cast<float>(dt.seconds), 0.0F, 1.0F);

        flashlight_transform.position +=
            (camera_transform.position - flashlight_transform.position) * blend;
        flashlight_transform.rotation = tbx::normalize(
            glm::slerp(flashlight_transform.rotation, camera_transform.rotation, blend));
        flashlight_transform.scale = camera_transform.scale;
    }
}
