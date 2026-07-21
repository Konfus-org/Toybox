#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include "tbx/ecs/registry.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tbx
{
    class Sandbox; // defined in sandbox.h, which completes Toy's inline methods

    /// @brief
    /// Purpose: Engine-internal identity every toy carries (runtime uuid + display name).
    struct TBX_API ToyHandle
    {
        Uuid uuid = {};
        std::string name = {};
        bool is_enabled = true;
    };

    /// @brief
    /// Purpose: Engine-internal sticker names slapped on a toy; compared by name hash.
    struct TBX_API StickerSet
    {
        std::vector<std::string> names = {};
    };

    /// @brief
    /// Purpose: Engine-internal parent link forming the transform hierarchy.
    struct TBX_API ParentLink
    {
        ToyId parent = NULL_TOY;
    };

    /// @brief
    /// Purpose: Fluent handle to one toy: sandbox.spawn("Grunt").with(Transform
    /// {...}).with(Health {...}).sticker("enemy").
    /// @details
    /// Ownership: A view — the Sandbox owns the toy. Thread Safety: Main thread only
    /// (structural mutation rule).
    class TBX_API Toy final
    {
      public:
        Toy() = default;

        Toy(Sandbox& sandbox, ToyId id);

      public:
        /// @brief
        /// Purpose: Returns the block of this type, adding a default-constructed one if absent.
        template <typename TBlock>
        TBlock& get_block();

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
        /// Purpose: The sandbox that owns this toy.
        Sandbox& get_sandbox() const
        {
            return _sandbox->get();
        }

        /// @brief
        /// Purpose: The toy's runtime uuid (fresh per instantiation; serialized by kits).
        Uuid get_uuid() const;

        /// @brief
        /// Purpose: True while the toy exists in its sandbox.
        bool is_alive() const;

        /// @brief
        /// Purpose: True when the toy participates in rendering, scripting, and physics.
        bool is_enabled() const;

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
        /// Purpose: Turns the toy on or off for rendering, scripting, and physics (persisted
        /// by kits).
        void set_enabled(bool is_enabled);

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
}
