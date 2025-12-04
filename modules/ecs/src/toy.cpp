#include "tbx/ecs/toy.h"

namespace tbx
{
    Toy::Toy(IMessageDispatcher& dispatcher, const ToyDescription& description)
        : _dispatcher(&dispatcher)
        , _description(description)
    {
    }

    Toy::Toy(
        IMessageDispatcher& dispatcher,
        const std::string& name,
        const std::vector<Sticker>& stickers,
        const Uuid& parent,
        const Uuid& id)
        : _dispatcher(&dispatcher)
        , _description(name, stickers, parent, id)
    {
    }

    const std::string& Toy::get_name() const
    {
        return _description.name;
    }

    const std::vector<Sticker>& Toy::get_stickers() const
    {
        return _description.stickers;
    }

    const Uuid& Toy::get_parent() const
    {
        return _description.parent;
    }

    const Uuid& Toy::get_id() const
    {
        return _description.id;
    }

    bool Toy::is_valid() const
    {
        if (!_description.id.is_valid())
        {
            return false;
        }

        auto request = IsToyValidRequest(_description.id);
        _dispatcher->send(request);
        return request.result;
    }

    std::vector<Block> Toy::get_full_view() const
    {
        auto request = ToyViewRequest(_description.id);
        _dispatcher->send(request);
        return request.result;
    }
}
