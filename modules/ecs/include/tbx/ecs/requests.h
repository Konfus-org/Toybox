#pragma once
#include "tbx/common/uuid.h"
#include "tbx/ecs/block.h"
#include "tbx/messages/message.h"
#include <typeinfo>
#include <vector>

namespace tbx
{
    struct TypedBlockMessageBase
    {
        // Purpose: Allows typed ECS block messages to expose their payload type at runtime.
        // Ownership: Non-owning. Implementations only expose type metadata and hold no resources.
        // Thread-safety: Read-only; safe for concurrent inspection.
        virtual ~TypedBlockMessageBase() = default;

        virtual const std::type_info& GetBlockType() const = 0;
    };

    template <typename T>
    struct TypedBlockMessage : public virtual TypedBlockMessageBase
    {
        // Purpose: CRTP helper to tag block messages with their payload type.
        // Ownership: Non-owning. Provides only compile-time and runtime type metadata.
        // Thread-safety: Read-only; safe for concurrent inspection.
        using payload_type = T;

        const std::type_info& GetBlockType() const override
        {
            return typeid(T);
        }
    };

    struct StageRequest
    {
        const Uuid stage_id;
    };

    struct StageViewRequest
        : public Request<std::vector<Uuid>>
        , StageRequest
    {
        StageViewRequest(
            const Uuid& id,
            const std::vector<const std::type_info*>& filter = {})
            : StageRequest(id)
            , block_type_filter(filter)
        {
        }

        std::vector<const std::type_info*> block_type_filter = {};
    };

    struct MakeToyRequest
        : public Request<Uuid>
        , StageRequest
    {
        MakeToyRequest(const Uuid& stage_id, const Uuid& toy)
            : StageRequest(stage_id)
            , toy_id(toy)
        {
        }

        const Uuid toy_id;
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

    struct StageToyRequest
        : StageRequest
    {
        StageToyRequest(const Uuid& stage, const Uuid& toy)
            : StageRequest(stage)
            , toy_id(toy)
        {
        }

        const Uuid toy_id;
    };

    struct ToyViewRequest
        : public Request<std::vector<Block>>
        , StageToyRequest
    {
        ToyViewRequest(
            const Uuid& stage_id,
            const Uuid& toy_id,
            const std::vector<const std::type_info*>& filter = {})
            : StageToyRequest(stage_id, toy_id)
            , block_type_filter(filter)
        {
        }

        std::vector<const std::type_info*> block_type_filter = {};
    };

    struct IsToyValidRequest
        : public Request<bool>
        , public StageToyRequest
    {
        IsToyValidRequest(const Uuid& stage_id, const Uuid& toy_id)
            : StageToyRequest(stage_id, toy_id)
        {
        }
    };

    struct GetToyBlockRequestBase
        : public Request<Block>
        , public StageToyRequest
        , public virtual TypedBlockMessageBase
    {
        GetToyBlockRequestBase(const Uuid& stage_id, const Uuid& toy_id)
            : StageToyRequest(stage_id, toy_id)
        {
        }
    };

    template <typename T>
    struct GetToyBlockRequest
        : public GetToyBlockRequestBase
        , public TypedBlockMessage<T>
    {
        // Purpose: Retrieves a block instance of the requested type from a toy.
        // Ownership: The block is owned by the toy; callers receive a non-owning pointer to the
        // stored std::any value wrapping the block instance.
        // Thread-safety: Not thread-safe. Calls are expected on the toy-owning thread.
        GetToyBlockRequest(const Uuid& stage_id, const Uuid& toy_id)
            : GetToyBlockRequestBase(stage_id, toy_id)
        {
        }
    };

    struct AddBlockToToyRequestBase
        : public Request<Block>
        , public StageToyRequest
        , public virtual TypedBlockMessageBase
    {
        AddBlockToToyRequestBase(const Uuid& stage_id, const Uuid& toy_id, const Block& block)
            : StageToyRequest(stage_id, toy_id)
            , block_data(block)
        {
        }

        const Block block_data;
    };

    template <typename T>
    struct AddBlockToToyRequest
        : public AddBlockToToyRequestBase
        , public TypedBlockMessage<T>
    {
        AddBlockToToyRequest(const Uuid& stage_id, const Uuid& toy_id, const T& block = T())
            : AddBlockToToyRequestBase(stage_id, toy_id, std::any(block))
        {
        }
    };

    struct RemoveBlockFromToyRequestBase
        : public Request<bool>
        , public StageToyRequest
        , public virtual TypedBlockMessageBase
    {
        RemoveBlockFromToyRequestBase(const Uuid& stage_id, const Uuid& toy_id)
            : StageToyRequest(stage_id, toy_id)
        {
        }
    };

    template <typename T>
    struct RemoveBlockFromToyRequest
        : public RemoveBlockFromToyRequestBase
        , public TypedBlockMessage<T>
    {
        RemoveBlockFromToyRequest(const Uuid& stage_id, const Uuid& toy_id)
            : RemoveBlockFromToyRequestBase(stage_id, toy_id)
        {
        }
    };
}
