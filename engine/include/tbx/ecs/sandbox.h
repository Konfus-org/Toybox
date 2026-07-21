#pragma once
#include "tbx/core/math.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/core/uuid.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/toy.h"
#include "tbx/ecs/builtin_blocks.h"
#include "tbx/jobs/jobs.h"
#include "tbx/reflect/json_walker.h"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Turns a kit reference string into its JSON body. The default engine resolver
    /// reads files; tests inject in-memory maps; the asset system replaces it later.
    using KitResolver = std::function<Result<Json>(const std::string& reference)>;

    /// @brief
    /// Purpose: How a sandbox-level kit entry loads — set ONLY at the sandbox level; nested kit
    /// references always load with whatever pulls them in.
    enum class KitMode : uint8
    {
        ALWAYS,
        STREAMED
    };

    /// @brief
    /// Purpose: Handle to one instantiated kit; despawn(instance) removes exactly the toys it
    /// spawned (including toys from nested kit references).
    struct KitInstance
    {
        uint64 id = 0;
    };

    /// @brief
    /// Purpose: THE world container: owns every toy and streams sandbox-level kit entries by
    /// distance. Serialization lives on tbx::save / tbx::load (save_load.h), not here.
    /// @details
    /// Ownership: Owns the registry and all kit bookkeeping. Thread Safety: Structural
    /// mutation on the main thread only; streaming resolves kit bodies on workers and splices
    /// on the main thread via Jobs.
    class Sandbox final
    {
      public:
        /// @brief
        /// Purpose: What a sandbox opens: the kit entries ({"kits": [{reference, mode,
        /// position}]}) plus the resolver that turns references into kit bodies.
        struct Layout
        {
            Json kits = {};
            KitResolver resolver = {};
        };

      public:
        explicit Sandbox(Jobs& jobs);

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
        /// Purpose: A toy's parent, when it has one.
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
        Mat4 get_world_matrix(Toy toy);

        /// @brief
        /// Purpose: Reparents a toy (pass a default Toy to clear the parent).
        void set_parent(Toy child, Toy parent);

        /// @brief
        /// Purpose: Opens a layout: ALWAYS entries load immediately; STREAMED entries
        /// load/unload by distance to the streaming focus (see stream()).
        Result<void> open(Layout layout);

        /// @brief
        /// Purpose: Unloads everything: every kit instance, every toy, and all streaming
        /// state. The sandbox is empty and ready to open another layout.
        void close();

        /// @brief
        /// Purpose: Spawns a kit body: instantiates its toys (nested kit references resolve
        /// recursively; cycles are errors; position offsets parentless toys) and returns the
        /// instance handle for despawn().
        Result<KitInstance> spawn(
            const Json& kit,
            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f),
            const KitResolver& resolver = {});

        /// @brief
        /// Purpose: Creates a toy with identity and a default Transform.
        Toy spawn(std::string name);

        /// @brief
        /// Purpose: Literal-friendly toy spawn (a bare string literal would otherwise be
        /// ambiguous between std::string and a Json kit body).
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
        void stream(const Vec3& focus);

      private:
        void process_streaming();

      private:
        // Serialization internals — the public surface is tbx::save / tbx::load (save_load.h).
        Json save_kit(std::span<const Toy> toys);
        Result<KitInstance> load_kit(
            const Json& kit,
            const Vec3& root_position,
            const KitResolver& resolver);
        Result<KitInstance> load_kit_body(
            const Json& kit,
            const Vec3& root_position,
            const KitResolver& resolver,
            std::vector<uint64>& reference_stack,
            std::vector<ToyId>& spawned);

      private:
        static constexpr float STREAM_LOAD_MARGIN = 5.0f;
        static constexpr float STREAM_UNLOAD_MARGIN = 15.0f; // > load margin: hysteresis band

      private:
        struct StreamedEntry
        {
            std::string reference = {};
            Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
            Vec3 bounds_center = Vec3(0.0f, 0.0f, 0.0f);
            float bounds_radius = 0.0f;
            bool is_loading = false;
            std::optional<KitInstance> instance = {};
        };

      private:
        std::reference_wrapper<Jobs> _jobs;
        Registry _registry;
        uint64 _next_kit_instance_id = 1;
        std::unordered_map<uint64, std::vector<ToyId>> _kit_instances;
        std::vector<StreamedEntry> _streamed_entries;
        KitResolver _layout_resolver = {};
        Vec3 _stream_focus = Vec3(0.0f, 0.0f, 0.0f);
        bool _has_stream_focus = false;

        friend class Toy;
        friend Json save(Sandbox& sandbox, std::span<const Toy> toys);
    };
}

#include "tbx/ecs/toy.inl"
