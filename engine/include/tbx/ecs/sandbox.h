#pragma once
#include "tbx/core/math.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/core/uuid.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/transform.h"
#include "tbx/jobs/jobs.h"
#include "tbx/reflect/json_walker.h"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tbx
{
    class Sandbox;

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
    /// Purpose: Handle to one instantiated kit; unload_kit() despawns exactly the toys this
    /// instance spawned (including toys from nested kit references).
    struct KitInstance
    {
        uint64 id = 0;
    };

    /// @brief
    /// Purpose: Engine-internal identity every toy carries (runtime uuid + display name).
    struct ToyHandle
    {
        Uuid uuid = {};
        std::string name = {};
    };

    /// @brief
    /// Purpose: Engine-internal sticker names slapped on a toy; compared by name hash.
    struct StickerSet
    {
        std::vector<std::string> names = {};
    };

    /// @brief
    /// Purpose: Engine-internal parent link forming the transform hierarchy.
    struct ParentLink
    {
        ToyId parent = NULL_TOY;
    };

    /// @brief
    /// Purpose: Fluent handle to one toy: sandbox.spawn("Grunt").with(Transform
    /// {...}).with(Health {...}).sticker("enemy").
    /// @details
    /// Ownership: A view — the Sandbox owns the toy. Thread Safety: Main thread only
    /// (structural mutation rule).
    class Toy final
    {
      public:
        Toy() = default;

        Toy(Sandbox& sandbox, ToyId id);

      public:
        /// @brief
        /// Purpose: The toy's per-session registry id.
        ToyId get_id() const
        {
            return _id;
        }

        /// @brief
        /// Purpose: Returns the block of this type, adding a default-constructed one if absent.
        template <typename TBlock>
        TBlock& get_block();

        /// @brief
        /// Purpose: The toy's display name.
        const std::string& get_name() const;

        /// @brief
        /// Purpose: The toy's runtime uuid (fresh per instantiation; serialized by kits).
        Uuid get_uuid() const;

        /// @brief
        /// Purpose: True while the toy exists in its sandbox.
        bool is_alive() const;

        /// @brief
        /// Purpose: True when this block type is attached.
        template <typename TBlock>
        bool has_block() const;

        /// @brief
        /// Purpose: True when the sticker is on this toy.
        bool has_sticker(std::string_view name) const;

        /// @brief
        /// Purpose: Detaches the block of this type (no-op when absent).
        template <typename TBlock>
        void remove_block();

        /// @brief
        /// Purpose: Peels a sticker off (no-op when absent).
        void remove_sticker(std::string_view name);

        /// @brief
        /// Purpose: Renames the toy.
        void set_name(std::string name);

        /// @brief
        /// Purpose: Fluent: slaps a sticker on and returns the toy for chaining.
        Toy& sticker(std::string name);

        /// @brief
        /// Purpose: Fluent: attaches (or replaces) a block and returns the toy for chaining.
        template <typename TBlock>
        Toy& with(TBlock block);

      private:
        std::optional<std::reference_wrapper<Sandbox>> _sandbox = {};
        ToyId _id = NULL_TOY;
    };

    /// @brief
    /// Purpose: THE world container: owns every toy, loads kits (recursive sets of toys), and
    /// streams sandbox-level kit entries by distance.
    /// @details
    /// Ownership: Owns the entt registry and all kit bookkeeping. Thread Safety: Structural
    /// mutation on the main thread only; streaming resolves kit bodies on workers and splices
    /// on the main thread via Jobs.
    class Sandbox final
    {
      public:
        explicit Sandbox(Jobs& jobs);

      public:
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

      public:
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
        /// Purpose: Finds a toy by runtime uuid.
        std::optional<Toy> find_toy(const Uuid& uuid);

        /// @brief
        /// Purpose: Finds the first toy with the given name.
        std::optional<Toy> find_toy(std::string_view name);

        /// @brief
        /// Purpose: Loads a sandbox layout: {"kits": [{reference, mode, position}]}. ALWAYS
        /// entries load immediately; STREAMED entries load/unload by distance to the streaming
        /// focus (see stream_from), using bounds stored in each kit at save time.
        Result<void> load_layout(const Json& layout, const KitResolver& resolver);

        /// @brief
        /// Purpose: Instantiates a kit body into the sandbox (main thread). Nested kit
        /// references resolve recursively through the resolver; reference cycles are load
        /// errors. Root position offsets every parentless toy.
        Result<KitInstance> load_kit(
            const Json& kit,
            const Vec3& root_position = Vec3(0.0f, 0.0f, 0.0f),
            const KitResolver& resolver = {});

        /// @brief
        /// Purpose: Despawns every toy a kit instance spawned.
        void unload_kit(KitInstance instance);

        /// @brief
        /// Purpose: Serializes toys (blocks, stickers, parent links) plus computed bounds into
        /// a kit body.
        Json save_kit(std::span<const Toy> toys);

        /// @brief
        /// Purpose: Creates a toy with identity and a default Transform.
        Toy spawn(std::string name);

        /// @brief
        /// Purpose: Despawns a toy; its children are orphaned (parent links cleared), not
        /// destroyed.
        void despawn(Toy toy);

        /// @brief
        /// Purpose: Reparents a toy (pass a default Toy to clear the parent).
        void set_parent(Toy child, Toy parent);

        /// @brief
        /// Purpose: Drives streaming; called once per frame by Engine::update().
        void process_streaming();

        /// @brief
        /// Purpose: Sets the streaming focus (typically player/camera position, every frame).
        void stream_from(const Vec3& focus);

      private:
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
    };

    //// TOY INLINE DEFINITIONS (need the Sandbox definition above) ////

    inline Toy::Toy(Sandbox& sandbox, ToyId id)
        : _sandbox(sandbox)
        , _id(id)
    {
    }

    template <typename TBlock>
    TBlock& Toy::get_block()
    {
        return _sandbox->get()._registry.get_or_emplace<TBlock>(_id);
    }

    template <typename TBlock>
    bool Toy::has_block() const
    {
        return _sandbox && _sandbox->get()._registry.all_of<TBlock>(_id);
    }

    template <typename TBlock>
    void Toy::remove_block()
    {
        _sandbox->get()._registry.remove<TBlock>(_id);
    }

    template <typename TBlock>
    Toy& Toy::with(TBlock block)
    {
        _sandbox->get()._registry.emplace_or_replace<TBlock>(_id, std::move(block));
        return *this;
    }
}
