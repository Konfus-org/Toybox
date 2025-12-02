#pragma once
#include "tbx/common/uuid.h"
#include "tbx/ecs/requests.h"
#include "tbx/ecs/toy.h"
#include "tbx/messages/dispatcher.h"
#include <typeinfo>

namespace tbx
{
    struct Stage
    {
        std::string name = "Default Stage";
        Uuid id = Uuid::generate();
    };

    std::vector<Toy> full_view(const Stage& stage)
    {
        auto* dispatcher = get_global_dispatcher();
        auto request = StageViewRequest(stage.id);
        dispatcher->send(request);
        return request.result;
    }

    template <typename... Ts>
    std::vector<Toy> view(const Stage& stage)
    {
        auto* dispatcher = get_global_dispatcher();
        std::vector<const std::type_info*> filters = { &typeid(Ts)... };
        auto request = StageViewRequest(stage.id, filters);
        dispatcher->send(request);
        return request.result;
    }

    Toy get_toy(const Stage& stage, const Uuid& toy_id)
    {
        auto toys = view(stage);
        for (auto& t : toys)
        {
            if (t.id == toy_id)
            {
                return t;
            }
        }
        return invalid::toy;
    }

    Toy add_toy(Stage& stage, const std::string& name)
    {
        auto* dispatcher = get_global_dispatcher();
        auto request = AddToyToStageRequest(stage.id, name);
        dispatcher->send(request);
        return request.result;
    }

    void remove_toy(Stage& stage, const Uuid& toy_id)
    {
        auto* dispatcher = get_global_dispatcher();
        auto request = RemoveToyFromStageRequest(stage.id, toy_id);
        dispatcher->send(request);
    }
}
