#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/container.h"
#include "tbx/ecs/kit.h"
#include "tbx/jobs/jobs.h"
#include "tbx/math/frustum.h"
#include "tbx/platform/window.h"
#include <filesystem>
#include <optional>
#include <span>

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

    struct EventsState; // wired in at boot (below); the asset/kit path reaches events through it

    /// @brief
    /// Purpose: How open(kit) folds the kit into the world: REPLACE makes the kit the whole world
    /// (closing what's there first), ADDITIVE instantiates it into the current world alongside
    /// everything already open.
    enum class OpenMode : uint8
    {
        REPLACE = 0,
        ADDITIVE
    };

    /// @brief
    /// Purpose: THE world container: a ToyContainer that also opens a level kit and streams
    /// its streamed child kits by camera sight. Plain data — every toy query/mutation goes
    /// through the shared ToyContainer surface (add/find/each/remove/...), and world
    /// lifecycle (open/close/add/stream/update_ecs) lives in the free functions below.
    /// Serialization lives on read/write — write(sandbox, path) saves the world as a kit,
    /// read<Sandbox>(kit_path) loads one.
    /// @details
    /// Movable so read<Sandbox> can hand one over — move it only while no streamed loads are
    /// in flight (startup, or right after close()). Copy is deleted: a world is unique.
    struct TBX_DLL_EXPORT Sandbox : ToyContainer
    {
        Sandbox() = default;
        Sandbox(Sandbox&&) = default;
        Sandbox& operator=(Sandbox&&) = default;
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

        // Wired at boot so kit add reaches the asset system directly. Raw views: the runtime owns
        // both modules and outlives the sandbox's use of them.
        AssetsState* assets = nullptr;
        EventsState* events = nullptr;

        std::vector<StreamedKit> streamed_kits;
        std::optional<Kit> pending_level;
        // A root kit opened by handle, resolved on the next stream() tick (so open() never needs the
        // asset system in hand). Set by open(handle, REPLACE); the free open(root) routes here.
        std::optional<AssetHandle<Kit>> pending_root = {};

        using ToyContainer::add; // keep add(name) (empty toy) alongside the kit add overloads

        /// @brief
        /// Purpose: Instantiates a kit into the world at `position` — the instance root (a
        /// KitInstance toy) with the kit's toys as its children; nested immediate kits expand,
        /// nested streamed kits register for streaming. Reads through the wired asset system.
        /// Returns the root toy; remove(root) removes the whole instance.
        Result<Toy> add(const AssetHandle<Kit>& kit, const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));
        Result<Toy> add(const Kit& kit, const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

        /// @brief
        /// Purpose: Opens a kit into the world and returns the world for chaining — REPLACE makes it
        /// the whole world (deferred to the next tick), ADDITIVE instantiates it now. Fluent: an
        /// additive open that fails is logged and skipped (call add(kit) when you need the Result).
        Sandbox& open(const AssetHandle<Kit>& kit, OpenMode mode = OpenMode::ADDITIVE);

        /// @brief
        /// Purpose: Spawns a toy in the world and returns its handle — the world-verb spelling of
        /// add(name).
        Toy spawn(std::string name);
    };

    /// @brief
    /// Purpose: Opens a kit as the whole world (a "level"): its toys add on the next
    /// update_sandbox() tick — immediate child kits expand at once, streamed ones when a camera
    /// looks their way.
    TBX_DLL_EXPORT void open(Sandbox& sandbox, Kit level);

    /// @brief
    /// Purpose: Unloads everything: every toy and all streaming state. The sandbox is empty and
    /// ready to open another level.
    TBX_DLL_EXPORT void close(Sandbox& sandbox);

    /// @brief
    /// Purpose: Sandbox's registered reader — a level file IS a sandbox: reads a .kit and
    /// opens it as a fresh world (its toys add on the first stream() tick). Call it through
    /// deserialize<Sandbox>(path).
    TBX_DLL_EXPORT Result<Sandbox> deserialize_sandbox(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Sandbox's registered writer — every live toy captured as a kit file. Call it
    /// through serialize(sandbox, path).
    TBX_DLL_EXPORT Result<void> serialize_sandbox(
        const Sandbox& sandbox,
        const std::filesystem::path& path);

}
