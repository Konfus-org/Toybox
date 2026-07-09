#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/world/manager.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

namespace tbx::studio_bridge
{
    // Shared bridge utilities — small world/entity/asset helpers used across the bridge subsystems.

    // Guards that a request carries an object params payload — the shared "Missing request
    // parameters." preamble of every params-taking op.
    Result require_object(const tbx::Json& params);

    // Reads the required unsigned-integer param `key` into `out`, failing with
    // "Missing or invalid '<key>'." when it is absent or not an unsigned number.
    Result require_uint(const tbx::Json& params, std::string_view key, uint64& out);

    // Reads the required string param `key` into `out`, failing with "Missing '<key>'." when it is
    // absent or empty.
    Result require_string(const tbx::Json& params, std::string_view key, std::string& out);

    // Lower-cased file extension without the leading dot, used as the asset's editor "type" so the
    // handle picker can filter (e.g. "mat", "png", "world"). Empty extensions fall back to "asset".
    std::string asset_type_from_path(const std::filesystem::path& path);

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

    // The entity id named by a sync address, accepting both the editor's component template
    // ("entity/{id}/…") and the world-sync path grammar ("world/{w}/entities/{id}/…"); zero when the
    // address names no entity.
    uint64 parse_address_entity(std::string_view address);

    // The editor's wire shape for a Vec3: a compact [x, y, z] array. The ONE place the shape is
    // spelled engine-side — every notification/reply that carries a vector builds it here so the
    // protocol can't drift between call sites.
    tbx::Json to_wire_vec3(const tbx::Vec3& value);

    // The editor's wire shape for a quaternion: a compact [x, y, z, w] array.
    tbx::Json to_wire_quat(const tbx::Quat& value);
}
