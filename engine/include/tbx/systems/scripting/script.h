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

namespace tbx
{
    class Script;
    class ScriptContext;
    class ScriptSystem;
    class World;

    template <typename TJson, typename TScript>
    void read_script_reference_field(
        const TJson& json,
        std::string_view field_name,
        Script& owner,
        std::weak_ptr<TScript>& script);

    template <typename TJson, typename TScript>
    void write_script_reference_field(
        TJson& json,
        std::string_view field_name,
        const Script& owner,
        const std::weak_ptr<TScript>& script);

    template <typename TScript>
    void bind_script_reference_field(
        Script& owner,
        std::string_view field_name,
        std::weak_ptr<TScript>& script,
        ScriptContext& context);

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
        [[editor::readonly]]
        Uuid entity = {};

        [[prop]]
        [[editor::view("script")]]
        Uuid script = {};

        [[prop]]
        [[editor::readonly]]
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

    template <typename TService>
    inline void bind_script_field(std::weak_ptr<TService>& service, ScriptContext& context)
    {
        bind_service_field(service, context.get_services());
    }

    /// @brief
    /// Purpose: Base asset type for C++ script prototypes with shared context and binding support.
    class TBX_API Script : public Asset
    {
      public:
        Script() = default;
        ~Script() noexcept override;

      public:
        Script(const Script&) = delete;
        Script& operator=(const Script&) = delete;
        Script(Script&&) noexcept = delete;
        Script& operator=(Script&&) noexcept = delete;

      protected:
        Entity& get_entity() const;
        ServiceProvider& get_services() const;
        std::weak_ptr<World> get_world_ptr() const;
        World& get_world() const;

      private:
        friend class ScriptSystem;

      private:
        template <typename TJson, typename TScript>
        friend void read_script_reference_field(
            const TJson& json,
            std::string_view field_name,
            Script& owner,
            std::weak_ptr<TScript>& script);

        template <typename TJson, typename TScript>
        friend void write_script_reference_field(
            TJson& json,
            std::string_view field_name,
            const Script& owner,
            const std::weak_ptr<TScript>& script);

        template <typename TScript>
        friend void bind_script_reference_field(
            Script& owner,
            std::string_view field_name,
            std::weak_ptr<TScript>& script,
            ScriptContext& context);

      private:
        void bind(ScriptContext context);
        std::optional<ScriptBinding> get_script_binding() const;
        std::optional<ScriptBinding> get_script_reference(std::string_view field_name) const;
        void set_script_reference(std::string_view field_name, ScriptBinding binding);
    };

    /// @brief
    /// Purpose: Base asset type for gameplay scripts driven by ScriptSystem lifetime hooks.
    class TBX_API GameplayScript : public Script
    {
      public:
        GameplayScript() = default;
        ~GameplayScript() noexcept override = default;

      public:
        GameplayScript(const GameplayScript&) = delete;
        GameplayScript& operator=(const GameplayScript&) = delete;
        GameplayScript(GameplayScript&&) noexcept = delete;
        GameplayScript& operator=(GameplayScript&&) noexcept = delete;

      public:
        virtual void on_destroy() {}
        virtual void on_fixed_update(const DeltaTime&) {}
        virtual void on_start() {}
        virtual void on_update(const DeltaTime&) {}
    };

    template <typename TJson, typename TScript>
    inline void read_script_reference_field(
        const TJson& json,
        std::string_view field_name,
        Script& owner,
        std::weak_ptr<TScript>& script)
    {
        static_assert(std::derived_from<TScript, Script>);
        auto binding = ScriptBinding {};
        read_serialization_field(json, field_name, binding, ScriptBinding {});
        owner.set_script_reference(field_name, binding);
        script = {};
    }

    template <typename TJson, typename TScript>
    inline void write_script_reference_field(
        TJson& json,
        std::string_view field_name,
        const Script& owner,
        const std::weak_ptr<TScript>& script)
    {
        static_assert(std::derived_from<TScript, Script>);
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
    inline void bind_script_reference_field(
        Script& owner,
        std::string_view field_name,
        std::weak_ptr<TScript>& script,
        ScriptContext& context)
    {
        static_assert(std::derived_from<TScript, Script>);
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
