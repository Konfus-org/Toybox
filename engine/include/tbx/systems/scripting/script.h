#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script.generated.h"
#include "tbx/systems/scripting/service_ref.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tbx
{
    class Script;
    class World;

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
        [[prop]]
        Uuid entity = {};

        [[prop]]
        Uuid script = {};

        [[prop]]
        Uuid binding_id = {};
    };

    class IScriptResolver
    {
      public:
        virtual ~IScriptResolver() noexcept = default;

        virtual std::weak_ptr<Script> try_get_script(const ScriptLookup& lookup) = 0;
    };

    /// @brief
    /// Purpose: Provides runtime context bound to one active script instance.
    class TBX_API ScriptContext final
    {
      public:
        ScriptContext() = default;
        ScriptContext(
            Uuid world_id,
            Entity entity,
            std::weak_ptr<World> world,
            ServiceProvider& services,
            IScriptResolver& resolver);

      public:
        Entity& get_entity() const;
        Uuid get_entity_id() const;
        IScriptResolver& get_resolver() const;
        ServiceProvider& get_services() const;
        std::weak_ptr<World> get_world_ptr() const;
        World& get_world() const;
        Uuid get_world_id() const;

      private:
        Uuid _world_id = {};
        Entity _entity = {};
        std::weak_ptr<World> _world = {};
        std::optional<std::reference_wrapper<ServiceProvider>> _services = std::nullopt;
        std::optional<std::reference_wrapper<IScriptResolver>> _resolver = std::nullopt;
    };

    template <typename TService>
    inline void bind_script_field(std::weak_ptr<TService>& service, ScriptContext& context)
    {
        bind_service_field(service, context.get_services());
    }

    /// @brief
    /// Purpose: Base asset type for C++ gameplay behavior prototypes.
    class TBX_API Script : public Asset
    {
      public:
        Script() = default;
        virtual ~Script() noexcept = default;

      public:
        Script(const Script&) = delete;
        Script& operator=(const Script&) = delete;
        Script(Script&&) noexcept = delete;
        Script& operator=(Script&&) noexcept = delete;

      public:
        void bind(ScriptBinding binding);
        void bind_context(ScriptContext context);
        std::optional<ScriptBinding> get_script_binding() const;
        std::optional<ScriptBinding> get_script_reference(std::string_view field_name) const;
        void set_script_reference(std::string_view field_name, ScriptBinding binding);

        virtual void on_destroy() {}
        virtual void on_fixed_update(const DeltaTime&) {}
        virtual void on_start() {}
        virtual void on_update(const DeltaTime&) {}

      protected:
        Entity& get_entity() const;
        ServiceProvider& get_services() const;
        std::weak_ptr<World> get_world_ptr() const;
        World& get_world() const;

      private:
        std::optional<ScriptContext> _context = std::nullopt;
        std::optional<ScriptBinding> _binding = std::nullopt;
        std::unordered_map<std::string, ScriptBinding> _script_references = {};
    };

    template <typename TJson, typename TScript>
        requires std::derived_from<TScript, Script>
    inline void read_script_reference_field(
        const TJson& json,
        std::string_view field_name,
        Script& owner,
        std::weak_ptr<TScript>& script)
    {
        auto binding = ScriptBinding {};
        read_serialization_field(json, field_name, binding, ScriptBinding {});
        owner.set_script_reference(field_name, binding);
        script = {};
    }

    template <typename TJson, typename TScript>
        requires std::derived_from<TScript, Script>
    inline void write_script_reference_field(
        TJson& json,
        std::string_view field_name,
        const Script& owner,
        const std::weak_ptr<TScript>& script)
    {
        auto binding = ScriptBinding {};
        if (const auto resolved = script.lock())
        {
            if (const auto resolved_binding = resolved->get_script_binding())
                binding = *resolved_binding;
        }
        if (!binding.script.is_valid())
        {
            if (const auto stored_binding = owner.get_script_reference(field_name))
                binding = *stored_binding;
        }

        write_serialization_field(json, field_name, binding);
    }

    template <typename TScript>
        requires std::derived_from<TScript, Script>
    inline void bind_script_reference_field(
        Script& owner,
        std::string_view field_name,
        std::weak_ptr<TScript>& script,
        ScriptContext& context)
    {
        const auto binding = owner.get_script_reference(field_name);
        if (!binding.has_value() || !binding->script.is_valid())
        {
            script = {};
            return;
        }

        auto resolved = context.get_resolver()
                            .try_get_script(
                                ScriptLookup {
                                    .world = context.get_world_id(),
                                    .entity = binding->entity.is_valid() ? binding->entity
                                                                         : context.get_entity_id(),
                                    .script = binding->script,
                                    .binding_id = binding->binding_id,
                                })
                            .lock();
        script = std::dynamic_pointer_cast<TScript>(resolved);
    }
}
