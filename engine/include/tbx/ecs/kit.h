#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/assets.h"
#include "tbx/assets/load.h"
#include "tbx/ecs/toy.h"
#include "tbx/events/events.h"
#include "tbx/math/math.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <span>
#include <string>
#include <vector>

namespace tbx::ecs
{
    class Sandbox;
    struct TBX_API Kit;

    /// @brief
    /// Purpose: One block on a kit toy, strongly typed in memory: the reflected type's name
    /// hash plus a boxed instance of it (an empty value or unknown type skips on load).
    struct TBX_API KitBlock
    {
        uint64 type = 0;
        std::any value = {};
    };

    /// @brief
    /// Purpose: One toy inside a kit: identity for parent links (fresh live uuids are minted
    /// on load), enablement, stickers, and its blocks.
    struct TBX_API KitToy
    {
        Uuid uuid = {};
        std::string name = "Toy";
        bool is_enabled = true;
        Uuid parent = {}; // another kit toy's uuid; nil = root
        std::vector<std::string> stickers = {};
        std::vector<KitBlock> blocks = {};
    };

    /// @brief
    /// Purpose: A nested kit reference: which kit and where it sits relative to the parent.
    struct TBX_API KitReference
    {
        assets::AssetHandle<Kit> kit = {};
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    };

    /// @brief
    /// Purpose: A set of things, strongly typed in memory: toys (with their blocks and
    /// stickers) plus references to other kits, recursively — one concept covering prefab,
    /// scene, level, and chunk. A kit is an ordinary asset (assets::AssetHandle<Kit>, .kit files);
    /// JSON exists only at the load<Kit>/to_json serialize boundary.
    struct TBX_API Kit : assets::Asset
    {
        std::vector<KitToy> toys = {};
        std::vector<KitReference> kits = {};
        Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
        float bounds_radius = 0.0f;
    };

    /// @brief
    /// Purpose: Handle to one instantiated kit; Sandbox::despawn(instance) removes exactly
    /// the toys it spawned (including toys from nested kit references).
    struct TBX_API KitInstance
    {
        uint64 id = 0;
    };

    /// @brief
    /// Purpose: The kit's on-disk JSON form ({toys, kits, bounds}) — the serialize half of
    /// the boundary; blocks write through their reflected types.
    TBX_API serialization::Json to_json(const Kit& kit);

    /// @brief
    /// Purpose: Serializes chosen toys (blocks, stickers, parent links, bounds) as a kit.
    TBX_API Kit save(Sandbox& sandbox, std::span<const Toy> toys);

    /// @brief
    /// Purpose: Serializes the whole sandbox — every live toy — as a kit.
    TBX_API Result<Kit> save(Sandbox& sandbox);

    /// @brief
    /// Purpose: Serializes one toy (with its blocks and stickers) as a kit.
    TBX_API Result<Kit> save(const Toy& toy);

    /// @brief
    /// Purpose: Instantiates a kit into a sandbox — the load half of save() round-trips and
    /// what Sandbox::spawn(assets::AssetHandle<Kit>) runs on. Nested kit references resolve
    /// recursively through the sandbox's assets; cycles are load errors; root position
    /// offsets every parentless toy.
    TBX_API Result<KitInstance> load(
        Sandbox& sandbox,
        assets::AssetsState& assets,
        events::EventsState& events,
        const Kit& kit,
        const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f));
}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a .kit file — the deserialize half of the kit's JSON boundary.
    template <>
    TBX_API Result<ecs::Kit> load<ecs::Kit>(const std::filesystem::path& path);
}
