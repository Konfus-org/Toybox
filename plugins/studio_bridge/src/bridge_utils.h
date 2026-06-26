#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/uuid.h"
#include <glm/glm.hpp>

namespace tbx::studio_bridge
{
    // Shared bridge utilities — small world/entity helpers used across the bridge subsystems.

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

    // Local transform that places an entity at the desired world transform (accounts for its
    // parent).
    tbx::Transform world_to_local_for(tbx::Entity entity, const tbx::Transform& new_world);
}
