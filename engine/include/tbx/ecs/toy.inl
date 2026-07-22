#pragma once
// Toy template/inline bodies that need only the Registry (the container-taking constructor
// lives in container.h, where ToyContainer is complete).

namespace tbx
{
    inline Toy::Toy(Registry& registry, const ToyId id)
        : _registry(registry)
        , _id(id)
    {
    }

    template <typename TBlock>
    TBlock& Toy::get_block()
    {
        return _registry->get().get_or_emplace<TBlock>(_id);
    }

    template <typename TBlock>
    TBlock* Toy::try_block() const
    {
        return _registry ? _registry->get().try_get<TBlock>(_id) : nullptr;
    }

    template <typename TBlock>
    bool Toy::has_block() const
    {
        return _registry && _registry->get().all_of<TBlock>(_id);
    }

    template <typename TBlock>
    Toy& Toy::remove_block()
    {
        _registry->get().remove<TBlock>(_id);
        return *this;
    }

    template <typename TBlock>
    Toy& Toy::with(TBlock block)
    {
        _registry->get().emplace_or_replace<TBlock>(_id, std::move(block));
        return *this;
    }
}
