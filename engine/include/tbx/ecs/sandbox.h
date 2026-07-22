#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/container.h"
#include "tbx/ecs/kit.h"
#include "tbx/jobs/jobs.h"
#include "tbx/math/frustum.h"
#include "tbx/math/math.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: A streamed kit instance the world is tracking — the KitInstance toy plus the
    /// bounds sphere that decides when its contents load/unload by camera sight.
    struct StreamedKit
    {
        ToyId instance = NULL_TOY; // the KitInstance toy whose children stream in/out
        AssetHandle<Kit> kit = {};
        Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
        float bounds_radius = 0.0f;
        bool is_loading = false;
        bool is_loaded = false;
    };

    /// @brief
    /// Purpose: THE world container: a ToyContainer that also opens a level kit and streams
    /// its streamed child kits by camera sight. Plain data — every toy query/mutation goes
    /// through the shared ToyContainer surface (spawn/find/each/despawn/...), and world
    /// lifecycle (open/close/spawn/stream/update_ecs) lives in the free functions below.
    /// Serialization lives on read/write — write(sandbox, path) saves the world as a kit,
    /// read<Sandbox>(kit_path) loads one.
    /// @details
    /// Movable so read<Sandbox> can hand one over — move it only while no streamed loads are
    /// in flight (startup, or right after close()). Copy is deleted: a world is unique.
    struct TBX_API Sandbox : ToyContainer
    {
        Sandbox() = default;
        Sandbox(Sandbox&&) = default;
        Sandbox& operator=(Sandbox&&) = default;
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

        std::vector<StreamedKit> streamed_kits;
        std::optional<Kit> pending_level;
    };

    /// @brief
    /// Purpose: Opens a kit as the whole world (a "level"): its toys spawn on the next stream()
    /// tick — immediate child kits expand at once, streamed ones when a camera looks their way.
    TBX_API void open(Sandbox& sandbox, Kit level);

    /// @brief
    /// Purpose: Unloads everything: every toy and all streaming state. The sandbox is empty and
    /// ready to open another level.
    TBX_API void close(Sandbox& sandbox);

    /// @brief
    /// Purpose: Instantiates a kit: creates the instance's root toy (a KitInstance block naming
    /// the kit) at `position` and spawns the kit's toys as its children. Nested immediate kits
    /// expand recursively; nested streamed kits register for streaming. Returns the root toy;
    /// despawn_subtree(root) removes the whole instance.
    TBX_API Result<Toy> spawn(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const AssetHandle<Kit>& kit,
        const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

    /// @brief
    /// Purpose: Spawns an in-memory kit (the handle overload resolves through assets and lands
    /// here).
    TBX_API Result<Toy> spawn(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const Kit& kit,
        const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

    /// @brief
    /// Purpose: Loads streamed kits whose bounds sphere is in sight of ANY frustum (one per
    /// enabled camera; tbx::run() gathers them every frame) and unloads kits out of sight of ALL
    /// of them. Also flushes a pending open() first. Load-only: streaming never writes disk.
    TBX_API void stream(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        std::span<const Frustum> frustums);

    /// @brief
    /// Purpose: The per-frame ECS tick: updates builtin components (billboards face the active
    /// camera) then streams. tbx::run() calls this once, after scripts/physics settle transforms
    /// and before rendering.
    TBX_API void update_ecs(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        std::span<const Frustum> frustums);

    /// @brief
    /// Purpose: Sandbox's registered reader — a level file IS a sandbox: reads a .kit and
    /// opens it as a fresh world (its toys spawn on the first stream() tick). Call it through
    /// deserialize<Sandbox>(path).
    TBX_API Result<Sandbox> deserialize_sandbox(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Sandbox's registered writer — every live toy captured as a kit file. Call it
    /// through serialize(sandbox, path).
    TBX_API Result<void> serialize_sandbox(const Sandbox& sandbox, const std::filesystem::path& path);
}
