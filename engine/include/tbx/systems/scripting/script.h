#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/scripting/script_context.h"
#include "tbx/systems/scripting/script_registry.h"
#include "tbx/systems/scripting/service_ref.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/handle.h"
#include <concepts>
#include <memory>
#include <optional>
#include <string_view>

namespace tbx
{
    class Script;
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

    // Binds a script's [[tbx::inject]] service field from the runtime context. The no-op base leaves
    // plain (non-service) fields untouched; the weak_ptr overload resolves the service.
    template <typename TValue>
    inline void bind_script_field(TValue&, ScriptContext&)
    {
    }

    template <typename TService>
    inline void bind_script_field(std::weak_ptr<TService>& service, ScriptContext& context)
    {
        bind_service_field(service, context.get_services());
    }

    /// @brief
    /// Purpose: Base class for script prototypes with shared runtime context and binding support.
    /// @details
    /// Lifetime hooks are driven by ScriptSystem (via a scripting backend): on_start once before the
    /// first update, on_update / on_fixed_update each tick, and on_destroy when the binding is removed.
    /// Subclasses override the hooks they need; the empty defaults make every hook optional. A Script is
    /// both an Asset (serialized prototype) and an IScriptInstance (runtime lifetime). It lives in the
    /// engine so compiled C++ scripts inherit it directly and future language backends reuse it.
    class TBX_API Script : public Asset, public IScriptInstance
    {
      public:
        Script() = default;
        ~Script() noexcept override;

      public:
        Script(const Script&) = delete;
        Script& operator=(const Script&) = delete;
        Script(Script&&) noexcept = delete;
        Script& operator=(Script&&) noexcept = delete;

      public:
        void on_start() override {}
        void on_update(const DeltaTime&) override {}
        void on_fixed_update(const DeltaTime&) override {}
        void on_destroy() override {}

        // Binds the per-tick runtime context (entity/world/services). Driven by the scripting backend
        // before the lifecycle hooks run.
        void bind(ScriptContext context);

      protected:
        Entity& get_entity() const;
        ServiceProvider& get_services() const;
        std::weak_ptr<World> get_world_ptr() const;
        World& get_world() const;

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
        std::optional<ScriptBinding> get_script_binding() const;
        std::optional<ScriptBinding> get_script_reference(std::string_view field_name) const;
        void set_script_reference(std::string_view field_name, ScriptBinding binding);
    };

    // A script reference is serialized as just the referenced script asset's id, carried as a Handle so
    // the editor renders it with the generic asset picker (no script type needed). It resolves to that
    // script's instance on the same entity at bind time.
    template <typename TJson, typename TScript>
    inline void read_script_reference_field(
        const TJson& json,
        std::string_view field_name,
        Script& owner,
        std::weak_ptr<TScript>& script)
    {
        static_assert(std::derived_from<TScript, Script>);
        auto reference = Handle {};
        read_typed_serialization_field(json, field_name, reference, Handle {});
        auto binding = ScriptBinding {};
        binding.script = reference.id;
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
        auto reference = Handle {};
        if (const auto resolved = script.lock())
        {
            if (const auto resolved_binding = resolved->get_script_binding())
                reference.id = resolved_binding->script;
        }
        if (!reference.id.is_valid())
        {
            if (const auto stored_binding = owner.get_script_reference(field_name))
                reference.id = stored_binding->script;
        }

        write_typed_serialization_field(json, field_name, reference);
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
                                    .entity = context.get_entity_id(),
                                    .script = binding->script,
                                    .binding_id = {},
                                })
                            .lock();
        script = std::dynamic_pointer_cast<TScript>(resolved);
    }
}
