#include "tbx/ecs/stage.h"

namespace tbx
{
    Stage::Stage(IMessageDispatcher& dispatcher, const std::string& name, const Uuid& id)
        : _dispatcher(&dispatcher)
        , _name(name)
        , _id(id)
    {
    }

    const std::string& Stage::get_name() const
    {
        return _name;
    }

    const Uuid& Stage::get_id() const
    {
        return _id;
    }

    std::vector<Toy> Stage::get_full_view() const
    {
        auto request = StageViewRequest(_id);
        _dispatcher->send(request);
        return create_toys(request.result);
    }

    Toy Stage::get_toy(const Uuid& toy_id) const
    {
        auto toys = get_view<>();
        for (auto& toy : toys)
        {
            if (toy.get_id() == toy_id)
            {
                return toy;
            }
        }

        return Toy(*_dispatcher, _id, invalid::uuid);
    }

    bool Stage::has_toy(const Uuid& toy_id) const
    {
        auto toys = get_view<>();
        for (auto& toy : toys)
        {
            if (toy.get_id() == toy_id)
            {
                return true;
            }
        }

        return false;
    }

    Toy Stage::make_toy(const Uuid& toy_id)
    {
        auto request = MakeToyRequest(_id, toy_id);
        _dispatcher->send(request);
        return Toy(*_dispatcher, _id, request.result);
    }

    void Stage::remove_toy(const Uuid& toy_id)
    {
        auto request = RemoveToyFromStageRequest(_id, toy_id);
        _dispatcher->send(request);
    }

    std::vector<Toy> Stage::create_toys(const std::vector<Uuid>& toy_ids) const
    {
        std::vector<Toy> toys = {};
        toys.reserve(toy_ids.size());
        for (const auto& toy_id : toy_ids)
        {
            toys.emplace_back(*_dispatcher, _id, toy_id);
        }

        return toys;
    }
}
