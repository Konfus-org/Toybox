#pragma once
#include "tbx/ecs/requests.h"
#include "tbx/ecs/toy_types.h"
#include "tbx/messages/dispatcher.h"
#include <typeinfo>

namespace tbx
{
    inline bool is_valid(const Toy& t)
    {
        if (!t.id.is_valid())
            return false;

        auto* dispatcher = current_dispatcher();
        auto request = IsToyValidRequest(t.id);
        dispatcher->send(request);
        return request.result;
    }

    inline std::vector<Block> full_view(const Toy& t)
    {
        auto* dispatcher = current_dispatcher();
        auto request = ToyViewRequest(t.id);
        dispatcher->send(request);
        return request.result;
    }

    template <typename... Ts>
    std::vector<Block> view(const Toy& t)
    {
        auto* dispatcher = current_dispatcher();
        std::vector<const std::type_info*> filters = { &typeid(Ts)... };
        auto request = ToyViewRequest(t.id, filters);
        dispatcher->send(request);
        return request.result;
    }

    template <typename T>
    T& get_block(const Toy& t)
    {
        auto blocks = view<T>(t);
        for (auto& b : blocks)
        {
            if (b.type() == typeid(T))
            {
                return std::any_cast<T&>(b);
            }
        }
        return invalid::block;
    }

    template <typename T>
    bool has_block(const Toy& t)
    {
        auto blocks = view<T>(t);
        for (auto& b : blocks)
        {
            if (b.type() == typeid(T))
            {
                return true;
            }
        }
        return false;
    }

    template <typename T>
    T& add_block(const Toy& t)
    {
        auto request = AddBlockToToyRequest(t.id, typeid(T));
        auto* dispatcher = current_dispatcher();
        dispatcher->send(request);
        return std::any_cast<T&>(request.result);
    }

    template <typename T>
    void remove_block(const Toy& t)
    {
        auto request = RemoveBlockFromToyRequest(t.id, typeid(T));
        auto* dispatcher = current_dispatcher();
        dispatcher->send(request);
    }
}
