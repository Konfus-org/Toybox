#include "physics_rpc_handlers.h"
#include "bridge_utils.h"
#include "wire.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/raycast.h"
#include "tbx/types/vectors.h"

namespace tbx::studio_bridge
{
    // The wire spells vectors as bare [x, y, z] arrays (the editor's WireValue shape).
    static tbx::Vec3 read_vec3(const tbx::Json& params, std::string_view key)
    {
        const auto iterator = params.find(key);
        if (iterator == params.end() || !iterator->is_array() || iterator->size() < 3)
            return tbx::Vec3(0.0F);

        const auto& token = *iterator;
        return {token[0].get<float>(), token[1].get<float>(), token[2].get<float>()};
    }

    static Result physics_raycast(
        EngineServices& services, const tbx::Json& params, tbx::Json& out_reply)
    {
        const auto physics = services.physics.lock();
        if (!physics)
            return Result(false, "physics.raycast: the physics service is unavailable.");

        auto query = tbx::RaycastQuery();
        query.ray.origin = read_vec3(params, Wire::ORIGIN);
        query.ray.direction = read_vec3(params, Wire::DIRECTION);
        query.max_distance = params.value(Wire::MAX_DISTANCE, 100.0F);
        const auto ignored = params.value(Wire::IGNORE_ENTITY_ID, uint64(0));
        query.ignore_entity = ignored != 0U;
        query.ignored_entity_id = tbx::Uuid(ignored);

        const auto hit = physics->raycast(query);
        out_reply[Wire::HAS_HIT] = hit.has_hit;
        out_reply[Wire::ENTITY_ID] = hit.hit_entity_id.value;
        out_reply[Wire::POSITION] = to_wire_vec3(hit.hit_position);
        out_reply[Wire::FRACTION] = hit.hit_fraction;
        return Result::OK;
    }

    template <typename TTrigger>
    static bool request_trigger_scan(tbx::Entity& entity)
    {
        if (!entity.has_component<TTrigger>())
            return false;

        entity.get_component<TTrigger>().request_overlap_scan();
        return true;
    }

    // Requests a manual overlap scan on every trigger the addressed entity carries — an entity has one
    // trigger in practice, and scanning them all matches the engine's per-trigger request semantics.
    static Result physics_overlap_scan(EngineServices& services, const tbx::Json& params)
    {
        const auto address = params.value(Wire::ADDRESS, std::string());
        const auto entity_id = parse_address_entity(address);
        if (entity_id == 0U)
            return Result(false, "physics.overlapScan: '" + address + "' names no entity.");

        const auto world = services.active_world();
        if (!world)
            return Result(false, "physics.overlapScan: no active world.");

        auto entity = world->get(tbx::Uuid(entity_id));
        if (!entity.get_id().is_valid())
            return Result(false, "physics.overlapScan: entity not found.");

        auto requested = request_trigger_scan<tbx::BoxTrigger>(entity);
        requested = request_trigger_scan<tbx::SphereTrigger>(entity) || requested;
        requested = request_trigger_scan<tbx::CapsuleTrigger>(entity) || requested;
        requested = request_trigger_scan<tbx::MeshTrigger>(entity) || requested;
        return requested ? Result::OK
                         : Result(false, "physics.overlapScan: the entity has no trigger component.");
    }

    void register_physics_handlers(const RpcRegistrar& registrar, EngineServices& services)
    {
        registrar.add_query(
            Wire::PHYSICS_RAYCAST,
            [&services](const tbx::Json& params, tbx::Json& reply)
            {
                return physics_raycast(services, params, reply);
            });
        registrar.add(
            Wire::PHYSICS_OVERLAP_SCAN,
            [&services](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(physics_overlap_scan(services, params));
            });
    }
}
