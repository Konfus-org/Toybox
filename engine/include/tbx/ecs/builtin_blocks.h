#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/material.h"
#include "tbx/assets/model.h"
#include "tbx/assets/script_source.h"
#include "tbx/assets/shader_source.h"
#include "tbx/assets/texture.h"
#include "tbx/assets/ui_document.h"
#include "tbx/core/api.h"
#include "tbx/core/color.h"
#include "tbx/core/math.h"
#include "tbx/core/typedefs.h"
#include <string>
#include <vector>


// Every block the engine ships, in one place. Registration stays with each owning system
// (sandbox registers Transform, gpu the render blocks, physics its blocks, scripting Script).
namespace tbx
{
    /// @brief
    /// Purpose: The shared spatial-shape vocabulary — colliders and spatial audio sources both
    /// speak it (BOX uses half_extents, SPHERE radius, CAPSULE radius + height).
    enum class Shape : uint8
    {
        BOX,
        SPHERE,
        CAPSULE
    };

    /// @brief
    /// Purpose: Collision shape centered on the toy's Transform, described by Shape.
    struct TBX_API Collider
    {
        Shape shape = Shape::BOX;
        Vec3 half_extents = Vec3(0.5f, 0.5f, 0.5f);
        float radius = 0.5f;
        float height = 1.0f;
    };

    /// @brief
    /// Purpose: A viewpoint: perspective settings; position/orientation come from Transform
    /// (looks along its -Z). The first camera renders.
    struct TBX_API Camera
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
    };

    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct TBX_API DirectionalLight
    {
        Color color = {};
        float intensity = 1.0f;
    };

    /// @brief
    /// Purpose: Makes a toy visible: an imported model when the handle is set, otherwise a
    /// builtin primitive by name (tbx::builtin), textured when the texture handle is set,
    /// always tinted.
    struct TBX_API Renderer
    {
        AssetHandle<Material> material = {};
        AssetHandle<Model> model = {};
        AssetHandle<Texture> texture = {};
        std::string mesh = "cube";
        Color tint = {};
    };

    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct TBX_API RigidBody
    {
        float mass = 1.0f;
        bool is_kinematic = false;
    };

    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// module exposes start(toy), update(toy, delta_time), and fixed_update(toy, delta_time).
    struct TBX_API Script
    {
        AssetHandle<ScriptSource> source = {};
    };

    /// @brief
    /// Purpose: On-screen UI owned by a toy: an RML document shown while the toy lives and
    /// is enabled (the renderer's ui pass manages loading/visibility).
    struct Ui
    {
        AssetHandle<UiDocument> document = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: The sky: an equirectangular texture rendered behind everything. One per
    /// sandbox (the first wins), usually on a dedicated environment toy.
    struct TBX_API Sky
    {
        AssetHandle<Texture> texture = {};
        Color tint = {};
    };

    /// @brief
    /// Purpose: Full-screen post processing: just a list of fragment shaders, applied to the
    /// rendered scene in order. Each shader samples u_scene (plus u_resolution and u_time).
    /// One per sandbox (the first wins).
    struct TBX_API PostProcessing
    {
        std::vector<AssetHandle<ShaderSource>> shaders = {};
    };

    /// @brief
    /// Purpose: The one engine-core spatial block: local position/rotation/scale. Hierarchy
    /// lives on the Sandbox (set_parent/get_parent), not inside the block.
    struct TBX_API Transform
    {
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    };
}
