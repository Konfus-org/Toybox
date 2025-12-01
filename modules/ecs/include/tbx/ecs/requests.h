#pragma once
#include "tbx/messages/message.h"
#include <any>
#include <vector>

namespace tbx
{
    struct Toy;

    struct StageRequest
    {
        const uuid stage_id;
    };

    struct StageViewRequest
        : public Request<std::vector<Toy>>
        , StageRequest
    {
        StageViewRequest(const uuid& id, const std::vector<type_info>& filter = {})
            : StageRequest(id)
            , block_type_filter(filter)
        {
        }

        std::vector<std::type_info> block_type_filter = {};
    };

    struct AddToyToStageRequest
        : public Request<Toy>
        , StageRequest
    {
        AddToyToStageRequest(const uuid& stage_id, const std::string& name)
            : StageRequest(stage_id)
            , toy_name(name)
        {
        }

        const std::string toy_name;
    };

    struct RemoveToyFromStageRequest
        : public Request<void>
        , StageRequest
    {
        RemoveToyFromStageRequest(const uuid& stage_id, const uuid& toy_id)
            : StageRequest(stage_id)
            , toy_id(toy_id)
        {
        }

        const uuid toy_id;
    };

    struct ToyRequest
    {
        const uuid toy_id;
    };

    struct ToyViewRequest
        : public Request<std::vector<std::any>>
        , ToyRequest
    {
        ToyViewRequest(const uuid& toy_id, const std::vector<type_info>& filter = {})
            : ToyRequest(toy_id)
            , block_type_filter(filter)
        {
        }

        std::vector<std::type_info> block_type_filter = {};
    };

    struct IsToyValidRequest
        : public Request<bool>
        , public ToyRequest
    {
        IsToyValidRequest(const uuid& toy_id)
            : ToyRequest(toy_id)
        {
        }
    };

    struct AddBlockToToyRequest
        : public Request<std::any>
        , public ToyRequest
    {
        AddBlockToToyRequest(const uuid& toy_id, const std::any& block)
            : ToyRequest(toy_id)
            , block_data(block)
        {
        }

        const std::any block_data;
    };

    struct RemoveBlockFromToyRequest
        : public Request<bool>
        , public ToyRequest
    {
        RemoveBlockFromToyRequest(const uuid& toy_id, const std::type_info& block)
            : ToyRequest(toy_id)
            , block_type(block)
        {
        }

        const std::type_info& block_type;
    };
}
