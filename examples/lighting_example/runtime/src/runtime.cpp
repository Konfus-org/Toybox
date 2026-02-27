#include "runtime.h"
#include "tbx/app/app_settings.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/input/input_action.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <array>
#include <cmath>
#include <string>

namespace lighting_example
{
    static constexpr float DIRECTIONAL_LIGHT_ENABLED_INTENSITY = 1.5F;
    static constexpr float DIRECTIONAL_LIGHT_ENABLED_AMBIENT = 0.15F;
    static constexpr float POINT_LIGHT_ENABLED_INTENSITY = 2.0F;
    static constexpr float SPOT_LIGHT_ENABLED_INTENSITY = 4.0F;
    static constexpr float AREA_LIGHT_ENABLED_INTENSITY = 5.0F;

    static tbx::InputAction create_key_toggle_action(
        const std::string& action_name,
        InputKey key,
        InputActionCallback on_start_callback)
    {
        return tbx::InputAction(
            action_name,
            InputActionValueType::BUTTON,
            InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control = KeyboardInputControl {.key = key},
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        on_start_callback,
                    },
            });
    }

    static tbx::Color evaluate_animated_color(double elapsed_seconds, float phase_offset)
    {
        const float t = static_cast<float>(elapsed_seconds);
        const float r = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset));
        const float g = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset + ((2.0F * PI) / 3.0F)));
        const float b = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset + ((4.0F * PI) / 3.0F)));
        return tbx::Color(r, g, b, 1.0F);
    }

    static float evaluate_bob_offset(
        double elapsed_seconds,
        float frequency,
        float phase_offset,
        float amplitude)
    {
        const float t = static_cast<float>(elapsed_seconds);
        return std::sin((t * frequency) + phase_offset) * amplitude;
    }

    static uint32 divide_round_up(uint32 numerator, uint32 denominator)
    {
        if (denominator == 0U)
            return 0U;
        return (numerator + denominator - 1U) / denominator;
    }

    static tbx::Vec3 evaluate_stress_light_position(
        uint32 light_index,
        uint32 light_count,
        double elapsed_seconds)
    {
        const uint32 columns =
            static_cast<uint32>(std::ceil(std::sqrt(static_cast<float>(light_count))));
        const uint32 rows = columns == 0U ? 0U : divide_round_up(light_count, columns);
        const uint32 row = columns == 0U ? 0U : (light_index / columns);
        const uint32 column = columns == 0U ? 0U : (light_index % columns);

        const float x = (static_cast<float>(column) - (static_cast<float>(columns) * 0.5F)) * 2.1F;
        const float z = (static_cast<float>(row) - (static_cast<float>(rows) * 0.5F)) * 2.1F;
        const float t = static_cast<float>(elapsed_seconds);
        const float phase = static_cast<float>(light_index) * 0.17F;
        const float y = 1.0F + (0.5F * std::sin((t * 1.4F) + phase));
        return tbx::Vec3(x, y, z);
    }

    void LightingExampleRuntimePlugin::clear_stress_lights()
    {
        for (auto& light_entity : _stress_point_lights)
            light_entity.destroy();
        _stress_point_lights.clear();
    }

    void LightingExampleRuntimePlugin::rebuild_stress_lights(uint32 light_count)
    {
        clear_stress_lights();
        auto& entity_registry = get_host().get_entity_registry();
        _stress_point_lights.reserve(light_count);
        for (uint32 light_index = 0U; light_index < light_count; ++light_index)
        {
            auto light_entity = tbx::Entity("StressPointLight", entity_registry);
            auto point_light = PointLight();
            point_light.range = 5.5F;
            point_light.intensity = 3.6F;
            point_light.color =
                evaluate_animated_color(0.0, static_cast<float>(light_index) * 0.21F);
            light_entity.add_component<PointLight>(point_light);
            light_entity.add_component<tbx::Transform>(
                evaluate_stress_light_position(light_index, light_count, 0.0));
            _stress_point_lights.push_back(light_entity);
        }
        TBX_TRACE_INFO("Stress lights rebuilt: {}", light_count);
    }

    void LightingExampleRuntimePlugin::on_attach(tbx::IPluginHost& host)
    {
        _elapsed_seconds = 0.0;
        auto& ent_registry = host.get_entity_registry();
        auto& input_manager = host.get_input_manager();

        _room.create(
            ent_registry,
            examples_common::RoomSettings {
                .center = tbx::Vec3(0.0F, -1.0F, 0.0F),
                .include_colliders = true,
            });

        auto showcase_sphere = tbx::Entity("ShowcaseSphere", ent_registry);
        showcase_sphere.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::LIGHT_GREY}},
        });
        showcase_sphere.add_component<DynamicMesh>(sphere);
        showcase_sphere.add_component<tbx::Transform>(
            tbx::Vec3(-2.0F, 1.5F, 0.0F),
            tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F),
            tbx::Vec3(2.0F, 2.0F, 2.0F));

        auto showcase_cube = tbx::Entity("ShowcaseCube", ent_registry);
        showcase_cube.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::GREY}},
        });
        showcase_cube.add_component<DynamicMesh>(cube);
        showcase_cube.add_component<tbx::Transform>(
            tbx::Vec3(2.0F, 0.25F, 0.0F),
            tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F),
            tbx::Vec3(2.0F, 0.5F, 2.0F));

        _directional_light = tbx::Entity("DirectionalLight", ent_registry);
        auto directional_light = DirectionalLight();
        directional_light.ambient = DIRECTIONAL_LIGHT_ENABLED_AMBIENT;
        directional_light.intensity = DIRECTIONAL_LIGHT_ENABLED_INTENSITY;
        _directional_light.add_component<DirectionalLight>(directional_light);
        _directional_light.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 10.0F, 0.0F),
            tbx::to_radians(tbx::Vec3(-45.0F, 30.0F, 0.0F)),
            tbx::Vec3(1.0F));

        _point_base_position = tbx::Vec3(-7.5F, 2.5F, -1.0F);
        _point_light = tbx::Entity("PointLight", ent_registry);
        auto point_light = PointLight();
        point_light.range = 14.0F;
        point_light.intensity = _point_enabled ? POINT_LIGHT_ENABLED_INTENSITY : 0.0F;
        _point_light.add_component<PointLight>(point_light);
        _point_light.add_component<tbx::Transform>(_point_base_position);

        _point_light_marker = tbx::Entity("PointLightMarker", ent_registry);
        _point_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::RED}, {"emissive", Color::RED}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _point_light_marker.add_component<DynamicMesh>(sphere);
        _point_light_marker.add_component<tbx::Transform>(
            _point_base_position,
            tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F),
            tbx::Vec3(0.3F));

        _spot_base_position = tbx::Vec3(0.0F, 3.8F, 8.5F);
        _spot_light = tbx::Entity("SpotLight", ent_registry);
        auto spot_light = SpotLight();
        spot_light.range = 30.0F;
        spot_light.inner_angle = 18.0F;
        spot_light.outer_angle = 32.0F;
        spot_light.intensity = _spot_enabled ? SPOT_LIGHT_ENABLED_INTENSITY : 0.0F;
        _spot_light.add_component<SpotLight>(spot_light);
        _spot_light.add_component<tbx::Transform>(
            _spot_base_position,
            tbx::to_radians(tbx::Vec3(-90.0F, 0.0F, 0.0F)),
            tbx::Vec3(1.0F));

        _spot_light_marker = tbx::Entity("SpotLightMarker", ent_registry);
        _spot_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::GREEN}, {"emissive", Color::GREEN}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _spot_light_marker.add_component<DynamicMesh>(cube);
        _spot_light_marker.add_component<tbx::Transform>(
            _spot_base_position,
            tbx::to_radians(tbx::Vec3(-90.0F, 0.0F, 0.0F)),
            tbx::Vec3(0.3F));

        _area_base_position = tbx::Vec3(7.5F, 2.5F, -1.0F);
        _area_light = tbx::Entity("AreaLight", ent_registry);
        auto area_light = AreaLight();
        area_light.range = 20.0F;
        area_light.area_size = tbx::Vec2(3.0F, 2.0F);
        area_light.intensity = _area_enabled ? AREA_LIGHT_ENABLED_INTENSITY : 0.0F;
        _area_light.add_component<AreaLight>(area_light);
        _area_light.add_component<tbx::Transform>(
            _area_base_position,
            tbx::to_radians(tbx::Vec3(-90.0F, 0.0F, 0.0F)),
            tbx::Vec3(1.0F));

        _area_light_marker = tbx::Entity("AreaLightMarker", ent_registry);
        _area_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::BLUE}, {"emissive", Color::BLUE}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _area_light_marker.add_component<DynamicMesh>(capsule);
        _area_light_marker.add_component<tbx::Transform>(
            _area_base_position,
            tbx::to_radians(tbx::Vec3(-90.0F, 0.0F, 0.0F)),
            tbx::Vec3(0.5F, 0.3F, 0.1F));

        auto camera = tbx::Entity("Camera", ent_registry);
        camera.add_component<Camera>();
        camera.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 3.5F, 14.0F),
            tbx::Quat(tbx::Vec3(tbx::to_radians(-90.0F), 0.0F, 0.0F)),
            tbx::Vec3(1.0F));

        _camera_controller.initialize(
            camera,
            input_manager,
            examples_common::FreeLookCameraControllerSettings {
                .initial_yaw = 0.0F,
                .initial_pitch = tbx::to_radians(-10.0F),
                .move_speed = 6.0F,
                .look_sensitivity = 0.0025F,
            });

        auto& light_scheme = _camera_controller.get_input_scheme();
        light_scheme.add_action(create_key_toggle_action(
            "toggle_directional",
            InputKey::ALPHA_0,
            [this](const tbx::InputAction&)
            {
                _directional_enabled = !_directional_enabled;
                TBX_TRACE_INFO("Toggle directional light: {}", _directional_enabled);
                auto& light = _directional_light.get_component<DirectionalLight>();
                light.intensity = _directional_enabled ? DIRECTIONAL_LIGHT_ENABLED_INTENSITY : 0.0F;
                light.ambient = _directional_enabled ? DIRECTIONAL_LIGHT_ENABLED_AMBIENT : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_point",
            InputKey::ALPHA_1,
            [this](const tbx::InputAction&)
            {
                _point_enabled = !_point_enabled;
                TBX_TRACE_INFO("Toggle point light: {}", _point_enabled);
                auto& light = _point_light.get_component<PointLight>();
                light.intensity = _point_enabled ? POINT_LIGHT_ENABLED_INTENSITY : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_spot",
            InputKey::ALPHA_2,
            [this](const tbx::InputAction&)
            {
                _spot_enabled = !_spot_enabled;
                TBX_TRACE_INFO("Toggle spot light: {}", _spot_enabled);
                auto& light = _spot_light.get_component<SpotLight>();
                light.intensity = _spot_enabled ? SPOT_LIGHT_ENABLED_INTENSITY : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_area",
            InputKey::ALPHA_3,
            [this](const tbx::InputAction&)
            {
                _area_enabled = !_area_enabled;
                TBX_TRACE_INFO("Toggle area light: {}", _area_enabled);
                auto& light = _area_light.get_component<AreaLight>();
                light.intensity = _area_enabled ? AREA_LIGHT_ENABLED_INTENSITY : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_stress_lights",
            InputKey::F5,
            [this](const tbx::InputAction&)
            {
                _stress_mode_enabled = !_stress_mode_enabled;
                if (_stress_mode_enabled)
                    rebuild_stress_lights(_stress_light_count);
                else
                    clear_stress_lights();

                TBX_TRACE_INFO(
                    "Toggle stress mode: {} (lights={})",
                    _stress_mode_enabled,
                    _stress_light_count);
            }));
        light_scheme.add_action(create_key_toggle_action(
            "cycle_stress_light_count",
            InputKey::F6,
            [this](const tbx::InputAction&)
            {
                static constexpr auto STRESS_PRESETS = std::array {128U, 256U, 512U, 768U};
                auto selected_index = 0U;
                for (size_t index = 0U; index < STRESS_PRESETS.size(); ++index)
                {
                    if (STRESS_PRESETS[index] != _stress_light_count)
                        continue;

                    selected_index = index;
                    break;
                }

                selected_index = (selected_index + 1U) % STRESS_PRESETS.size();
                _stress_light_count = STRESS_PRESETS[selected_index];
                if (_stress_mode_enabled)
                    rebuild_stress_lights(_stress_light_count);

                TBX_TRACE_INFO("Stress light preset: {}", _stress_light_count);
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_shadow_budget",
            InputKey::F7,
            [this](const tbx::InputAction&)
            {
                auto& graphics = get_host().get_settings().graphics;
                if (graphics.shadow_render_distance.value <= 25.0F)
                {
                    graphics.shadow_map_resolution = 2048U;
                    graphics.shadow_render_distance = 60.0F;
                    graphics.shadow_softness = 1.0F;
                }
                else
                {
                    graphics.shadow_map_resolution = 1024U;
                    graphics.shadow_render_distance = 20.0F;
                    graphics.shadow_softness = 0.6F;
                }

                TBX_TRACE_INFO(
                    "Shadow budget preset -> res={}, distance={}, softness={}",
                    graphics.shadow_map_resolution.value,
                    graphics.shadow_render_distance.value,
                    graphics.shadow_softness.value);
            }));
    }

    void LightingExampleRuntimePlugin::on_detach()
    {
        auto& input_manager = get_host().get_input_manager();
        _camera_controller.shutdown(input_manager);
        clear_stress_lights();
        _room.destroy();
    }

    void LightingExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        _camera_controller.update(dt);
        _elapsed_seconds += dt.seconds;

        {
            auto& directional_transform = _directional_light.get_component<tbx::Transform>();
            constexpr float yaw_rate = 0.02F;
            const float angle = yaw_rate * static_cast<float>(dt.seconds);
            const auto rotation_delta = tbx::Quat(tbx::Vec3(0.0F, angle, 0.0F));
            directional_transform.rotation =
                normalize(rotation_delta * directional_transform.rotation);
        }

        {
            auto& point_light = _point_light.get_component<PointLight>();
            if (_point_enabled)
            {
                point_light.color = evaluate_animated_color(_elapsed_seconds, 0.75F);
                point_light.intensity =
                    POINT_LIGHT_ENABLED_INTENSITY
                    + (1.2F * std::sin(static_cast<float>(_elapsed_seconds) * 1.1F));

                auto& point_transform = _point_light.get_component<tbx::Transform>();
                point_transform.position = _point_base_position;
                point_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 2.4F, 0.1F, 0.9F);

                {
                    auto& marker_transform = _point_light_marker.get_component<tbx::Transform>();
                    marker_transform.position = point_transform.position;
                    auto& marker_renderer = _point_light_marker.get_component<Renderer>();
                    marker_renderer.material.parameters.set("color", point_light.color);
                    marker_renderer.material.parameters.set("emissive", point_light.color);
                }
            }
            else
            {
                point_light.intensity = 0.0F;
                auto& marker_renderer = _point_light_marker.get_component<Renderer>();
                marker_renderer.material.parameters.set("color", Color::BLACK);
                marker_renderer.material.parameters.set("emissive", Color::BLACK);
            }
        }

        {
            auto& spot_light = _spot_light.get_component<SpotLight>();
            if (_spot_enabled)
            {
                spot_light.color = evaluate_animated_color(_elapsed_seconds, 2.1F);
                spot_light.intensity =
                    SPOT_LIGHT_ENABLED_INTENSITY
                    + (1.5F * std::sin(static_cast<float>(_elapsed_seconds) * 1.35F));
                auto& spot_transform = _spot_light.get_component<tbx::Transform>();
                spot_transform.position = _spot_base_position;
                spot_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 2.1F, 1.2F, 0.8F);

                {
                    auto& marker_transform = _spot_light_marker.get_component<tbx::Transform>();
                    marker_transform.position = spot_transform.position;
                    marker_transform.rotation = spot_transform.rotation;
                    auto& marker_renderer = _spot_light_marker.get_component<Renderer>();
                    marker_renderer.material.parameters.set("color", spot_light.color);
                    marker_renderer.material.parameters.set("emissive", spot_light.color);
                }
            }
            else
            {
                spot_light.intensity = 0.0F;
                auto& marker_renderer = _spot_light_marker.get_component<Renderer>();
                marker_renderer.material.parameters.set("color", Color::BLACK);
                marker_renderer.material.parameters.set("emissive", Color::BLACK);
            }
        }

        {
            auto& area_light = _area_light.get_component<AreaLight>();
            if (_area_enabled)
            {
                area_light.color = evaluate_animated_color(_elapsed_seconds, 3.6F);
                area_light.intensity =
                    AREA_LIGHT_ENABLED_INTENSITY
                    + (1.0F * std::sin(static_cast<float>(_elapsed_seconds) * 0.95F));

                auto& area_transform = _area_light.get_component<tbx::Transform>();
                area_transform.position = _area_base_position;
                area_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 1.8F, 2.7F, 0.85F);

                {
                    auto& marker_transform = _area_light_marker.get_component<tbx::Transform>();
                    marker_transform.position = area_transform.position;
                    marker_transform.rotation = area_transform.rotation;
                    auto& marker_renderer = _area_light_marker.get_component<Renderer>();
                    marker_renderer.material.parameters.set("color", area_light.color);
                    marker_renderer.material.parameters.set("emissive", area_light.color);
                }
            }
            else
            {
                area_light.intensity = 0.0F;
                auto& marker_renderer = _area_light_marker.get_component<Renderer>();
                marker_renderer.material.parameters.set("color", Color::BLACK);
                marker_renderer.material.parameters.set("emissive", Color::BLACK);
            }
        }

        if (_stress_mode_enabled)
        {
            for (uint32 light_index = 0U; light_index < _stress_point_lights.size(); ++light_index)
            {
                auto& light_entity = _stress_point_lights[light_index];
                if (!light_entity.has_component<PointLight>()
                    || !light_entity.has_component<tbx::Transform>())
                    continue;

                auto& point_light = light_entity.get_component<PointLight>();
                auto& transform = light_entity.get_component<tbx::Transform>();
                transform.position = evaluate_stress_light_position(
                    light_index,
                    static_cast<uint32>(_stress_point_lights.size()),
                    _elapsed_seconds);
                point_light.color = evaluate_animated_color(
                    _elapsed_seconds,
                    static_cast<float>(light_index) * 0.13F);
                point_light.intensity = 2.6F
                                        + (1.8F
                                           * std::sin(
                                               (static_cast<float>(_elapsed_seconds) * 1.25F)
                                               + (static_cast<float>(light_index) * 0.19F)));
            }
        }
    }
}
