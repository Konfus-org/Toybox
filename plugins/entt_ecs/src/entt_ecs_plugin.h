#pragma once
#include <any>
#include "tbx/ecs/requests.h"
#include "tbx/plugin_api/plugin.h"
#include <entt/entt.hpp>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tbx::plugins::enttecs
{
    struct ToyRecord
    {
        // Purpose: Identifies a toy entity stored in a stage registry.
        // Ownership: Owned by the stage registry.
        // Thread-safety: Not thread-safe; only valid on the ECS thread.
        Uuid id = invalid::uuid;
    };

    struct ToyBlock
    {
        // Purpose: Represents a single block stored as its own component entity within a stage.
        // Ownership: Owned by the stage registry; the registry controls lifetime of the block data.
        // Thread-safety: Not thread-safe; access is limited to the Toybox ECS thread.
        Uuid toy_id = invalid::uuid;
        std::type_index type = typeid(void);
        std::any data = {};
    };

    struct StageRecord
    {
        // Purpose: Stores the runtime registry for a managed stage.
        // Ownership: Owned by the EnttEcsPlugin; registries are reset when the plugin detaches.
        // Thread-safety: Not thread-safe; expects all access from the Toybox ECS thread.
        entt::registry registry = {};
    };

    class EnttEcsPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;

      private:
        void handle_stage_view_request(StageViewRequest& request);
        void handle_make_toy_request(MakeToyRequest& request);
        void handle_remove_toy_request(RemoveToyFromStageRequest& request);
        void handle_toy_view_request(ToyViewRequest& request) const;
        void handle_is_toy_valid_request(IsToyValidRequest& request) const;
        void handle_get_toy_block_request(GetToyBlockRequestBase& request) const;
        void handle_add_block_request(AddBlockToToyRequestBase& request);
        void handle_remove_block_request(RemoveBlockFromToyRequestBase& request);

        StageRecord& resolve_stage(const Uuid& stage_id);
        StageRecord* find_stage(const Uuid& stage_id);
        const StageRecord* find_stage(const Uuid& stage_id) const;
        entt::entity find_toy_entity(StageRecord& stage, const Uuid& toy_id);
        entt::entity find_toy_entity(const StageRecord& stage, const Uuid& toy_id) const;
        entt::entity to_entity_id(const Uuid& toy_id) const;

        bool matches_filters(
            const StageRecord& stage,
            const Uuid& toy_id,
            const std::vector<const std::type_info*>& filters) const;
        std::vector<Block> build_block_view(
            const StageRecord& stage,
            const Uuid& toy_id,
            const std::vector<const std::type_info*>& filters) const;
        std::vector<Uuid> build_stage_view(
            const StageRecord& stage,
            const std::vector<const std::type_info*>& filters) const;
        ToyBlock* find_block(StageRecord& stage, const Uuid& toy_id, const std::type_index& type);
        const ToyBlock* find_block(
            const StageRecord& stage,
            const Uuid& toy_id,
            const std::type_index& type) const;
        entt::entity find_block_entity(
            const StageRecord& stage,
            const Uuid& toy_id,
            const std::type_index& type) const;
        entt::entity resolve_block_entity(StageRecord& stage, const Uuid& toy_id, const std::type_index& type);
        void remove_block_entities(StageRecord& stage, const Uuid& toy_id);

        std::unordered_map<Uuid, StageRecord, UuidHash> _stages = {};
        bool _is_initialized = false;
    };
}
