#pragma once
#include "tbx/assets/assets.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/box.h"
#include "tbx/ecs/toy.h"
#include "tbx/jobs/jobs.h"
#include "tbx/math/math.h"
#include "tbx/math/transform.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::ecs
{

    /// @brief
    /// Purpose: THE world container: owns every toy and streams sandbox-level kit entries by
    /// distance. Serialization lives on tbx::save / tbx::load (save_load.h), not here.
    /// @details
    /// Ownership: Owns the registry and all kit bookkeeping. Thread Safety: Structural
    /// mutation on the main thread only; streaming resolves kit bodies on workers and splices
    /// on the main thread via Jobs.
    class TBX_API Sandbox final
    {
      public:
        Sandbox();

      public:
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

      public:
        /// @brief
        /// Purpose: Finds a toy by runtime uuid.
        std::optional<Toy> find(const Uuid& uuid);

        /// @brief
        /// Purpose: Finds the first toy with the given name.
        std::optional<Toy> find(std::string_view name);

        /// @brief
        /// Purpose: Invokes the callback for every toy wearing the sticker.
        void for_each_sticker(std::string_view name, const std::function<void(Toy)>& callback);

        /// @brief
        /// Purpose: A toy's parent, when it has one. Non-const because a Toy is a mutation
        /// handle over its sandbox.
        std::optional<Toy> get_parent(Toy child);

        /// @brief
        /// Purpose: Direct registry access — the sandbox exposes its internals deliberately;
        /// systems iterate views without ceremony.
        Registry& get_registry()
        {
            return _registry;
        }

        /// @brief
        /// Purpose: Number of live toys.
        size get_toy_count() const;

        /// @brief
        /// Purpose: Composed world matrix walking the parent chain.
        Mat4 get_world_matrix(Toy toy) const;

        /// @brief
        /// Purpose: Reparents a toy (pass a default Toy to clear the parent).
        void set_parent(Toy child, Toy parent);

        /// @brief
        /// Purpose: Opens a box: ALWAYS entries load immediately; STREAMED entries
        /// load/unload by distance to the streaming focus (see stream()).
        Result<void> open(assets::AssetsState& assets, events::EventsState& events, const Box& box);

        /// @brief
        /// Purpose: Unloads everything: every kit instance, every toy, and all streaming
        /// state. The sandbox is empty and ready to open another box.
        void close();

        /// @brief
        /// Purpose: Spawns a kit asset: instantiates its toys (nested kit references resolve
        /// recursively through assets; cycles are errors; position offsets parentless toys)
        /// and returns the instance handle for despawn().
        Result<KitInstance> spawn(
            assets::AssetsState& assets,
            events::EventsState& events,
            const assets::AssetHandle<Kit>& kit,
            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

        /// @brief
        /// Purpose: Spawns an in-memory kit (the handle overload resolves through assets and
        /// lands here).
        Result<KitInstance> spawn(
            assets::AssetsState& assets,
            events::EventsState& events,
            const Kit& kit,
            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f));

        /// @brief
        /// Purpose: Creates a toy with identity and a default Transform.
        Toy spawn(std::string name);

        /// @brief
        /// Purpose: Literal-friendly toy spawn (a bare string literal would otherwise be
        /// ambiguous with the kit overloads).
        Toy spawn(const char* name)
        {
            return spawn(std::string(name));
        }

        /// @brief
        /// Purpose: Despawns every toy a kit instance spawned.
        void despawn(KitInstance instance);

        /// @brief
        /// Purpose: Despawns a toy; its children are orphaned (parent links cleared), not
        /// destroyed.
        void despawn(Toy toy);

        /// @brief
        /// Purpose: Sets the focus (typically player/camera position) and
        /// loads/unloads streamed kits by distance. Call once per frame.
        void stream(
            assets::AssetsState& assets,
            events::EventsState& events,
            jobs::JobsState& jobs,
            const Vec3& focus);

      private:
        void process_streaming(
            assets::AssetsState& assets,
            events::EventsState& events,
            jobs::JobsState& jobs);

      private:
        static constexpr float STREAM_LOAD_MARGIN = 5.0f;
        static constexpr float STREAM_UNLOAD_MARGIN = 15.0f; // > load margin: hysteresis band

      private:
        struct StreamedEntry
        {
            assets::AssetHandle<Kit> kit = {};
            Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
            Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
            float bounds_radius = 0.0f;
            bool is_loading = false;
            std::optional<KitInstance> instance = {};
        };

      private:
        Registry _registry;
        uint64 _next_kit_instance_id = 1;
        std::unordered_map<uint64, std::vector<ToyId>> _kit_instances;
        std::vector<StreamedEntry> _streamed_entries;
        Vec3 _stream_focus = Vec3(0.0f, 0.0f, 0.0f);
        bool _has_stream_focus = false;

        friend class Toy;
        // The kit serialization pair lives next to Kit (ecs/kit.h); load() needs the
        // instance bookkeeping and the asset system.
        friend TBX_API Result<KitInstance> load(
            Sandbox& sandbox,
            assets::AssetsState& assets,
            events::EventsState& events,
            const Kit& kit,
            const Vec3& root_position);
    };
}

#include "tbx/ecs/toy.inl"
