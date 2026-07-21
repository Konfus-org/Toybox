#pragma once
// Template bodies for block registration — included by block.h.

namespace tbx
{
    template <typename TBlock>
    reflection::TypeRegistration<TBlock> register_block(std::string name)
    {
        auto registration = reflection::describe<TBlock>(std::move(name));
        auto operations = BlockOperations {};
        operations.add_default = [](Registry& registry, const ToyId entity) -> std::byte*
        {
            return reinterpret_cast<std::byte*>(&registry.get_or_emplace<TBlock>(entity));
        };
        operations.get = [](Registry& registry, const ToyId entity) -> std::byte*
        {
            return reinterpret_cast<std::byte*>(registry.try_get<TBlock>(entity));
        };
        operations.has = [](Registry& registry, const ToyId entity)
        {
            return registry.all_of<TBlock>(entity);
        };
        operations.remove = [](Registry& registry, const ToyId entity)
        {
            registry.remove<TBlock>(entity);
        };
        get_block_registry().add(reflection::TypeSlot<TBlock>::hash, operations);
        return registration;
    }
}
