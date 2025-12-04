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
        return CreateToys(request.result);
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

        return Toy(*_dispatcher, invalid::toy_description);
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

    Toy Stage::add_toy(const std::string& name)
    {
        auto request = AddToyToStageRequest(_id, name);
        _dispatcher->send(request);
        return Toy(*_dispatcher, request.result);
    }

    void Stage::remove_toy(const Uuid& toy_id)
    {
        auto request = RemoveToyFromStageRequest(_id, toy_id);
        _dispatcher->send(request);
    }

    std::vector<Toy> Stage::CreateToys(const std::vector<ToyDescription>& descriptions) const
    {
        std::vector<Toy> toys = {};
        toys.reserve(descriptions.size());
        for (const auto& description : descriptions)
        {
            toys.emplace_back(*_dispatcher, description);
        }

        return toys;
    }
}
