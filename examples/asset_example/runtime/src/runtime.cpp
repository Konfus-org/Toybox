#include "runtime.h"
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
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        _ent_registry = &host.get_entity_registry();
        _input_manager = &host.get_input_manager();

        // Setup assets to use
        auto smily_mat = Handle("Materials/Smily.mat");
        auto skybox_mat = Handle("Materials/AnimeSkybox.mat");
        auto lut_post_mat = Handle("Materials/LutPostProcess.mat");
        auto skybox_tex = Handle("Textures/AnimeSkybox.png");
        auto lut_tex = Handle("Textures/LUTs/LUT_Cinematic.png");
        auto green_cube = Handle("Models/Green_Cube.fbx");

        // Setup light
        _sun = Entity("Light", *_ent_registry);
        _sun.add_component<DirectionalLight>(RgbaColor::yellow, 1.0f, 0.25f);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0f, 45.0f, 0.0f)), Vec3(1));

        // Setup cube
        _green_cube = Entity("Cube", *_ent_registry);
        _green_cube.add_component<Renderer>();
        _green_cube.add_component<StaticMesh>(green_cube);
        _green_cube.add_component<Transform>(Vec3(0.0f, 0.0f, -5.0f));

        // Setup ground
        auto ground_ent = Entity("Ground", *_ent_registry);
        ground_ent.add_component<Renderer>(smily_mat);
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<Transform>(
            Vec3(0.0f, -2.0f, -5.0f),
            to_radians(Vec3(-90.0f, 0.0f, 0.0f)),
            Vec3(20.0f, 20.0f, 1.0f));

        // Setup sky
        auto sky_ent = Entity("Sky", *_ent_registry);
        sky_ent.add_component<Sky>(skybox_mat);

        // Setup global post-processing stack
        auto post_ent = Entity("PostProcessing", *_ent_registry);
        auto post_processing = PostProcessing({
            PostProcessingEffect {
                .material =
                    {
                        .handle = lut_post_mat,
                        .textures = {{
                            .name = "lut",
                            .texture = {lut_tex},
                        }},
                    },
                .blend = 1.0f,
            },
        });
        post_ent.add_component<PostProcessing>(post_processing);

        // Setup camera
        _camera = Entity("Camera", *_ent_registry);
        _camera.add_component<Camera>();
        _camera.add_component<Transform>(Vec3(0.0f, 0.0f, 10.0f));

        // Setup input scheme: WASD move + mouse delta look and locked mouse
        auto camera_scheme = InputScheme(_camera_scheme_name);

        auto move_action = InputAction("Move", InputActionValueType::VECTOR2);
        move_action.add_binding(
            InputBinding {
                .control =
                    KeyboardVector2CompositeInputControl {
                        .up = InputKey::W,
                        .down = InputKey::S,
                        .left = InputKey::A,
                        .right = InputKey::D,
                    },
                .scale = 1.0F,
            });

        auto look_action = InputAction("Look", InputActionValueType::VECTOR2);
        look_action.add_binding(
            InputBinding {
                .control = MouseVectorInputControl {.control = InputMouseVectorControl::DELTA},
                .scale = 1.0F,
            });

        move_action.add_on_performed_callback(
            [this](const InputAction& action)
            {
                Vec2 move_axis = Vec2(0.0F, 0.0F);
                if (action.try_get_value_as<Vec2>(move_axis))
                    _move_axis = move_axis;
            });
        move_action.add_on_cancelled_callback(
            [this](const InputAction&)
            {
                _move_axis = Vec2(0.0F, 0.0F);
            });

        look_action.add_on_performed_callback(
            [this](const InputAction& action)
            {
                Vec2 look_delta = Vec2(0.0F, 0.0F);
                if (action.try_get_value_as<Vec2>(look_delta))
                    _look_delta = look_delta;
            });
        look_action.add_on_cancelled_callback(
            [this](const InputAction&)
            {
                _look_delta = Vec2(0.0F, 0.0F);
            });

        camera_scheme.add_action(move_action);
        camera_scheme.add_action(look_action);

        _input_manager->add_scheme(camera_scheme);
        _input_manager->activate_scheme(_camera_scheme_name);
        _input_manager->set_mouse_lock_mode(MouseLockMode::RELATIVE);
    }

    void AssetExampleRuntimePlugin::on_detach()
    {
        if (_input_manager)
            _input_manager->remove_scheme(_camera_scheme_name);

        _input_manager = nullptr;
        _ent_registry = nullptr;
    }

    void AssetExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        // Rotate cube...
        {
            auto& transform = _green_cube.get_component<Transform>();
            const float yaw_rate = 2.0F;
            float angle = yaw_rate * static_cast<float>(dt.seconds);
            auto delta = Quat({0.0f, angle, 0.0f});
            transform.rotation = normalize(delta * transform.rotation);
        }

        // Rotate light...
        {
            auto& transform = _sun.get_component<Transform>();
            const float yaw_rate = 0.5F;
            float angle = yaw_rate * static_cast<float>(dt.seconds);
            auto delta = Quat({0.0f, angle, 0.0f});
            transform.rotation = normalize(delta * transform.rotation);
        }

        auto& camera_transform = _camera.get_component<Transform>();

        // Mouse-look.
        _camera_yaw += _look_delta.x * _camera_look_sensitivity;
        _camera_pitch -= _look_delta.y * _camera_look_sensitivity;

        const float max_pitch = to_radians(89.0F);
        if (_camera_pitch > max_pitch)
            _camera_pitch = max_pitch;
        if (_camera_pitch < -max_pitch)
            _camera_pitch = -max_pitch;

        camera_transform.rotation = normalize(Quat(Vec3(_camera_pitch, _camera_yaw, 0.0F)));

        // WASD translation in camera yaw-space.
        const float right_axis = _move_axis.x;
        const float forward_axis = _move_axis.y;

        Vec3 forward = Vec3(std::sin(_camera_yaw), 0.0F, -std::cos(_camera_yaw));
        Vec3 right = Vec3(std::cos(_camera_yaw), 0.0F, std::sin(_camera_yaw));
        Vec3 move = forward * forward_axis + right * right_axis;

        const float move_length_squared = move.x * move.x + move.y * move.y + move.z * move.z;
        if (move_length_squared > 0.0F)
        {
            const float inverse_length = 1.0F / std::sqrt(move_length_squared);
            move *= inverse_length;
            camera_transform.position += move * _camera_move_speed * static_cast<float>(dt.seconds);
        }
    }
}
