#pragma once
#include "tbx/core/color.h"
#include "tbx/core/math.h"
#include <string>

// Every block the engine ships, in one place. Registration stays with each owning system
// (sandbox registers Transform, gpu the render blocks, physics its blocks, scripting Script).
namespace tbx
{
    /// @brief
    /// Purpose: Box collision shape centered on the toy's Transform.
    struct BoxCollider
    {
        Vec3 half_extents = Vec3(0.5f, 0.5f, 0.5f);
    };

    /// @brief
    /// Purpose: A viewpoint: perspective settings; position/orientation come from Transform
    /// (looks along its -Z). The first camera renders.
    struct Camera
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
    };

    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct DirectionalLight
    {
        Color color = {};
        float intensity = 1.0f;
    };

    /// @brief
    /// Purpose: Makes a toy visible: a named mesh (see tbx::builtin for primitives; asset
    /// meshes come with model loading) with a tint.
    struct MeshRenderer
    {
        std::string mesh = "cube";
        Color tint = {};
    };

    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct RigidBody
    {
        float mass = 1.0f;
        bool is_kinematic = false;
    };

    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// module exposes start(toy), update(toy, delta_time), and fixed_update(toy, delta_time).
    struct Script
    {
        std::string source = {};
    };

    /// @brief
    /// Purpose: The one engine-core spatial block: local position/rotation/scale. Hierarchy
    /// lives on the Sandbox (set_parent/get_parent), not inside the block.
    struct Transform
    {
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    };
}
