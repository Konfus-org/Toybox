#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/world/manager.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/uuid.h"
#include <glm/glm.hpp>

namespace tbx::studio_bridge
{
    // Builds a rotation whose -Z (camera forward) points along `forward`, with the horizon kept
    // level against `world_up`. Used to aim the editor camera at the world.
    tbx::Quat look_rotation(const glm::vec3& forward, const glm::vec3& world_up);

    // Average world position of the world's renderable (static-mesh) entities. Gives the editor
    // camera something meaningful to face when it opens. Returns false when the world has no
    // renderable geometry.
    bool compute_world_focus(tbx::World& world, glm::vec3& out_focus);

    // Expands [out_min, out_max] (world space) by an entity's static-mesh corners transformed by
    // its world matrix, mirroring how the renderer places parts vs. bare meshes. Returns whether it
    // contributed (false when the entity has no loadable model or no valid bounds).
    bool accumulate_entity_world_bounds(
        tbx::AssetManager& assets,
        tbx::Entity entity,
        glm::vec3& out_min,
        glm::vec3& out_max);

    // Whether `entity` is `ancestor` itself or sits anywhere beneath it (walks the parent chain).
    bool is_self_or_descendant(tbx::World& world, tbx::Entity entity, const tbx::Uuid& ancestor);

    // Builds a world-space ray through a normalized image point (top-left origin), correct for both
    // perspective and orthographic cameras (two-point unprojection).
    void make_cursor_ray(
        const tbx::CameraView& camera_view,
        float u,
        float v,
        glm::vec3& out_origin,
        glm::vec3& out_direction);

    // Parameter along the (unit) axis through pivot of the point closest to the (unit) cursor ray.
    float closest_param_on_axis(
        const glm::vec3& ray_origin,
        const glm::vec3& ray_direction,
        const glm::vec3& pivot,
        const glm::vec3& axis);

    // Projects a world point to a normalized screen point (top-left origin). False when behind the
    // camera.
    bool project_to_screen(
        const glm::mat4& view_projection,
        const glm::vec3& world,
        float& out_u,
        float& out_v);

    // Distance from a 2D point to a 2D segment.
    float distance_point_segment(float px, float py, float ax, float ay, float bx, float by);

    // Intersects a ray with a plane (point p0, normal n). False when (almost) parallel.
    bool ray_plane(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& p0,
        const glm::vec3& normal,
        glm::vec3& out_hit);

    // Signed angle from a to b about axis (radians). Zero when either vector is degenerate.
    float signed_angle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& axis);

    // Local transform that places an entity at the desired world transform (accounts for its
    // parent).
    tbx::Transform world_to_local_for(tbx::Entity entity, const tbx::Transform& new_world);
}
