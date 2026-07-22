#include "tbx/reflection/reflection.h"
#include "tbx/app.h"
#include "tbx/audio/clip.h"
#include "tbx/audio/listener.h"
#include "tbx/audio/source.h"
#include "tbx/ecs/billboard.h"
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
#include "tbx/scripting/source.h"
#include "tbx/serialization/serializers.h"
#include "tbx/ui/document.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui_block.h"

namespace tbx
{

    /// @brief
    /// Purpose: Registers every builtin block type. Idempotent.
    static void register_blocks()
    {
        // Deriving tbx::Block is what makes these blocks — register_type stamps the ecs
        // facet from the base automatically.
        register_type<Transform>("Transform")
            .field("position", &Transform::position)
            .field("rotation", &Transform::rotation)
            .field("scale", &Transform::scale);
        register_type<Camera>("Camera")
            .field("fov_degrees", &Camera::fov_degrees)
            .field("near_plane", &Camera::near_plane)
            .field("far_plane", &Camera::far_plane)
            .field("window", &Camera::window)
            .field("viewport", &Camera::viewport);
        register_type<Renderer>("Renderer")
            .field("material", &Renderer::material)
            .field("model", &Renderer::model);
        register_type<DirectionalLight>("DirectionalLight")
            .field("color", &DirectionalLight::color)
            .field("intensity", &DirectionalLight::intensity);
        register_type<RigidBody>("RigidBody")
            .field("mass", &RigidBody::mass)
            .field("is_kinematic", &RigidBody::is_kinematic);
        register_type<Collider>("Collider")
            .field("shape", &Collider::shape)
            .field("half_extents", &Collider::half_extents)
            .field("radius", &Collider::radius)
            .field("height", &Collider::height);
        register_type<UI>("UI")
            .field("document", &UI::document)
            .field("vertex", &UI::vertex)
            .field("fragment", &UI::fragment)
            .field("mode", &UI::mode);
        register_type<Sky>("Sky")
            .field("texture", &Sky::texture)
            .field("tint", &Sky::tint);
        register_type<PostProcessing>("PostProcessing")
            .field("shaders", &PostProcessing::shaders);
        register_type<Script>("Script").field("source", &Script::source);
        register_type<AudioListener>("AudioListener").field("volume", &AudioListener::volume);
        register_type<AudioSource>("AudioSource")
            .field("clip", &AudioSource::clip)
            .field("volume", &AudioSource::volume)
            .field("is_looping", &AudioSource::is_looping)
            .field("is_playing", &AudioSource::is_playing);
        // A toy wearing this block is a nested kit; its children are the kit's contents.
        register_type<KitInstance>("KitInstance")
            .field("kit", &KitInstance::kit)
            .field("streamed", &KitInstance::streamed);
        // Opt-in billboarding — a toy only faces the camera with this block.
        register_type<Billboard>("Billboard").field("lock_y", &Billboard::lock_y);
    }

    /// @brief
    /// Purpose: Registers every builtin asset type's SHAPE. How each one reads/writes on
    /// disk is the serializer registry's business — register_builtin_serializers() pairs
    /// every entry here with its serializer. Idempotent.
    static void register_builtin_assets()
    {
        register_type<Texture>("Texture");
        register_type<Model>("Model");
        register_type<ShaderSource>("ShaderSource");
        register_type<AudioClip>("AudioClip");
        register_type<ScriptSource>("ScriptSource");
        register_type<Document>("UiDocument");
        register_type<Font>("Font");
        register_type<Kit>("Kit");
        register_type<Material>("Material")
            .field("vertex", &Material::vertex)
            .field("fragment", &Material::fragment)
            .field("albedo_map", &Material::albedo_map)
            .field("normal_map", &Material::normal_map)
            .field("metallic_map", &Material::metallic_map)
            .field("roughness_map", &Material::roughness_map)
            .field("albedo", &Material::albedo)
            .field("emissive", &Material::emissive)
            .field("metallic", &Material::metallic)
            .field("roughness", &Material::roughness)
            .field("uv_scale", &Material::uv_scale);
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

    void initialize_reflection()
    {
        static bool g_registered = false;
        if (g_registered)
            return;

        register_blocks();
        register_builtin_assets();
        register_app_types();
        // Shapes first, serializers second — Format::DEFAULT validates reflection exists.
        register_builtin_serializers();

        g_registered = true;
    }
}