#include "tbx/ecs/toy.h"

namespace tbx
{
    Toy::Toy(IMessageDispatcher& dispatcher, const Uuid& stage_id, const Uuid& id)
        : _dispatcher(&dispatcher)
        , _stage_id(stage_id)
        , _id(id)
    {
    }

    const Uuid& Toy::get_id() const
    {
        return _id;
    }

    const Uuid& Toy::get_stage_id() const
    {
        return _stage_id;
    }

    bool Toy::is_valid() const
    {
        if (!_id.is_valid())
        {
            return false;
        }

        auto request = IsToyValidRequest(_stage_id, _id);
        _dispatcher->send(request);
        return request.result;
    }

    std::vector<Block> Toy::get_full_view() const
    {
        auto request = ToyViewRequest(_stage_id, _id);
        _dispatcher->send(request);
        return request.result;
    }
}
