#pragma once
#include "tbx/api.h"
#include "tbx/math/transform.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tbx
{
    class ToyContainer; // a Sandbox or a Kit; both own a hidden registry of toys

    /// @brief
    /// Purpose: The one engine-internal record every toy carries: identity (uuid + name),
    /// enabled state, its parent, and its stickers. One component holding all the standard
    /// per-toy data the engine tracks itself.
    struct TBX_DLL_EXPORT ToyInfo
    {
        Uuid uuid = {};
        std::string name = {};
        bool is_enabled = true;
        ToyId parent = NULL_TOY;
        std::vector<std::string> stickers = {};
    };

    /// @brief
    /// Purpose: Fluent handle to one toy: container.add("Grunt").with(Transform
    /// {...}).with(Health {...}).add("enemy"). A toy is the handle you query and mutate a
    /// toy through — the same handle works whether the toy lives in a Sandbox (the world) or a
    /// Kit (a bundle). The owning container's registry stays hidden; everything goes through
    /// the handle.
    /// @details
    /// Ownership: A view — the container owns the toy. Thread Safety: Main thread only
    /// (structural mutation rule).
    class TBX_DLL_EXPORT Toy final
    {
      public:
        Toy() = default;
        Toy(Registry& registry, ToyId id);
        // A toy in a container's registry (Sandbox or Kit) — the everyday constructor.
        Toy(ToyContainer& container, ToyId id);

      public:
        /// @brief
        /// Purpose: Returns the block of this type, adding a default-constructed one if absent.
        template <typename TBlock>
        TBlock& add();

        /// @brief
        /// Purpose: The block of this type if attached, else nullptr (never inserts).
        template <typename TBlock>
        TBlock* get() const;

        /// @brief
        /// Purpose: The toy's per-session registry id.
        ToyId get_id() const
        {
            return _id;
        }

        /// @brief
        /// Purpose: The toy's display name.
        const std::string& get_name() const;

        /// @brief
        /// Purpose: The toy's runtime uuid (fresh per instantiation; serialized by kits).
        Uuid get_uuid() const;

        /// @brief
        /// Purpose: The toy's local transform block (position/rotation/scale) — the common
        /// case, so it lives right on the handle. Adds a default one if absent.
        Transform& get_transform();

        /// @brief
        /// Purpose: The toy's world transform — its local transform composed up the parent
        /// chain (what the renderer draws with).
        Mat4 get_world_transform() const;

        /// @brief
        /// Purpose: The toy's parent, when it has one.
        std::optional<Toy> get_parent() const;

        /// @brief
        /// Purpose: The toy's direct children (toys parented to it).
        std::vector<Toy> get_children() const;

        /// @brief
        /// Purpose: True while the toy exists in its registry.
        bool is_alive() const;

        /// @brief
        /// Purpose: True when the toy participates in rendering, scripting, and physics.
        bool is_enabled() const;

        /// @brief
        /// Purpose: True when this block type is attached.
        template <typename TBlock>
        bool has() const;

        /// @brief
        /// Purpose: True when the sticker is on this toy.
        bool has(std::string_view name) const;

        /// @brief
        /// Purpose: Detaches the block of this type (no-op when absent). Fluent: returns the toy
        /// for chaining.
        template <typename TBlock>
        Toy& remove();

        /// @brief
        /// Purpose: Peels a sticker off (no-op when absent). Fluent: returns the toy for chaining.
        Toy& remove(std::string_view name);

        /// @brief
        /// Purpose: Turns the toy on or off for rendering, scripting, and physics (persisted
        /// by kits). Fluent: returns the toy for chaining.
        Toy& set_enabled(bool is_enabled);

        /// @brief
        /// Purpose: Renames the toy. Fluent: returns the toy for chaining.
        Toy& set_name(std::string name);

        /// @brief
        /// Purpose: Reparents the toy (pass a default Toy to clear the parent). Fluent:
        /// returns the toy for chaining.
        Toy& set_parent(Toy parent);

        /// @brief
        /// Purpose: Fluent: slaps a sticker on and returns the toy for chaining.
        Toy& add(std::string name);

        /// @brief
        /// Purpose: Spawns a sibling toy (same container, same parent) and returns it — so a spawn
        /// chain keeps flowing: spawn("A").spawn("B"), or spawn("Grunt").with(Transform{}).spawn("Bullet").
        Toy spawn(std::string name);

        /// @brief
        /// Purpose: Fluent: attaches (or replaces) a block and returns the toy for chaining.
        template <typename TBlock>
        Toy& with(TBlock block);

        //// REFLECTED-BY-NAME BLOCK ACCESS (scripting/tooling) ////

        /// @brief
        /// Purpose: Raw bytes of the reflected block with this type-name hash, or nullptr —
        /// scripting reads/writes fields through the reflection FieldInfo over these bytes.
        std::byte* get_block_bytes(uint64 type_hash) const;

        /// @brief
        /// Purpose: Attaches (default-constructs) the reflected block with this type-name hash
        /// and returns its bytes, or nullptr if the type is unknown.
        std::byte* add_block_bytes(uint64 type_hash);

        /// @brief
        /// Purpose: True when the reflected block with this type-name hash is attached.
        bool has_block_named(uint64 type_hash) const;

        /// @brief
        /// Purpose: Detaches the reflected block with this type-name hash (no-op when absent).
        void remove_block_named(uint64 type_hash);

      private:
        /// @brief
        /// Purpose: The info record every accessor reads/writes.
        ToyInfo& get_info() const;

      private:
        std::optional<std::reference_wrapper<Registry>> _registry = {};
        ToyId _id = NULL_TOY;
    };
}

#include "tbx/ecs/toy.inl"
