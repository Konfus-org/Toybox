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
    /// Purpose: Base class for script prototypes: both the serialized asset prototype and the live
    /// runtime instance, with shared runtime context and binding support.
    /// @details
    /// Lifetime hooks are driven by ScriptSystem: on_start once before the first update, on_update /
    /// on_fixed_update each tick, and on_destroy when the binding is removed. Subclasses override the
    /// hooks they need; the empty defaults make every hook optional. It lives in the engine so compiled
    /// C++ scripts inherit it directly.
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

      public:
        virtual void on_start() {}
        virtual void on_update(const DeltaTime&) {}
        virtual void on_fixed_update(const DeltaTime&) {}
        virtual void on_destroy() {}

        // Binds the per-tick runtime context (entity/world/services). Driven by ScriptSystem before the
        // lifecycle hooks run.
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

    // The wire keys of a script reference's cross-entity value shape (see
    // parse_script_reference_value / write_script_reference_field).
    inline constexpr std::string_view SCRIPT_REFERENCE_SCRIPT_KEY = "script";
    inline constexpr std::string_view SCRIPT_REFERENCE_ENTITY_KEY = "entity";
    inline constexpr std::string_view SCRIPT_REFERENCE_BINDING_KEY = "bindingId";

    // Parses a script-reference value node into the binding it names. The value is either the
    // cross-entity object { script, entity, bindingId } (an unset entity means "on my own entity")
    // or — the legacy, pre-cross-entity shape — the referenced script asset's bare id.
    template <typename TJson>
    inline ScriptBinding parse_script_reference_value(const TJson& field)
    {
        auto binding = ScriptBinding {};
        if (field.is_object())
        {
            binding.script = Uuid(field.value(std::string(SCRIPT_REFERENCE_SCRIPT_KEY), static_cast<uint64>(0U)));
            binding.entity = Uuid(field.value(std::string(SCRIPT_REFERENCE_ENTITY_KEY), static_cast<uint64>(0U)));
            binding.binding_id =
                Uuid(field.value(std::string(SCRIPT_REFERENCE_BINDING_KEY), static_cast<uint64>(0U)));
        }
        else
        {
            auto reference = Handle {};
            read_serialization_value(field, reference);
            binding.script = reference.id;
        }
        return binding;
    }

    // A script reference serializes as a first-class "script" wire shape: { type: "script", value:
    // { script, entity, bindingId } }. The entity + binding id pin the exact gadget instance — on
    // another entity when set (the editor's cross-entity wires); unset entity = "on my own entity",
    // which is also how every legacy same-entity reference reads. The editor renders the token as a
    // wireable gadget plug rather than an asset picker.
    template <typename TJson, typename TScript>
    inline void read_script_reference_field(
        const TJson& json,
        std::string_view field_name,
        Script& owner,
        std::weak_ptr<TScript>& script)
    {
        static_assert(std::derived_from<TScript, Script>);
        auto binding = ScriptBinding {};
        const auto key = make_serialization_json_key(field_name);
        if (const auto field = json.find(key); field != json.end())
            binding = parse_script_reference_value(*field);
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
        // Prefer the live target's own binding (it knows its entity + binding id exactly); fall back
        // to the stored reference so an unresolved wire still round-trips.
        auto binding = ScriptBinding {};
        if (const auto resolved = script.lock())
        {
            if (const auto resolved_binding = resolved->get_script_binding())
                binding = *resolved_binding;
        }
        if (!binding.script.is_valid() && !binding.binding_id.is_valid())
        {
            if (const auto stored_binding = owner.get_script_reference(field_name))
                binding = *stored_binding;
        }

        auto value = TJson::object();
        value[std::string(SCRIPT_REFERENCE_SCRIPT_KEY)] = binding.script.value;
        value[std::string(SCRIPT_REFERENCE_ENTITY_KEY)] = binding.entity.value;
        value[std::string(SCRIPT_REFERENCE_BINDING_KEY)] = binding.binding_id.value;
        json[make_serialization_json_key(field_name)] = std::move(value);
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
        if (!binding.has_value() || (!binding->script.is_valid() && !binding->binding_id.is_valid()))
        {
            script = {};
            return;
        }

        // The stored entity pins a cross-entity target; unset means the owner's own entity.
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
