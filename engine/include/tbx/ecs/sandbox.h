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
    /// Purpose: THE world container: a ToyContainer that also opens a level kit and streams
    /// its streamed child kits by camera sight. Every toy query/mutation goes through the
    /// shared ToyContainer surface (spawn/find/each/despawn/...); the registry is hidden.
    /// Serialization lives on read/write — write(sandbox, path) saves the
    /// world as a kit, read<Sandbox>(kit_path) loads one.
    /// @details
    /// Movable so read<Sandbox> can hand one over — move it only while no streamed loads are
    /// in flight (startup, or right after close()). Copy is deleted: a world is unique.
    class TBX_API Sandbox final : public ToyContainer
    {
      public:
        Sandbox();
        Sandbox(Sandbox&&) = default;
        Sandbox& operator=(Sandbox&&) = default;
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

      public:
        // The kit-spawning overloads below would otherwise hide the toy-spawning ones.
        using ToyContainer::spawn;
        /// @brief
        /// Purpose: Opens a kit as the whole world (a "level"): its toys spawn on the next
        /// stream() tick — immediate child kits expand at once, streamed ones when a camera
        /// looks their way.
        void open(Kit level);

        /// @brief
        /// Purpose: Unloads everything: every toy and all streaming state. The sandbox is
        /// empty and ready to open another level.
        void close();

        /// @brief
        /// Purpose: Instantiates a kit: creates the instance's root toy (a KitInstance block
        /// naming the kit) at `position` and spawns the kit's toys as its children. Nested
        /// immediate kits expand recursively; nested streamed kits register for streaming.
        /// Returns the root toy; despawn_subtree(root) removes the whole instance.
        Result<Toy> spawn(
            AssetsState& assets,
            EventsState& events,
            const AssetHandle<Kit>& kit,
            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

        /// @brief
        /// Purpose: Spawns an in-memory kit (the handle overload resolves through assets and
        /// lands here).
        Result<Toy> spawn(
            AssetsState& assets,
            EventsState& events,
            const Kit& kit,
            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

        /// @brief
        /// Purpose: Loads streamed kits whose bounds sphere is in sight of ANY frustum (one
        /// per enabled camera; tbx::run() gathers them every frame) and unloads kits out of
        /// sight of ALL of them. Also flushes a pending open() first. Load-only: streaming
        /// never writes disk.
        void stream(
            AssetsState& assets,
            EventsState& events,
            JobsState& jobs,
            std::span<const Frustum> frustums);

      private:
        static constexpr float STREAM_LOAD_MARGIN = 5.0f;
        // Larger than the load margin on purpose: the hysteresis band is what keeps a camera
        // turning in place from thrashing loads — raise it if turning still pops.
        static constexpr float STREAM_UNLOAD_MARGIN = 15.0f;

        struct StreamedKit
        {
            ToyId instance = NULL_TOY; // the KitInstance toy whose children stream in/out
            AssetHandle<Kit> kit = {};
            Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
            float bounds_radius = 0.0f;
            bool is_loading = false;
            bool is_loaded = false;
        };

      private:
        /// @brief
        /// Purpose: Spawns the pending level (open() defers so opening never needs the asset
        /// system in hand).
        void open_pending(AssetsState& assets, EventsState& events);

        /// @brief
        /// Purpose: Copies a kit's toys under `parent`, expanding nested immediate kits and
        /// registering nested streamed kits — the shared body of spawn() and stream-in.
        /// (Defined in kit.cpp with the kit machinery.)
        Result<void> instantiate_under(
            AssetsState& assets,
            EventsState& events,
            Toy parent,
            const Kit& kit,
            std::vector<uint64>& reference_stack,
            std::vector<ToyId>& spawned);

        /// @brief
        /// Purpose: Records a streamed KitInstance toy (peeks the referenced kit's bounds).
        void register_streamed_kit(
            AssetsState& assets,
            EventsState& events,
            Toy instance,
            const AssetHandle<Kit>& kit);

      private:
        std::vector<StreamedKit> _streamed_kits;
        std::optional<Kit> _pending_level;
    };

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
