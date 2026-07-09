#include "physics_event_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "sync_event_ops.h"
#include "sync_event_state.h"
#include "wire.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/collider.h"
#include <string_view>

namespace tbx::studio_bridge
{
    static bool is_physics_event_key(std::string_view key)
    {
        return key == Wire::EVENT_OVERLAP_BEGAN || key == Wire::EVENT_OVERLAP_STAYED
               || key == Wire::EVENT_OVERLAP_ENDED || key == Wire::EVENT_COLLISION_BEGAN
               || key == Wire::EVENT_COLLISION_ENDED;
    }

    // The shared Trigger behavior of whichever shaped trigger the entity carries, when any (an
    // entity stacks at most one trigger shape in editor use).
    static tbx::Trigger* try_get_trigger(tbx::Entity& entity)
    {
        if (entity.has_component<tbx::BoxTrigger>())
            return &entity.get_component<tbx::BoxTrigger>();
        if (entity.has_component<tbx::SphereTrigger>())
            return &entity.get_component<tbx::SphereTrigger>();
        if (entity.has_component<tbx::CapsuleTrigger>())
            return &entity.get_component<tbx::CapsuleTrigger>();
        if (entity.has_component<tbx::MeshTrigger>())
            return &entity.get_component<tbx::MeshTrigger>();
        return nullptr;
    }

    // The shared Collider behavior of whichever solid shape the entity carries, when any.
    static tbx::Collider* try_get_collider(tbx::Entity& entity)
    {
        if (entity.has_component<tbx::BoxCollider>())
            return &entity.get_component<tbx::BoxCollider>();
        if (entity.has_component<tbx::SphereCollider>())
            return &entity.get_component<tbx::SphereCollider>();
        if (entity.has_component<tbx::CapsuleCollider>())
            return &entity.get_component<tbx::CapsuleCollider>();
        if (entity.has_component<tbx::MeshCollider>())
            return &entity.get_component<tbx::MeshCollider>();
        return nullptr;
    }

    // Attaches one forwarding-callback set to `entity`'s trigger/collider components (a no-op for an
    // entity with neither). Each raise is filtered by the subscription table, so a callback for an
    // event the editor did not subscribe to is a harmless no-op.
    static void bind_entity_physics_events(
        SyncEventState& events, const EngineServices& services, tbx::Entity entity)
    {
        const auto entity_id = entity.get_id();
        if (!entity_id.is_valid())
            return;

        // One forwarding set per entity, regardless of how many of its physics events the editor
        // subscribed to: each raise is filtered by the subscription table, so a callback with no
        // matching subscription is a harmless no-op. Re-binding the same entity does nothing.
        if (!events.bound_entities.insert(entity_id).second)
            return;

        if (auto* trigger = try_get_trigger(entity))
        {
            const auto forward_overlap = [&events, &services](std::string_view key)
            {
                return [&events, &services, key](const tbx::ColliderOverlapEvent& overlap)
                {
                    auto args = tbx::Json::object();
                    args[Wire::TRIGGER] = overlap.trigger_entity_id.value;
                    args[Wire::OTHER] = overlap.overlapped_entity_id.value;
                    raise_sync_event_for_entity(
                        events, services, overlap.trigger_entity_id.value, key, args);
                };
            };
            trigger->overlap_begin_callbacks.push_back(forward_overlap(Wire::EVENT_OVERLAP_BEGAN));
            trigger->overlap_stay_callbacks.push_back(forward_overlap(Wire::EVENT_OVERLAP_STAYED));
            trigger->overlap_end_callbacks.push_back(forward_overlap(Wire::EVENT_OVERLAP_ENDED));
        }

        if (auto* collider = try_get_collider(entity))
        {
            const auto forward_contact = [&events, &services](std::string_view key)
            {
                return [&events, &services, key](const tbx::ColliderContactEvent& contact)
                {
                    auto args = tbx::Json::object();
                    args[Wire::ENTITY] = contact.entity_id.value;
                    args[Wire::OTHER] = contact.other_entity_id.value;
                    args[Wire::POSITION] = to_wire_vec3(contact.position);
                    args[Wire::NORMAL] = to_wire_vec3(contact.normal);
                    raise_sync_event_for_entity(
                        events, services, contact.entity_id.value, key, args);
                };
            };
            collider->contact_begin_callbacks.push_back(
                forward_contact(Wire::EVENT_COLLISION_BEGAN));
            collider->contact_end_callbacks.push_back(forward_contact(Wire::EVENT_COLLISION_ENDED));
        }
    }

    void bind_subscribed_physics_events(SyncEventState& events, const EngineServices& services)
    {
        auto world = services.active_world();
        if (!world)
            return;

        for (const auto& subscription : events.subscriptions)
        {
            if (subscription.entity_id == 0U || !is_physics_event_key(subscription.key))
                continue;

            auto entity = world->get(tbx::Uuid(subscription.entity_id));
            if (entity.get_id().is_valid())
                bind_entity_physics_events(events, services, entity);
        }
    }

    void unbind_all_physics_events(SyncEventState& events, const EngineServices& services)
    {
        if (auto world = services.active_world())
        {
            for (const auto& entity_id : events.bound_entities)
            {
                auto entity = world->get(entity_id);
                if (!entity.get_id().is_valid())
                    continue;

                if (auto* trigger = try_get_trigger(entity))
                {
                    trigger->overlap_begin_callbacks.clear();
                    trigger->overlap_stay_callbacks.clear();
                    trigger->overlap_end_callbacks.clear();
                }
                if (auto* collider = try_get_collider(entity))
                {
                    collider->contact_begin_callbacks.clear();
                    collider->contact_end_callbacks.clear();
                }
            }
        }
        events.bound_entities.clear();
    }
}
