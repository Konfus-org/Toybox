#include "tbx/reflection/reflection.h"
#include "tbx/app.h"
#include "tbx/audio/audio_clip.h"
#include "tbx/audio/audio_listener.h"
#include "tbx/audio/audio_source.h"
#include "tbx/ecs/box.h"
#include "tbx/ecs/kit.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/renderer.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/sky.h"
#include "tbx/gfx/texture.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/scripting/script.h"
#include "tbx/scripting/script_source.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui_block.h"
#include "tbx/ui/ui_document.h"

namespace tbx::reflection
{

    /// @brief
    /// Purpose: Registers every builtin block type. Idempotent.
    static void register_blocks()
    {
        // Deriving tbx::ecs::Block is what makes these blocks — register_type stamps the ecs
        // facet from the base automatically.
        register_type<Transform>("Transform")
            .field("position", &Transform::position)
            .field("rotation", &Transform::rotation)
            .field("scale", &Transform::scale);
        register_type<gfx::Camera>("Camera")
            .field("fov_degrees", &gfx::Camera::fov_degrees)
            .field("near_plane", &gfx::Camera::near_plane)
            .field("far_plane", &gfx::Camera::far_plane)
            .field("window", &gfx::Camera::window)
            .field("viewport", &gfx::Camera::viewport);
        register_type<gfx::Renderer>("Renderer")
            .field("material", &gfx::Renderer::material)
            .field("model", &gfx::Renderer::model);
        register_type<gfx::DirectionalLight>("DirectionalLight")
            .field("color", &gfx::DirectionalLight::color)
            .field("intensity", &gfx::DirectionalLight::intensity);
        register_type<physics::RigidBody>("RigidBody")
            .field("mass", &physics::RigidBody::mass)
            .field("is_kinematic", &physics::RigidBody::is_kinematic);
        register_type<physics::Collider>("Collider")
            .field("shape", &physics::Collider::shape)
            .field("half_extents", &physics::Collider::half_extents)
            .field("radius", &physics::Collider::radius)
            .field("height", &physics::Collider::height);
        register_type<ui::Ui>("Ui")
            .field("document", &ui::Ui::document)
            .field("vertex", &ui::Ui::vertex)
            .field("fragment", &ui::Ui::fragment)
            .field("is_world_anchored", &ui::Ui::is_world_anchored);
        register_type<gfx::Sky>("Sky").field("texture", &gfx::Sky::texture).field("tint", &gfx::Sky::tint);
        register_type<gfx::PostProcessing>("PostProcessing").field("shaders", &gfx::PostProcessing::shaders);
        register_type<scripts::Script>("Script").field("source", &scripts::Script::source);
        register_type<audio::AudioListener>("AudioListener").field("volume", &audio::AudioListener::volume);
        register_type<audio::AudioSource>("AudioSource")
            .field("clip", &audio::AudioSource::clip)
            .field("volume", &audio::AudioSource::volume)
            .field("is_looping", &audio::AudioSource::is_looping)
            .field("is_playing", &audio::AudioSource::is_playing);
    }

    /// @brief
    /// Purpose: Registers every builtin asset type — deriving tbx::Asset is what makes them
    /// assets; register_type stamps the load/hot-reload facet from the base automatically.
    /// Types with real decoders (stb, assimp, raw text...) keep a load<T> specialization;
    /// plain data types (gfx::Material, ecs::Box) decode generically through their fields. Idempotent.
    static void register_builtin_assets()
    {
        register_type<gfx::Texture>("Texture");
        register_type<gfx::Model>("Model");
        register_type<gfx::ShaderSource>("ShaderSource");
        register_type<audio::AudioClip>("AudioClip");
        register_type<scripts::ScriptSource>("ScriptSource");
        register_type<ui::UiDocument>("UiDocument");
        register_type<ui::Font>("Font");
        register_type<ecs::Kit>("Kit");
        register_type<gfx::Material>("Material")
            .field("vertex", &gfx::Material::vertex)
            .field("fragment", &gfx::Material::fragment)
            .field("albedo_map", &gfx::Material::albedo_map)
            .field("normal_map", &gfx::Material::normal_map)
            .field("metallic_map", &gfx::Material::metallic_map)
            .field("roughness_map", &gfx::Material::roughness_map)
            .field("albedo", &gfx::Material::albedo)
            .field("emissive", &gfx::Material::emissive)
            .field("metallic", &gfx::Material::metallic)
            .field("roughness", &gfx::Material::roughness)
            .field("uv_scale", &gfx::Material::uv_scale);
        register_type<ecs::BoxEntry>("BoxEntry")
            .field("reference", &ecs::BoxEntry::kit)
            .field("mode", &ecs::BoxEntry::mode)
            .field("position", &ecs::BoxEntry::position);
        register_type<ecs::Box>("Box").field("kits", &ecs::Box::kits);
    }

    /// @brief
    /// Purpose: Registers the App struct and its settings groups — the .tapp schema.
    static void register_app_types()
    {
        register_type<GraphicsSettings>("GraphicsSettings")
            .field("is_vsync_enabled", &GraphicsSettings::is_vsync_enabled)
            .field("is_custom_pipeline", &GraphicsSettings::is_custom_pipeline)
            .field("shadow_resolution", &GraphicsSettings::shadow_resolution);
        register_type<PhysicsSettings>("PhysicsSettings")
            .field("fixed_timestep", &PhysicsSettings::fixed_timestep)
            .field("gravity", &PhysicsSettings::gravity);
        register_type<AudioSettings>("AudioSettings")
            .field("master_volume", &AudioSettings::master_volume);
        register_type<AssetSettings>("AssetSettings")
            .field("idle_lifetime_seconds", &AssetSettings::idle_lifetime_seconds);
        register_type<AppConfig>("AppConfig")
            .field("title", &AppConfig::title)
            .field("width", &AppConfig::width)
            .field("height", &AppConfig::height)
            .field("is_headless", &AppConfig::is_headless)
            .field("sandbox", &AppConfig::sandbox)
            .field("icon", &AppConfig::icon);
        register_type<AppSettings>("AppSettings")
            .field("graphics", &AppSettings::graphics)
            .field("physics", &AppSettings::physics)
            .field("audio", &AppSettings::audio)
            .field("assets", &AppSettings::assets);
        register_type<App>("App").field("config", &App::config).field("settings", &App::settings);
    }

    void initialize()
    {
        static bool g_registered = false;
        if (g_registered)
            return;

        register_blocks();
        register_builtin_assets();
        register_app_types();

        g_registered = true;
    }
}