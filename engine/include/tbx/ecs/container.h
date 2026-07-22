#pragma once
#include "tbx/api.h"
#include "tbx/ecs/registry.h"
#include "tbx/ecs/toy.h"
#include "tbx/utils/result.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: The shape both a Sandbox (the world) and a Kit (a bundle) share: a container
    /// of toys arranged by parent/child, queried and mutated ONLY through wrapper methods —
    /// the underlying registry is private, so nothing outside the container names the ECS
    /// library. Systems iterate with each<Components...>(); everything else goes through Toy.
    /// @details
    /// The registry is shared_ptr so a Kit stays copyable (it rides in the asset cache and is
    /// shallow-copied during streaming); a resident kit is only ever read, never mutated after
    /// decode, so sharing is safe. A Sandbox deletes copy to keep its world unique.
    class TBX_API ToyContainer
    {
      public:
        ToyContainer() = default;
        virtual ~ToyContainer() = default;
        ToyContainer(ToyContainer&&) = default;
        ToyContainer& operator=(ToyContainer&&) = default;
        ToyContainer(const ToyContainer&) = default;
        ToyContainer& operator=(const ToyContainer&) = default;

      public:
        /// @brief
        /// Purpose: Creates a toy with identity and a default Transform.
        Toy spawn(std::string name);

        /// @brief
        /// Purpose: Literal-friendly toy spawn.
        Toy spawn(const char* name)
        {
            return spawn(std::string(name));
        }

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
        /// Purpose: Number of live toys.
        size get_toy_count() const;

        /// @brief
        /// Purpose: Every toy in the container, as handles.
        std::vector<Toy> get_toys() const;

        /// @brief
        /// Purpose: Despawns a toy; its children are orphaned (parent cleared), not destroyed.
        void despawn(Toy toy);

        /// @brief
        /// Purpose: Despawns a toy and its whole subtree.
        void despawn_subtree(Toy root);

        /// @brief
        /// Purpose: Despawns every descendant of a toy but keeps the toy itself (how a
        /// streamed kit instance collapses).
        void despawn_children(Toy root);

        /// @brief
        /// Purpose: Removes every toy — the container is empty afterward.
        void clear();

        /// @brief
        /// Purpose: The one iteration seam for systems: invokes fn(Toy, Components&...) for
        /// every toy carrying all of Components. Access other components off the Toy.
        template <typename... TComponents, typename TFn>
        void each(TFn&& fn)
        {
            for (auto&& row : _registry->view<TComponents...>().each())
                std::apply(
                    [&](const ToyId id, TComponents&... components)
                    { fn(Toy(*this, id), components...); },
                    row);
        }

        /// @brief
        /// Purpose: Copies every toy of `source` into this container as children of `parent`
        /// (fresh uuids, in-source parents remapped) — how a kit instantiates into a world.
        /// Returns the toys it created, in source order.
        std::vector<Toy> copy_toys_from(const ToyContainer& source, Toy parent);

      private:
        std::shared_ptr<Registry> _registry = std::make_shared<Registry>();

        // Toy reads _registry to bind its handle; the floating serializers walk it. Nothing
        // else names it.
        friend class Toy;
        friend Json serialize_toys(const ToyContainer&);
        friend Result<void> deserialize_toys(ToyContainer&, const Json&);
    };

    // The container-taking Toy constructor — defined here where ToyContainer is complete and
    // Toy is its friend.
    inline Toy::Toy(ToyContainer& container, const ToyId id)
        : _registry(*container._registry)
        , _id(id)
    {
    }

    /// @brief
    /// Purpose: The container's toys as the "toys" JSON array — the shared serialize half Kit
    /// and Sandbox both use (a kit instance's regenerated children are skipped). A floating
    /// function over the toy graph; a kit/level serializer wraps it with its own fields
    /// (bounds, ...) under its own extension.
    TBX_API Json serialize_toys(const ToyContainer& container);

    /// @brief
    /// Purpose: Spawns a "toys" JSON array into a container, linking parents by uuid — the
    /// shared deserialize half.
    TBX_API Result<void> deserialize_toys(ToyContainer& container, const Json& toys);
}
