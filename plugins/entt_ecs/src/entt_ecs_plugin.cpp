#include "entt_ecs_plugin.h"
#include "tbx/messages/handler.h"
#include <type_traits>

namespace tbx::plugins::enttecs
{
    void EnttEcsPlugin::on_attach(Application&)
    {
        _is_initialized = true;
    }

    void EnttEcsPlugin::on_detach()
    {
        _stages.clear();
        _is_initialized = false;
    }

    void EnttEcsPlugin::on_update(const DeltaTime&)
    {
    }

    void EnttEcsPlugin::on_message(Message& msg)
    {
        handle_message<StageViewRequest>(msg, [this](StageViewRequest& request)
        {
            handle_stage_view_request(request);
        });

        handle_message<MakeToyRequest>(msg, [this](MakeToyRequest& request)
        {
            handle_make_toy_request(request);
        });

        handle_message<RemoveToyFromStageRequest>(msg, [this](RemoveToyFromStageRequest& request)
        {
            handle_remove_toy_request(request);
        });

        handle_message<ToyViewRequest>(msg, [this](ToyViewRequest& request)
        {
            handle_toy_view_request(request);
        });

        handle_message<IsToyValidRequest>(msg, [this](IsToyValidRequest& request)
        {
            handle_is_toy_valid_request(request);
        });

        handle_message<GetToyBlockRequestBase>(msg, [this](GetToyBlockRequestBase& request)
        {
            handle_get_toy_block_request(request);
        });

        handle_message<AddBlockToToyRequestBase>(msg, [this](AddBlockToToyRequestBase& request)
        {
            handle_add_block_request(request);
        });

        handle_message<RemoveBlockFromToyRequestBase>(msg, [this](RemoveBlockFromToyRequestBase& request)
        {
            handle_remove_block_request(request);
        });
    }

    void EnttEcsPlugin::handle_stage_view_request(StageViewRequest& request)
    {
        StageRecord& stage = resolve_stage(request.stage_id);
        request.result = build_stage_view(stage, request.block_type_filter);
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_make_toy_request(MakeToyRequest& request)
    {
        StageRecord& stage = resolve_stage(request.stage_id);
        const entt::entity entity = to_entity_id(request.toy_id);

        if (!stage.registry.valid(entity))
        {
            stage.registry.create(entity);
        }
        else
        {
            stage.registry.remove_all(entity);
        }

        stage.registry.emplace_or_replace<ToyRecord>(entity, ToyRecord { request.toy_id });
        remove_block_entities(stage, request.toy_id);

        request.result = request.toy_id;
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_remove_toy_request(RemoveToyFromStageRequest& request)
    {
        StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.state = MessageState::Failed;
            return;
        }

        const entt::entity toy_entity = find_toy_entity(*stage, request.toy_id);
        if (toy_entity == entt::null)
        {
            request.state = MessageState::Failed;
            return;
        }

        remove_block_entities(*stage, request.toy_id);

        if (stage->registry.valid(toy_entity))
        {
            stage->registry.destroy(toy_entity);
        }

        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_toy_view_request(ToyViewRequest& request) const
    {
        const StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.state = MessageState::Failed;
            return;
        }

        const entt::entity toy_entity = find_toy_entity(*stage, request.toy_id);
        if ((toy_entity == entt::null) || !stage->registry.valid(toy_entity))
        {
            request.state = MessageState::Failed;
            return;
        }

        request.result = build_block_view(*stage, request.toy_id, request.block_type_filter);
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_is_toy_valid_request(IsToyValidRequest& request) const
    {
        const StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.result = false;
            request.state = MessageState::Handled;
            return;
        }

        request.result = find_toy_entity(*stage, request.toy_id) != entt::null;
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_get_toy_block_request(GetToyBlockRequestBase& request) const
    {
        const StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.state = MessageState::Failed;
            return;
        }

        const ToyBlock* block = find_block(*stage, request.toy_id, std::type_index(request.GetBlockType()));
        if (!block)
        {
            request.result = invalid::block;
            request.state = MessageState::Handled;
            return;
        }

        request.result = static_cast<void*>(const_cast<std::any*>(&block->data));
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_add_block_request(AddBlockToToyRequestBase& request)
    {
        StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.state = MessageState::Failed;
            return;
        }

        const entt::entity toy_entity = find_toy_entity(*stage, request.toy_id);
        if (toy_entity == entt::null)
        {
            request.state = MessageState::Failed;
            return;
        }

        const std::type_index block_type = std::type_index(request.GetBlockType());
        const entt::entity block_entity = resolve_block_entity(*stage, request.toy_id, block_type);

        ToyBlock block = {};
        block.toy_id = request.toy_id;
        block.type = block_type;
        block.data = request.block_data;

        stage->registry.emplace_or_replace<ToyBlock>(block_entity, block);

        request.result = static_cast<void*>(&stage->registry.get<ToyBlock>(block_entity).data);
        request.state = MessageState::Handled;
    }

    void EnttEcsPlugin::handle_remove_block_request(RemoveBlockFromToyRequestBase& request)
    {
        StageRecord* stage = find_stage(request.stage_id);
        if (!stage)
        {
            request.state = MessageState::Failed;
            return;
        }

        const std::type_index block_type = std::type_index(request.GetBlockType());
        const ToyBlock* block = find_block(*stage, request.toy_id, block_type);
        if (!block)
        {
            request.result = false;
            request.state = MessageState::Handled;
            return;
        }

        entt::entity block_entity = find_block_entity(*stage, request.toy_id, block_type);
        if (stage->registry.valid(block_entity))
        {
            stage->registry.destroy(block_entity);
        }

        request.result = true;
        request.state = MessageState::Handled;
    }

    StageRecord& EnttEcsPlugin::resolve_stage(const Uuid& stage_id)
    {
        auto iterator = _stages.find(stage_id);
        if (iterator == _stages.end())
        {
            StageRecord stage = {};
            iterator = _stages.emplace(stage_id, std::move(stage)).first;
        }

        return iterator->second;
    }

    StageRecord* EnttEcsPlugin::find_stage(const Uuid& stage_id)
    {
        auto iterator = _stages.find(stage_id);
        if (iterator != _stages.end())
        {
            return &iterator->second;
        }

        return nullptr;
    }

    const StageRecord* EnttEcsPlugin::find_stage(const Uuid& stage_id) const
    {
        auto iterator = _stages.find(stage_id);
        if (iterator != _stages.end())
        {
            return &iterator->second;
        }

        return nullptr;
    }

    entt::entity EnttEcsPlugin::find_toy_entity(StageRecord& stage, const Uuid& toy_id)
    {
        return find_toy_entity(static_cast<const StageRecord&>(stage), toy_id);
    }

    entt::entity EnttEcsPlugin::find_toy_entity(const StageRecord& stage, const Uuid& toy_id) const
    {
        auto view = stage.registry.view<ToyRecord>();
        for (auto entity : view)
        {
            if (view.get(entity).id == toy_id)
            {
                return entity;
            }
        }

        return entt::null;
    }

    entt::entity EnttEcsPlugin::to_entity_id(const Uuid& toy_id) const
    {
        using entity_type = std::underlying_type_t<entt::entity>;

        entity_type entity_value = static_cast<entity_type>(toy_id.value);
        if (entt::entity(entity_value) == entt::null)
        {
            entity_value = static_cast<entity_type>(entity_value + 1U);
        }

        return entt::entity(entity_value);
    }

    ToyBlock* EnttEcsPlugin::find_block(StageRecord& stage, const Uuid& toy_id, const std::type_index& type)
    {
        return const_cast<ToyBlock*>(find_block(static_cast<const StageRecord&>(stage), toy_id, type));
    }

    const ToyBlock* EnttEcsPlugin::find_block(
        const StageRecord& stage,
        const Uuid& toy_id,
        const std::type_index& type) const
    {
        auto view = stage.registry.view<ToyBlock>();
        for (auto entity : view)
        {
            const ToyBlock& block = view.get(entity);
            if ((block.toy_id == toy_id) && (block.type == type))
            {
                return &block;
            }
        }

        return nullptr;
    }

    entt::entity EnttEcsPlugin::find_block_entity(
        const StageRecord& stage,
        const Uuid& toy_id,
        const std::type_index& type) const
    {
        auto view = stage.registry.view<ToyBlock>();
        for (auto entity : view)
        {
            const ToyBlock& block = view.get(entity);
            if ((block.toy_id == toy_id) && (block.type == type))
            {
                return entity;
            }
        }

        return entt::null;
    }

    entt::entity EnttEcsPlugin::resolve_block_entity(
        StageRecord& stage,
        const Uuid& toy_id,
        const std::type_index& type)
    {
        const entt::entity existing_entity = find_block_entity(stage, toy_id, type);
        if ((existing_entity != entt::null) && stage.registry.valid(existing_entity))
        {
            return existing_entity;
        }

        return stage.registry.create();
    }

    void EnttEcsPlugin::remove_block_entities(StageRecord& stage, const Uuid& toy_id)
    {
        auto view = stage.registry.view<ToyBlock>();
        std::vector<entt::entity> to_destroy = {};

        for (auto entity : view)
        {
            const ToyBlock& block = view.get(entity);
            if (block.toy_id == toy_id)
            {
                to_destroy.push_back(entity);
            }
        }

        for (auto entity : to_destroy)
        {
            if (stage.registry.valid(entity))
            {
                stage.registry.destroy(entity);
            }
        }
    }

    bool EnttEcsPlugin::matches_filters(
        const StageRecord& stage,
        const Uuid& toy_id,
        const std::vector<const std::type_info*>& filters) const
    {
        if (filters.empty())
        {
            return true;
        }

        for (const auto* filter : filters)
        {
            if (!filter)
            {
                continue;
            }

            const ToyBlock* block = find_block(stage, toy_id, std::type_index(*filter));
            if (!block)
            {
                return false;
            }
        }

        return true;
    }

    std::vector<Block> EnttEcsPlugin::build_block_view(
        const StageRecord& stage,
        const Uuid& toy_id,
        const std::vector<const std::type_info*>& filters) const
    {
        std::vector<Block> blocks = {};
        auto view = stage.registry.view<ToyBlock>();

        for (auto entity : view)
        {
            if (!stage.registry.valid(entity))
            {
                continue;
            }

            const ToyBlock& block = view.get(entity);
            if (block.toy_id != toy_id)
            {
                continue;
            }

            if (!filters.empty())
            {
                bool found = false;
                for (const auto* filter : filters)
                {
                    if (filter && (block.type == std::type_index(*filter)))
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }
            }

            blocks.push_back(static_cast<void*>(const_cast<std::any*>(&block.data)));
        }

        return blocks;
    }

    std::vector<Uuid> EnttEcsPlugin::build_stage_view(
        const StageRecord& stage,
        const std::vector<const std::type_info*>& filters) const
    {
        std::vector<Uuid> toys = {};
        auto view = stage.registry.view<ToyRecord>();

        for (auto entity : view)
        {
            if (!stage.registry.valid(entity))
            {
                continue;
            }

            const ToyRecord& record = view.get(entity);
            if (!record.id.is_valid())
            {
                continue;
            }

            if (!matches_filters(stage, record.id, filters))
            {
                continue;
            }

            toys.push_back(record.id);
        }

        return toys;
    }
}
