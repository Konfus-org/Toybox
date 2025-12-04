#pragma once
#include "tbx/ecs/toy_description.h"
#include "tbx/messages/message.h"
#include <any>
#include <typeinfo>
#include <vector>

namespace tbx
{
    struct StageRequest
    {
        const Uuid stage_id;
    };

    struct StageViewRequest
        : public Request<std::vector<ToyDescription>>
        , StageRequest
    {
        StageViewRequest(const Uuid& id, const std::vector<const std::type_info*>& filter = {})
            : StageRequest(id)
            , block_type_filter(filter)
        {
        }

        std::vector<const std::type_info*> block_type_filter = {};
    };

    struct AddToyToStageRequest
        : public Request<ToyDescription>
        , StageRequest
    {
        AddToyToStageRequest(const Uuid& stage_id, const std::string& name)
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
        RemoveToyFromStageRequest(const Uuid& stage_id, const Uuid& toy_id)
            : StageRequest(stage_id)
            , toy_id(toy_id)
        {
        }

        const Uuid toy_id;
    };

    struct ToyRequest
    {
        const Uuid toy_id;
    };

    struct ToyViewRequest
        : public Request<std::vector<std::any>>
        , ToyRequest
    {
        ToyViewRequest(const Uuid& toy_id, const std::vector<const std::type_info*>& filter = {})
            : ToyRequest(toy_id)
            , block_type_filter(filter)
        {
        }

        std::vector<const std::type_info*> block_type_filter = {};
    };

    struct IsToyValidRequest
        : public Request<bool>
        , public ToyRequest
    {
        IsToyValidRequest(const Uuid& toy_id)
            : ToyRequest(toy_id)
        {
        }
    };

    struct GetToyBlockRequest
        : public Request<std::any>
        , public ToyRequest
    {
        // Purpose: Retrieves a block instance of the requested type from a toy.
        // Ownership: The block is owned by the toy; callers receive a reference wrapped in the
        // returned std::any value.
        // Thread-safety: Not thread-safe. Calls are expected on the toy-owning thread.
        GetToyBlockRequest(const Uuid& toy_id, const std::type_info& requested_type)
            : ToyRequest(toy_id)
            , block_type(requested_type)
        {
        }

        const std::type_info& block_type;
    };

    struct AddBlockToToyRequest
        : public Request<std::any>
        , public ToyRequest
    {
        AddBlockToToyRequest(const Uuid& toy_id, const std::any& block)
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
        RemoveBlockFromToyRequest(const Uuid& toy_id, const std::type_info& block)
            : ToyRequest(toy_id)
            , block_type(block)
        {
        }

        const std::type_info& block_type;
    };
}
