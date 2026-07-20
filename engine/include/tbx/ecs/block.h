#pragma once
#include "tbx/core/typedefs.h"
#include "tbx/reflect/type_info.h"
#include <entt/entt.hpp>
#include <optional>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: Type-erased ECS accessors for one registered block type, so kit save/load and
    /// the future editor can touch any toy's blocks through the reflection schema alone.
    struct BlockOperations
    {
        void* (*add_default)(entt::registry&, entt::entity) = nullptr;
        void* (*get)(entt::registry&, entt::entity) = nullptr;
        bool (*has)(entt::registry&, entt::entity) = nullptr;
        void (*remove)(entt::registry&, entt::entity) = nullptr;
    };

    /// @brief
    /// Purpose: The block-operations table keyed by type name hash; filled by register_block.
    /// @details
    /// Ownership: Process-lifetime, like the type registry. Thread Safety: Register on the main
    /// thread during startup; lookups are reads afterwards.
    class BlockRegistry final
    {
      public:
        /// @brief
        /// Purpose: Stores the operations for a block type under its name hash.
        void add(uint64 name_hash, BlockOperations operations);

        /// @brief
        /// Purpose: Every registered block type's name hash, for save-time enumeration.
        std::vector<uint64> get_all_hashes() const;

        /// @brief
        /// Purpose: Looks up operations by type name hash; empty when the type is not a block.
        std::optional<BlockOperations> find(uint64 name_hash) const;

      private:
        std::unordered_map<uint64, BlockOperations> _operations;
    };

    /// @brief
    /// Purpose: The process-wide block-operations registry.
    BlockRegistry& get_block_registry();

    /// @brief
    /// Purpose: Registers a type as a Block (attachable to toys): reflection via register_type
    /// PLUS the ECS accessors kits and the editor need. Chain .version()/.field() off the
    /// result exactly like register_type.
    template <typename TBlock>
    TypeRegistration<TBlock> register_block(std::string name)
    {
        auto registration = register_type<TBlock>(std::move(name));
        auto operations = BlockOperations {};
        operations.add_default = [](entt::registry& registry, entt::entity entity) -> void*
        {
            return &registry.get_or_emplace<TBlock>(entity);
        };
        operations.get = [](entt::registry& registry, entt::entity entity) -> void*
        {
            return registry.try_get<TBlock>(entity);
        };
        operations.has = [](entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<TBlock>(entity);
        };
        operations.remove = [](entt::registry& registry, entt::entity entity)
        {
            registry.remove<TBlock>(entity);
        };
        get_block_registry().add(TypeSlot<TBlock>::hash, operations);
        return registration;
    }
}
