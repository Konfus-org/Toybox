#pragma once
#include "tbx/common/uuid.h"
#include "tbx/ecs/requests.h"
#include <string>
#include <vector>

namespace tbx
{
    using Sticker = std::string;
    using Block = std::any;

    struct Toy
    {
        std::string name = "";
        std::vector<Sticker> stickers = {};
        uuid parent = invalid::uuid;
        uuid id = uuid::generate();
    };

    namespace invalid
    {
        inline Block block = std::any();
        inline Toy toy = Toy("INVALID", {}, invalid::uuid, invalid::uuid);
    }

    bool is_valid(const Toy& t)
    {
        if (!t.id.is_valid())
            return false;

        auto* dispatcher = current_dispatcher();
        auto request = IsToyValidRequest(t.id);
        dispatcher->send(request);
        return request.result;
    }

    std::vector<Block> full_view(const Toy& t)
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
        auto request = ToyViewRequest(t.id, {typeid(Ts)...});
        dispatcher->send(request);
        return request.result;
    }

    template <typename T>
    T& get_block(const Toy& t)
    {
        auto blocks = view(t);
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
        auto blocks = view(t);
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
