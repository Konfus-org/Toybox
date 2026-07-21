#include "tbx/systems/connections/property_connection_system.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/world/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/property_connections.h"
#include <string>
#include <utility>

namespace tbx
{
    // Copies one connection's source property onto its target. A dead source (entity gone, component
    // or property unknown) is silently skipped — the wire stays authored, the editor shows it broken,
    // and it resumes the moment the source exists again.
    static void apply_connection(World& world, Entity& target, const PropertyConnection& connection)
    {
        if (!world.has(connection.source_entity))
            return;

        auto source = world.get(connection.source_entity);
        auto value_json = std::string();
        if (!serialize_component_property(
                source, connection.source_component, connection.source_field, value_json))
            return;

        // serialize_component_property returns the bare property value, which is exactly what
        // apply_component_property wants.
        static_cast<void>(apply_component_property(
            target, connection.target_component, connection.target_field, value_json));
    }

    void evaluate_property_connections(World& world)
    {
        world.for_each_with<PropertyConnections>(
            [&world](Entity& entity)
            {
                for (const auto& connection :
                     entity.get_component<PropertyConnections>().connections)
                    apply_connection(world, entity, connection);
            });
    }

    PropertyConnectionSystem::PropertyConnectionSystem(std::weak_ptr<WorldManager> world_manager)
        : _world_manager(std::move(world_manager))
    {
    }

    void PropertyConnectionSystem::update()
    {
        auto manager = _world_manager.lock();
        if (!manager)
            return;

        if (auto world = manager->get_active_world().lock())
            evaluate_property_connections(*world);
    }
}
