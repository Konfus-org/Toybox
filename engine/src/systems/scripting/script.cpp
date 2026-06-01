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
        std::weak_ptr<World> world,
        ServiceProvider& services,
        IScriptResolver& resolver)
        : _world_id(world_id)
        , _entity(std::move(entity))
        , _world(std::move(world))
        , _services(std::ref(services))
        , _resolver(std::ref(resolver))
    {
    }

    Entity& ScriptContext::get_entity() const
    {
        TBX_ASSERT(_entity.get_id().is_valid(), "Script context does not have an entity.");
        return const_cast<Entity&>(_entity);
    }

    Uuid ScriptContext::get_entity_id() const
    {
        return _entity.get_id();
    }

    IScriptResolver& ScriptContext::get_resolver() const
    {
        TBX_ASSERT(_resolver.has_value(), "Script context does not have a script resolver.");
        return _resolver->get();
    }

    ServiceProvider& ScriptContext::get_services() const
    {
        TBX_ASSERT(_services.has_value(), "Script context does not have services.");
        return _services->get();
    }

    World& ScriptContext::get_world() const
    {
        auto world = _world.lock();
        TBX_ASSERT(world != nullptr, "Script context does not have a world.");
        return *world;
    }

    std::weak_ptr<World> ScriptContext::get_world_ptr() const
    {
        return _world;
    }

    Uuid ScriptContext::get_world_id() const
    {
        return _world_id;
    }

    void Script::bind_context(ScriptContext context)
    {
        _context = std::move(context);
    }

    void Script::bind(ScriptBinding binding)
    {
        _binding = binding;
    }

    std::optional<ScriptBinding> Script::get_script_binding() const
    {
        return _binding;
    }

    std::optional<ScriptBinding> Script::get_script_reference(std::string_view field_name) const
    {
        const auto iterator = _script_references.find(std::string(field_name));
        if (iterator == _script_references.end())
            return std::nullopt;

        return iterator->second;
    }

    Entity& Script::get_entity() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->get_entity();
    }

    ServiceProvider& Script::get_services() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->get_services();
    }

    std::weak_ptr<World> Script::get_world_ptr() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->get_world_ptr();
    }

    World& Script::get_world() const
    {
        TBX_ASSERT(_context.has_value(), "Script has no bound context.");
        return _context->get_world();
    }

    void Script::set_script_reference(std::string_view field_name, ScriptBinding binding)
    {
        _script_references[std::string(field_name)] = binding;
    }
}
