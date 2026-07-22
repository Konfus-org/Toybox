#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/container.h"
#include "tbx/math/math.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include <filesystem>

namespace tbx
{
    struct TBX_API Kit;

    /// @brief
    /// Purpose: A block marking a toy as an instantiated (or to-be-instantiated) kit: the
    /// toy's children ARE the kit's contents. Child kits are just child toys wearing this
    /// block — a nested kit is authored as a toy with a KitInstance, positioned by its
    /// transform. `streamed` defers that expansion to the streaming system: the kit loads
    /// when a camera looks its way and unloads when none does. Serializes like any block.
    struct TBX_API KitInstance : Block
    {
        AssetHandle<Kit> kit = {};
        bool streamed = false;

        KitInstance& set_kit(AssetHandle<Kit> value)
        {
            kit = std::move(value);
            return *this;
        }
        KitInstance& set_streamed(bool value)
        {
            streamed = value;
            return *this;
        }
    };

    /// @brief
    /// Purpose: A kit is a container of toys — real Toys arranged by the ordinary parent/child
    /// links, with nested kits sitting right in that hierarchy as toys wearing a KitInstance
    /// block. One concept covering prefab, scene, level, and chunk: a prefab is a kit you
    /// reference, a level is a kit you open as the world. It is an ordinary asset
    /// (AssetHandle<Kit>, .kit files) that serializes itself and its children.
    /// @details
    /// It shares its whole toy-container shape and query surface with Sandbox (ToyContainer);
    /// the registry is hidden. It stays copyable (it rides in the asset cache).
    struct TBX_API Kit : ToyContainer, Asset
    {
        // How far the kit reaches from its origin — streaming uses this for the load/unload
        // distance (authored, preserved across read/write).
        Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
        float bounds_radius = 0.0f;
    };

    /// @brief
    /// Purpose: Kit's registered reader — the .kit JSON schema ({toys, bounds}) into a kit
    /// (toys spawned into its container). Call it through deserialize<Kit>(path).
    TBX_API Result<Kit> deserialize_kit(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Kit's registered writer — call it through serialize(kit, path).
    TBX_API Result<void> serialize_kit(const Kit& kit, const std::filesystem::path& path);
}
