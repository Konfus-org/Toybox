#pragma once
// Toy method bodies — included by sandbox.h once Sandbox is complete.

namespace tbx
{
    inline Toy::Toy(Sandbox& sandbox, const ToyId id)
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
