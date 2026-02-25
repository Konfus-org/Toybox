#include "runtime.h"
#include "tbx/app/app_settings.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/input/input_action.h"
#include "tbx/input/input_scheme.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <cmath>

namespace tbx::examples
{
    static constexpr float directional_light_enabled_intensity = 1.5F;
    static constexpr float directional_light_enabled_ambient = 0.15F;
    static constexpr float point_light_enabled_intensity = 6.0F;
    static constexpr float spot_light_enabled_intensity = 40.0F;
    static constexpr float area_light_enabled_intensity = 55.0F;

    static InputAction create_key_toggle_action(
        const std::string& action_name,
        const InputKey key,
        InputActionCallback on_start_callback)
    {
        return InputAction(
            action_name,
            InputActionValueType::BUTTON,
            InputActionConstruction {
                .bindings =
                    {
                        InputBinding {
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

    static Color evaluate_animated_color(const double elapsed_seconds, const float phase_offset)
    {
        const float t = static_cast<float>(elapsed_seconds);
        const float r = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset));
        const float g = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset + ((2.0F * PI) / 3.0F)));
        const float b = 0.5F + (0.5F * std::sin((t * 0.9F) + phase_offset + ((4.0F * PI) / 3.0F)));
        return Color(r, g, b, 1.0F);
    }

    static float evaluate_bob_offset(
        const double elapsed_seconds,
        const float frequency,
        const float phase_offset,
        const float amplitude)
    {
        const float t = static_cast<float>(elapsed_seconds);
        return std::sin((t * frequency) + phase_offset) * amplitude;
    }

    void LightingExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        _elapsed_seconds = 0.0;
        auto& ent_registry = host.get_entity_registry();
        auto& input_manager = host.get_input_manager();

        auto& graphics = host.get_settings().graphics;
        graphics.shadow_map_resolution = 2048U;
        graphics.shadow_render_distance = 60.0F;
        graphics.shadow_softness = 1.0F;

        _room.create(
            ent_registry,
            RoomSettings {
                .center = Vec3(0.0F, -1.0F, 0.0F),
                .include_colliders = true,
            });

        auto showcase_sphere = Entity("ShowcaseSphere", ent_registry);
        showcase_sphere.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::LIGHT_GREY}},
        });
        showcase_sphere.add_component<DynamicMesh>(sphere);
        showcase_sphere.add_component<Transform>(
            Vec3(-2.0F, 1.5F, 0.0F),
            Quat(1.0F, 0.0F, 0.0F, 0.0F),
            Vec3(2.0F, 2.0F, 2.0F));

        auto showcase_cube = Entity("ShowcaseCube", ent_registry);
        showcase_cube.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::GREY}},
        });
        showcase_cube.add_component<DynamicMesh>(cube);
        showcase_cube.add_component<Transform>(
            Vec3(2.0F, 0.25F, 0.0F),
            Quat(1.0F, 0.0F, 0.0F, 0.0F),
            Vec3(2.0F, 0.5F, 2.0F));

        _directional_light = Entity("DirectionalLight", ent_registry);
        auto directional_light = DirectionalLight();
        directional_light.ambient = directional_light_enabled_ambient;
        directional_light.intensity = directional_light_enabled_intensity;
        _directional_light.add_component<DirectionalLight>(directional_light);
        _directional_light.add_component<Transform>(
            Vec3(0.0F, 10.0F, 0.0F),
            to_radians(Vec3(-45.0F, 30.0F, 0.0F)),
            Vec3(1.0F));

        _point_base_position = Vec3(-7.5F, 2.5F, -1.0F);
        _point_light = Entity("PointLight", ent_registry);
        auto point_light = PointLight();
        point_light.range = 14.0F;
        point_light.intensity = _point_enabled ? point_light_enabled_intensity : 0.0F;
        _point_light.add_component<PointLight>(point_light);
        _point_light.add_component<Transform>(_point_base_position);

        _point_light_marker = Entity("PointLightMarker", ent_registry);
        _point_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::RED}, {"emissive", Color::RED}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _point_light_marker.add_component<DynamicMesh>(sphere);
        _point_light_marker.add_component<Transform>(
            _point_base_position,
            Quat(1.0F, 0.0F, 0.0F, 0.0F),
            Vec3(0.3F));

        _spot_base_position = Vec3(0.0F, 3.8F, 8.5F);
        _spot_light = Entity("SpotLight", ent_registry);
        auto spot_light = SpotLight();
        spot_light.range = 30.0F;
        spot_light.inner_angle = 18.0F;
        spot_light.outer_angle = 32.0F;
        spot_light.intensity = _spot_enabled ? spot_light_enabled_intensity : 0.0F;
        _spot_light.add_component<SpotLight>(spot_light);
        _spot_light.add_component<Transform>(
            _spot_base_position,
            to_radians(Vec3(-28.0F, 0.0F, 0.0F)),
            Vec3(1.0F));

        _spot_light_marker = Entity("SpotLightMarker", ent_registry);
        _spot_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::GREEN}, {"emissive", Color::GREEN}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _spot_light_marker.add_component<DynamicMesh>(cube);
        _spot_light_marker.add_component<Transform>(
            _spot_base_position,
            to_radians(Vec3(-28.0F, 0.0F, 0.0F)),
            Vec3(0.3F));

        _area_base_position = Vec3(7.5F, 2.5F, -1.0F);
        _area_light = Entity("AreaLight", ent_registry);
        auto area_light = AreaLight();
        area_light.range = 20.0F;
        area_light.area_size = Vec2(3.0F, 2.0F);
        area_light.intensity = _area_enabled ? area_light_enabled_intensity : 0.0F;
        _area_light.add_component<AreaLight>(area_light);
        _area_light.add_component<Transform>(
            _area_base_position,
            to_radians(Vec3(-20.0F, 90.0F, 0.0F)),
            Vec3(1.0F));

        _area_light_marker = Entity("AreaLightMarker", ent_registry);
        _area_light_marker.add_component<Renderer>(Renderer {
            .material =
                MaterialInstance {
                    .parameters = {{"color", Color::BLUE}, {"emissive", Color::BLUE}},
                },
            .shadow_mode = ShadowMode::None,
        });
        _area_light_marker.add_component<DynamicMesh>(capsule);
        _area_light_marker.add_component<Transform>(
            _area_base_position,
            to_radians(Vec3(-20.0F, 90.0F, 0.0F)),
            Vec3(0.5F, 0.3F, 0.1F));

        auto camera = Entity("Camera", ent_registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(
            Vec3(0.0F, 3.5F, 14.0F),
            Quat(Vec3(to_radians(-10.0F), 0.0F, 0.0F)),
            Vec3(1.0F));

        auto light_scheme = InputScheme(_light_scheme_name);
        light_scheme.add_action(create_key_toggle_action(
            "toggle_directional",
            InputKey::ALPHA_0,
            [this](const InputAction&)
            {
                _directional_enabled = !_directional_enabled;
                TBX_TRACE_INFO("Toggle directional light: {}", _directional_enabled);
                auto& light = _directional_light.get_component<DirectionalLight>();
                light.intensity = _directional_enabled ? directional_light_enabled_intensity : 0.0F;
                light.ambient = _directional_enabled ? directional_light_enabled_ambient : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_point",
            InputKey::ALPHA_1,
            [this](const InputAction&)
            {
                _point_enabled = !_point_enabled;
                TBX_TRACE_INFO("Toggle point light: {}", _point_enabled);
                auto& light = _point_light.get_component<PointLight>();
                light.intensity = _point_enabled ? point_light_enabled_intensity : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_spot",
            InputKey::ALPHA_2,
            [this](const InputAction&)
            {
                _spot_enabled = !_spot_enabled;
                TBX_TRACE_INFO("Toggle spot light: {}", _spot_enabled);
                auto& light = _spot_light.get_component<SpotLight>();
                light.intensity = _spot_enabled ? spot_light_enabled_intensity : 0.0F;
            }));
        light_scheme.add_action(create_key_toggle_action(
            "toggle_area",
            InputKey::ALPHA_3,
            [this](const InputAction&)
            {
                _area_enabled = !_area_enabled;
                TBX_TRACE_INFO("Toggle area light: {}", _area_enabled);
                auto& light = _area_light.get_component<AreaLight>();
                light.intensity = _area_enabled ? area_light_enabled_intensity : 0.0F;
            }));
        input_manager.add_scheme(light_scheme);
        input_manager.activate_scheme(_light_scheme_name);
    }

    void LightingExampleRuntimePlugin::on_detach()
    {
        auto& input_manager = get_host().get_input_manager();
        input_manager.remove_scheme(_light_scheme_name);
        _room.destroy();
    }

    void LightingExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        _elapsed_seconds += dt.seconds;

        {
            auto& directional_transform = _directional_light.get_component<Transform>();
            const float yaw_rate = 0.02F;
            float angle = yaw_rate * static_cast<float>(dt.seconds);
            auto rotation_delta = Quat(Vec3(0.0F, angle, 0.0F));
            directional_transform.rotation =
                normalize(rotation_delta * directional_transform.rotation);
        }

        {
            auto& point_light = _point_light.get_component<PointLight>();
            if (_point_enabled)
            {
                point_light.color = evaluate_animated_color(_elapsed_seconds, 0.75F);
                point_light.intensity =
                    point_light_enabled_intensity
                    + (1.2F * std::sin(static_cast<float>(_elapsed_seconds) * 1.1F));

                auto& point_transform = _point_light.get_component<Transform>();
                point_transform.position = _point_base_position;
                point_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 2.4F, 0.1F, 0.9F);

                {
                    auto& marker_transform = _point_light_marker.get_component<Transform>();
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
                    spot_light_enabled_intensity
                    + (1.5F * std::sin(static_cast<float>(_elapsed_seconds) * 1.35F));
                auto& spot_transform = _spot_light.get_component<Transform>();
                spot_transform.position = _spot_base_position;
                spot_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 2.1F, 1.2F, 0.8F);

                {
                    auto& marker_transform = _spot_light_marker.get_component<Transform>();
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
                    area_light_enabled_intensity
                    + (1.0F * std::sin(static_cast<float>(_elapsed_seconds) * 0.95F));

                auto& area_transform = _area_light.get_component<Transform>();
                area_transform.position = _area_base_position;
                area_transform.position.y +=
                    evaluate_bob_offset(_elapsed_seconds, 1.8F, 2.7F, 0.85F);

                {
                    auto& marker_transform = _area_light_marker.get_component<Transform>();
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
    }
}
