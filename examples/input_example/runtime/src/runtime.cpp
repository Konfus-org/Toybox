#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <cmath>

namespace tbx::examples
{
    void InputExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        _ent_registry = &host.get_entity_registry();
        _input_manager = &host.get_input_manager();
        auto& graphics = host.get_settings().graphics;
        graphics.shadow_map_resolution = 2048U;
        graphics.shadow_render_distance = 40.0F;
        graphics.shadow_softness = 1.1F;

        _sun = Entity("Light", _ent_registry);
        _sun.add_component<DirectionalLight>(RgbaColor::white, 1.0F, 0.25F);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0F, 45.0F, 0.0F)), Vec3(1));

        auto ground_ent = Entity("Ground", _ent_registry);
        ground_ent.add_component<Renderer>();
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<Transform>(
            Vec3(0.0F, 0.0F, 0.0F),
            to_radians(Vec3(-90.0F, 0.0F, 0.0F)),
            Vec3(20.0F, 20.0F, 1.0F));

        auto cube_ent = Entity("Cube", _ent_registry);
        cube_ent.add_component<Renderer>();
        cube_ent.add_component<DynamicMesh>(cube);
        cube_ent.add_component<Transform>(Vec3(0.0F, 0.5F, -5.0F));

        // Create player entity
        {
            _player = Entity("Player", _ent_registry);
            _player.add_component<Transform>(Vec3(0.0F, 0.01F, 0.0F));

            auto player_visual = Entity("PlayerVisual", _player.get_id(), _ent_registry);
            player_visual.add_component<Renderer>();
            player_visual.add_component<DynamicMesh>(capsule);
            player_visual.add_component<Transform>(
                Vec3(0.0F, 1.0F, 0.0F),
                Quat(1.0F, 0.0F, 0.0F, 0.0F),
                Vec3(1.5F, 2.0F, 1.5F));

            _camera = Entity("Camera", _player.get_id(), _ent_registry);
            _camera.add_component<Camera>();
            _camera.add_component<Transform>(Vec3(0.0F, 2.0F, -0.55F));
        }

        // Setup input scheme
        auto camera_scheme = InputScheme(
            _camera_scheme_name,
            {
                InputAction(
                    "Move",
                    InputActionValueType::VECTOR2,
                    InputActionConstruction {
                        .bindings =
                            {
                                InputBinding {
                                    .control =
                                        KeyboardVector2CompositeInputControl {
                                            .up = InputKey::W,
                                            .down = InputKey::S,
                                            .left = InputKey::A,
                                            .right = InputKey::D,
                                        },
                                    .scale = 1.0F,
                                },
                            },
                        .on_performed_callbacks =
                            {
                                [this](const InputAction& action)
                                {
                                    Vec2 move_axis = Vec2(0.0F, 0.0F);
                                    if (action.try_get_value_as<Vec2>(move_axis))
                                        _move_axis = move_axis;
                                },
                            },
                        .on_cancelled_callbacks =
                            {
                                [this](const InputAction&)
                                {
                                    _move_axis = Vec2(0.0F, 0.0F);
                                },
                            },
                    }),
                InputAction(
                    "Look",
                    InputActionValueType::VECTOR2,
                    InputActionConstruction {
                        .bindings =
                            {
                                InputBinding {
                                    .control =
                                        MouseVectorInputControl {
                                            .control = InputMouseVectorControl::DELTA,
                                        },
                                    .scale = 1.0F,
                                },
                            },
                        .on_performed_callbacks =
                            {
                                [this](const InputAction& action)
                                {
                                    Vec2 look_delta = Vec2(0.0F, 0.0F);
                                    if (action.try_get_value_as<Vec2>(look_delta))
                                        _look_delta = look_delta;
                                },
                            },
                        .on_cancelled_callbacks =
                            {
                                [this](const InputAction&)
                                {
                                    _look_delta = Vec2(0.0F, 0.0F);
                                },
                            },
                    }),
            });

        _input_manager->add_scheme(camera_scheme);
        _input_manager->activate_scheme(_camera_scheme_name);
        _input_manager->set_mouse_lock_mode(MouseLockMode::RELATIVE);
    }

    void InputExampleRuntimePlugin::on_detach()
    {
        if (_input_manager)
            _input_manager->remove_scheme(_camera_scheme_name);

        _input_manager = nullptr;
        _ent_registry = nullptr;
    }

    void InputExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        auto& light_transform = _sun.get_component<Transform>();
        const float yaw_rate = 0.5F;
        float angle = yaw_rate * static_cast<float>(dt.seconds);
        auto light_delta = Quat({0.0F, angle, 0.0F});
        light_transform.rotation = normalize(light_delta * light_transform.rotation);

        auto& camera_transform = _camera.get_component<Transform>();
        auto& player_transform = _player.get_component<Transform>();

        _camera_yaw -= _look_delta.x * _camera_look_sensitivity;
        _camera_pitch -= _look_delta.y * _camera_look_sensitivity;

        const float max_pitch = to_radians(89.0F);
        if (_camera_pitch > max_pitch)
            _camera_pitch = max_pitch;
        if (_camera_pitch < -max_pitch)
            _camera_pitch = -max_pitch;

        const Quat yaw_rotation = normalize(Quat(Vec3(0.0F, _camera_yaw, 0.0F)));
        const Quat pitch_rotation = normalize(Quat(Vec3(_camera_pitch, 0.0F, 0.0F)));

        const float right_axis = _move_axis.x;
        const float forward_axis = _move_axis.y;

        Vec3 forward = yaw_rotation * Vec3(0.0F, 0.0F, -1.0F);
        Vec3 right = yaw_rotation * Vec3(1.0F, 0.0F, 0.0F);

        forward.y = 0.0F;
        right.y = 0.0F;

        const float forward_length_squared =
            forward.x * forward.x + forward.y * forward.y + forward.z * forward.z;
        if (forward_length_squared > 0.0F)
            forward *= 1.0F / std::sqrt(forward_length_squared);

        const float right_length_squared =
            right.x * right.x + right.y * right.y + right.z * right.z;
        if (right_length_squared > 0.0F)
            right *= 1.0F / std::sqrt(right_length_squared);

        Vec3 move = forward * forward_axis + right * right_axis;

        const float move_length_squared = move.x * move.x + move.y * move.y + move.z * move.z;
        if (move_length_squared <= 0.0F)
            move = Vec3(0.0F, 0.0F, 0.0F);
        else
        {
            const float inverse_length = 1.0F / std::sqrt(move_length_squared);
            move *= inverse_length;
            player_transform.position += move * _player_move_speed * static_cast<float>(dt.seconds);
        }

        camera_transform.rotation = pitch_rotation;
        player_transform.rotation = yaw_rotation;
    }
}
