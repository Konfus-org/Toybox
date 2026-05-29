#include "tbx/systems/scripting/script.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/assets/world.h"
#include <utility>

namespace tbx
{
    ScriptContext::ScriptContext(
        Uuid world_id,
        Entity entity,
        World* world,
        ServiceProvider* services,
        IScriptResolver* resolver)
        : _world_id(world_id)
        , _entity(std::move(entity))
        , _world(world)
        , _services(services)
        , _resolver(resolver)
    {
    }

    Entity& ScriptContext::entity() const
    {
        TBX_ASSERT(_entity.get_id().is_valid(), "Script context does not have an entity.");
        return const_cast<Entity&>(_entity);
    }

    Uuid ScriptContext::entity_id() const
    {
        return _entity.get_id();
    }

    IScriptResolver& ScriptContext::resolver() const
    {
        TBX_ASSERT(_resolver != nullptr, "Script context does not have a script resolver.");
        return *_resolver;
    }

    ServiceProvider& ScriptContext::services() const
    {
        TBX_ASSERT(_services != nullptr, "Script context does not have services.");
        return *_services;
    }

    World& ScriptContext::world() const
    {
        TBX_ASSERT(_world != nullptr, "Script context does not have a world.");
        return *_world;
    }

    Uuid ScriptContext::world_id() const
    {
        return _world_id;
    }

    void Script::bind_context(ScriptContext context)
    {
        _context = std::move(context);
    }

    Entity& Script::entity() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->entity();
    }

    ServiceProvider& Script::services() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->services();
    }

    World& Script::world() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->world();
    }
}
