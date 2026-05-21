#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include <mutex>

namespace tbx::internal
{
    using EntityHandle = entt::entity;

    struct EntityNameComponent
    {
        std::string value = "";
    };

    struct EntityTagComponent
    {
        std::string value = "";
    };

    struct EntityLayerComponent
    {
        std::string value = "";
    };

    struct EntityParentComponent
    {
        Uuid value = {};
    };

    static EntityHandle to_entity_handle(const Uuid& id)
    {
        if (!id.is_valid())
            return entt::null;

        return static_cast<EntityHandle>(id.value - 1U);
    }

    static Uuid to_entity_id(EntityHandle handle)
    {
        const auto handle_value = static_cast<uint32>(entt::to_integral(handle));
        return Uuid(handle_value + 1U);
    }

    template <typename TComponent, typename TValue>
    static void set_component_value(entt::registry& registry, const Uuid& id, TValue&& value)
    {
        auto entityHandle = to_entity_handle(id);
        if (!registry.valid(entityHandle))
            return;

        if (!registry.all_of<TComponent>(entityHandle))
        {
            registry.emplace<TComponent>(
                entityHandle,
                TComponent {.value = std::forward<TValue>(value)});
            return;
        }

        registry.get<TComponent>(entityHandle).value = std::forward<TValue>(value);
    }

    template <typename TComponent, typename TValue>
    static TValue get_component_value(const entt::registry& registry, const Uuid& id)
    {
        auto entityHandle = to_entity_handle(id);
        if (!registry.valid(entityHandle))
            return {};

        if (!registry.all_of<TComponent>(entityHandle))
            return {};

        return registry.get<TComponent>(entityHandle).value;
    }

}
