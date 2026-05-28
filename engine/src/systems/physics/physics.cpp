#include "tbx/systems/physics/physics.h"
#include "systems/physics/internal/physics_internal.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/components/world_simulation_state.h"
#include "tbx/types/quaternions.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
namespace tbx
{
    Physics::Physics(
        std::weak_ptr<IPhysicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<AppSettings> settings)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _settings(std::move(settings))
    {
        if (auto backend_strong = _backend.lock())
            backend_strong->initialize(get_backend_settings());
    }

    Physics::~Physics() noexcept
    {
        clear_resources();
        if (auto backend = _backend.lock())
            backend->shutdown();
    }

    RaycastResult Physics::raycast(const RaycastQuery& raycast_query) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return {};

        auto ignored_rigidbody = PhysicsRigidbodyHandle {};
        if (raycast_query.ignore_entity && raycast_query.ignored_entity_id.is_valid())
        {
            if (const auto record_it = _records_by_entity.find(raycast_query.ignored_entity_id);
                record_it != _records_by_entity.end())
                ignored_rigidbody = record_it->second.rigidbody;
        }

        auto backend_hit = PhysicsRaycastHit {};
        if (!backend->raycast(raycast_query, ignored_rigidbody, backend_hit) || !backend_hit)
            return {};

        return RaycastResult {
            .has_hit = true,
            .hit_entity_id = try_get_entity_for_rigidbody(backend_hit.rigidbody),
            .hit_position = backend_hit.hit_position,
            .hit_fraction = backend_hit.hit_fraction,
        };
    }

    void Physics::update(const DeltaTime& dt)
    {
        if (_backend.expired())
            return;

        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return;

        for (const auto& world : asset_manager->get_loaded<World>())
        {
            if (!world)
                continue;

            sync_entities_to_backend(*world, static_cast<float>(dt.seconds));
        }
        if (auto backend = _backend.lock())
            backend->update(get_backend_settings(), dt);
        for (const auto& world : asset_manager->get_loaded<World>())
        {
            if (!world)
                continue;

            sync_backend_to_entities(*world);
            process_trigger_colliders(*world);
        }
    }

    void Physics::clear_resources()
    {
        for (auto& record_entry : _records_by_entity)
            destroy_record(record_entry.second);

        _records_by_entity.clear();
        _entity_by_rigidbody_handle.clear();
        _overlap_entities_by_trigger.clear();
    }

    void Physics::destroy_record(PhysicsEntityRecord& record)
    {
        if (auto backend = _backend.lock())
        {
            if (record.rigidbody.is_valid())
                backend->destroy_rigidbody(record.rigidbody);

            if (record.collider.is_valid())
                backend->destroy_collider(record.collider);
        }

        _entity_by_rigidbody_handle.erase(record.rigidbody.value);
        record = {};
    }

    PhysicsBackendSettings Physics::get_backend_settings() const
    {
        auto settings = _settings.lock();
        if (!settings)
            return {};

        const auto& physics_settings = settings->physics;
        return PhysicsBackendSettings {
            .gravity = physics_settings.gravity.value,
            .max_body_count = physics_settings.max_body_count.value,
            .max_contact_constraints = physics_settings.max_contact_constraints.value,
            .max_body_pairs = physics_settings.max_body_pairs.value,
            .solver_velocity_iterations = physics_settings.solver_velocity_iterations.value,
            .solver_position_iterations = physics_settings.solver_position_iterations.value,
            .max_linear_velocity = physics_settings.max_linear_velocity.value,
            .max_angular_velocity = physics_settings.max_angular_velocity.value,
        };
    }

    void Physics::process_trigger_colliders(World& world)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        auto active_trigger_entities = std::unordered_set<Uuid>();
        auto trigger_entities = world.get_with<Transform>();
        for (auto& trigger_entity : trigger_entities)
        {
            const Uuid trigger_entity_id = trigger_entity.get_id();
            auto* trigger_collider = internal::try_get_trigger_collider(trigger_entity);
            if (trigger_collider == nullptr)
                continue;

            active_trigger_entities.insert(trigger_entity_id);
            if (!trigger_collider->is_overlap_enabled)
            {
                trigger_collider->is_manual_scan_requested = false;
                _overlap_entities_by_trigger.erase(trigger_entity_id);
                continue;
            }

            if (!internal::should_execute_overlap_query(
                    trigger_collider->overlap_execution_mode,
                    trigger_collider->is_manual_scan_requested))
            {
                trigger_collider->is_manual_scan_requested = false;
                continue;
            }
            trigger_collider->is_manual_scan_requested = false;

            auto current_overlaps = std::unordered_set<Uuid>();
            if (const auto record_it = _records_by_entity.find(trigger_entity_id);
                record_it != _records_by_entity.end())
            {
                auto overlapped_rigidbodies = std::vector<PhysicsRigidbodyHandle> {};
                backend->get_rigidbody_overlaps(
                    record_it->second.rigidbody,
                    overlapped_rigidbodies);
                current_overlaps.reserve(overlapped_rigidbodies.size());
                for (const PhysicsRigidbodyHandle overlapped_rigidbody : overlapped_rigidbodies)
                {
                    const Uuid overlapped_entity_id =
                        try_get_entity_for_rigidbody(overlapped_rigidbody);
                    if (!overlapped_entity_id.is_valid()
                        || overlapped_entity_id == trigger_entity_id)
                        continue;

                    current_overlaps.insert(overlapped_entity_id);
                }
            }

            auto& previous_overlaps = _overlap_entities_by_trigger[trigger_entity_id];
            for (const Uuid& overlapped_entity_id : current_overlaps)
            {
                const ColliderOverlapEvent event = ColliderOverlapEvent {
                    .trigger_entity_id = trigger_entity_id,
                    .overlapped_entity_id = overlapped_entity_id,
                };
                const bool was_overlapping = previous_overlaps.contains(overlapped_entity_id);
                const auto& callbacks = was_overlapping ? trigger_collider->overlap_stay_callbacks
                                                        : trigger_collider->overlap_begin_callbacks;
                for (const auto& callback : callbacks)
                {
                    if (callback)
                        callback(event);
                }
            }

            for (const Uuid& overlapped_entity_id : previous_overlaps)
            {
                if (current_overlaps.contains(overlapped_entity_id))
                    continue;

                const ColliderOverlapEvent event = ColliderOverlapEvent {
                    .trigger_entity_id = trigger_entity_id,
                    .overlapped_entity_id = overlapped_entity_id,
                };
                for (const auto& callback : trigger_collider->overlap_end_callbacks)
                {
                    if (callback)
                        callback(event);
                }
            }

            if (current_overlaps.empty())
            {
                _overlap_entities_by_trigger.erase(trigger_entity_id);
                continue;
            }

            previous_overlaps = std::move(current_overlaps);
        }

        auto stale_trigger_entities = std::vector<Uuid>();
        stale_trigger_entities.reserve(_overlap_entities_by_trigger.size());
        for (const auto& overlap_entry : _overlap_entities_by_trigger)
        {
            if (active_trigger_entities.contains(overlap_entry.first))
                continue;

            stale_trigger_entities.push_back(overlap_entry.first);
        }

        for (const Uuid& stale_trigger_entity : stale_trigger_entities)
            _overlap_entities_by_trigger.erase(stale_trigger_entity);
    }

    void Physics::sync_entities_to_backend(World& world, float dt_seconds)
    {
        auto asset_manager = _asset_manager.lock();
        auto backend = _backend.lock();
        if (!asset_manager || !backend)
            return;

        auto active_entities = std::unordered_set<Uuid>();

        auto entities = world.get_with<Transform>();
        for (auto& entity : entities)
        {
            const Uuid entity_id = entity.get_id();
            if (entity.has_component<WorldSimulationState>()
                && entity.get_component<WorldSimulationState>().mode != WorldSimulationMode::FULL)
                continue;

            const auto world_transform = get_world_space_transform(entity);
            const bool has_rigidbody_component = entity.has_component<Rigidbody>();
            const bool has_collider = internal::has_any_collider(entity);
            if (!has_rigidbody_component && !has_collider)
                continue;

            const bool is_trigger_only = internal::is_trigger_only_collider(entity);
            const auto* rigidbody =
                has_rigidbody_component ? &entity.get_component<Rigidbody>() : nullptr;
            const bool is_physics_driven = rigidbody != nullptr && rigidbody->is_valid();
            if (has_rigidbody_component && !is_physics_driven)
                continue;

            active_entities.insert(entity_id);

            auto record_it = _records_by_entity.find(entity_id);
            if (record_it != _records_by_entity.end()
                && (record_it->second.is_physics_driven != is_physics_driven
                    || record_it->second.is_trigger_only != is_trigger_only
                    || (entity.has_component<MeshCollider>() && record_it->second.has_last_transform
                        && internal::has_scale_changed(
                            world_transform.scale,
                            record_it->second.last_scale))))
            {
                destroy_record(record_it->second);
                _records_by_entity.erase(record_it);
                record_it = _records_by_entity.end();
            }

            if (record_it == _records_by_entity.end())
            {
                const PhysicsColliderCreateInfo collider_info =
                    internal::create_collider_info_for_entity(
                        *asset_manager,
                        entity,
                        world_transform,
                        is_physics_driven);
                PhysicsColliderHandle collider = backend->create_collider(collider_info);
                if (!collider.is_valid())
                    continue;

                const PhysicsRigidbodyCreateInfo rigidbody_info = PhysicsRigidbodyCreateInfo {
                    .collider = collider,
                    .transform = world_transform,
                    .rigidbody = rigidbody != nullptr ? *rigidbody : Rigidbody {},
                    .has_rigidbody = is_physics_driven,
                    .is_trigger_only = is_trigger_only,
                };
                PhysicsRigidbodyHandle rigidbody_handle = backend->create_rigidbody(rigidbody_info);
                if (!rigidbody_handle.is_valid())
                {
                    backend->destroy_collider(collider);
                    continue;
                }

                auto& record = _records_by_entity[entity_id];
                record.collider = collider;
                record.rigidbody = rigidbody_handle;
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                record.is_physics_driven = is_physics_driven;
                record.is_trigger_only = is_trigger_only;
                _entity_by_rigidbody_handle[rigidbody_handle.value] = entity_id;
                continue;
            }

            auto& record = record_it->second;
            const bool transform_is_dirty = record.has_last_transform
                                            && internal::has_transform_changed(
                                                world_transform,
                                                record.last_position,
                                                record.last_rotation,
                                                record.last_scale);
            auto update_info = PhysicsRigidbodyUpdateInfo {
                .transform = world_transform,
                .rigidbody = rigidbody != nullptr ? *rigidbody : Rigidbody {},
                .has_rigidbody = is_physics_driven,
                .is_trigger_only = is_trigger_only,
                .is_transform_dirty = transform_is_dirty,
                .dt_seconds = std::max(0.0001F, dt_seconds),
            };

            if (is_physics_driven && rigidbody != nullptr && !rigidbody->is_kinematic
                && transform_is_dirty
                && rigidbody->transform_sync_mode == PhysicsTransformSyncMode::SWEEP)
            {
                const PhysicsRigidbodyState current_state =
                    backend->get_rigidbody_state(record.rigidbody);
                const float safe_dt_seconds = std::max(0.0001F, dt_seconds);
                if (current_state.is_valid)
                {
                    update_info.sweep_linear_velocity =
                        (world_transform.position - current_state.transform.position)
                        / safe_dt_seconds;
                    update_info.sweep_angular_velocity =
                        internal::calculate_angular_velocity_for_step(
                            current_state.transform.rotation,
                            world_transform.rotation,
                            safe_dt_seconds);
                }
            }

            backend->update_rigidbody(record.rigidbody, update_info);
            if (!is_physics_driven || (rigidbody != nullptr && rigidbody->is_kinematic))
            {
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
            }
        }

        auto stale_entities = std::vector<Uuid>();
        stale_entities.reserve(_records_by_entity.size());
        for (const auto& record_entry : _records_by_entity)
        {
            if (active_entities.contains(record_entry.first))
                continue;

            stale_entities.push_back(record_entry.first);
        }

        for (const Uuid& stale_entity : stale_entities)
        {
            auto record_it = _records_by_entity.find(stale_entity);
            if (record_it == _records_by_entity.end())
                continue;

            destroy_record(record_it->second);
            _records_by_entity.erase(record_it);
        }
    }

    void Physics::sync_backend_to_entities(World& world)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        for (auto& record_entry : _records_by_entity)
        {
            const Uuid& entity_id = record_entry.first;
            auto& record = record_entry.second;

            if (!world.has<Transform>(entity_id))
                continue;

            auto entity = world.get(entity_id);
            if (!entity.get_id().is_valid())
                continue;
            auto& transform = entity.get_component<Transform>();

            if (!world.has<Rigidbody>(entity_id))
            {
                const auto world_transform = get_world_space_transform(entity);
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                continue;
            }

            PhysicsRigidbodyState state = backend->get_rigidbody_state(record.rigidbody);
            if (!state.is_valid)
                continue;

            auto& rigidbody = entity.get_component<Rigidbody>();
            rigidbody.linear_velocity = state.linear_velocity;
            rigidbody.angular_velocity = state.angular_velocity;

            if (rigidbody.is_kinematic)
            {
                const auto world_transform = get_world_space_transform(entity);
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                continue;
            }

            auto world_transform = state.transform;
            world_transform.scale = transform.scale;
            auto parent_entity = Entity {};
            if (entity.try_get_parent_entity(parent_entity))
            {
                const auto parent_world_transform = get_world_space_transform(parent_entity);
                const auto local_transform =
                    world_to_local_tranform(parent_world_transform, world_transform);
                transform.position = local_transform.position;
                transform.rotation = local_transform.rotation;
            }
            else
            {
                transform.position = world_transform.position;
                transform.rotation = world_transform.rotation;
            }

            record.last_position = world_transform.position;
            record.last_rotation = world_transform.rotation;
            record.last_scale = world_transform.scale;
            record.has_last_transform = true;
        }
    }

    Uuid Physics::try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const
    {
        auto entity_it = _entity_by_rigidbody_handle.find(rigidbody.value);
        if (entity_it == _entity_by_rigidbody_handle.end())
            return {};

        return entity_it->second;
    }
}
