#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_context.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>
#include <optional>

namespace tbx
{
    class World;
    class Script;

    // Identifies which runtime script instance a reference resolves to (a script on a given entity
    // in a given world). Used by ScriptSystem's resolver.
    struct ScriptLookup
    {
        Uuid world = {};
        Uuid entity = {};
        Uuid script = {};
        Uuid binding_id = {};
    };

    /// @brief
    /// Purpose: Serialized binding identity for a weak script reference.
    [[serializable]];
    struct TBX_API ScriptBinding
    {
        Uuid entity = {};

        [[do_not_serialize]]
        Uuid script = {};

        Uuid binding_id = {};
    };

    // Resolves a script reference to the live Script instance it names. Implemented by ScriptSystem.
    class IScriptResolver
    {
      public:
        virtual ~IScriptResolver() noexcept = default;

        virtual std::weak_ptr<Script> try_get_script(const ScriptLookup& lookup) = 0;
    };

    /// @brief
    /// Purpose: Runtime context bound to one active script instance (entity/world/services/resolver).
    class TBX_API ScriptContext final
    {
      public:
        ScriptContext() = default;
        ScriptContext(
            Uuid world_id,
            ScriptBinding binding,
            Entity entity,
            std::weak_ptr<World> world,
            ServiceProvider& services,
            IScriptResolver& resolver);

      public:
        Entity& get_entity() const;
        Uuid get_entity_id() const;
        ScriptBinding get_script_binding() const;
        IScriptResolver& get_resolver() const;
        ServiceProvider& get_services() const;
        std::weak_ptr<World> get_world_ptr() const;
        World& get_world() const;
        Uuid get_world_id() const;

      private:
        Uuid _world_id = {};
        std::weak_ptr<World> _world = {};
        Entity _entity = {};
        ScriptBinding _binding = {};
        std::optional<std::reference_wrapper<ServiceProvider>> _services = std::nullopt;
        std::optional<std::reference_wrapper<IScriptResolver>> _resolver = std::nullopt;
    };
}
