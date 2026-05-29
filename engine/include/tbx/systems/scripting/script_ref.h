#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/scripting/ref.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/systems/scripting/script_ref.generated.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <optional>

namespace tbx
{
    /// @brief
    /// Purpose: Serialized identity for a script reference.
    /// @details
    /// Ownership: Stores UUIDs only. Runtime resolver state is kept by ScriptRef and is never
    /// written to world, chunk, or prefab JSON.
    [[serializable]];
    struct ScriptRefIdentity
    {
        [[prop]]
        Uuid entity = {};

        [[prop]]
        Uuid script = {};

        [[prop]]
        Uuid binding_id = {};
    };
}

namespace tbx
{
    /// @brief
    /// Purpose: Non-owning pointer-like reference to an active script instance.
    /// @details
    /// Ownership: Serializes only UUID identity fields inherited from ScriptRefIdentity. Resolved
    /// pointers are runtime-only and should not be cached across frames.
    template <typename TScript>
        requires std::derived_from<TScript, Script>
    class ScriptRef final
        : public ScriptRefIdentity
        , public IRef<TScript>
    {
      public:
        ScriptRef() = default;

        ScriptRef(Uuid script_id)
        {
            script = script_id;
        }

        ScriptRef(Uuid entity_id, Uuid script_id, Uuid binding_id = {})
        {
            entity = entity_id;
            script = script_id;
            this->binding_id = binding_id;
        }

      public:
        bool is_valid() const
        {
            return script.is_valid();
        }

        TScript* try_get() const override
        {
            auto* resolved = try_get_script();
            return resolved == nullptr ? nullptr : dynamic_cast<TScript*>(resolved);
        }

        explicit operator bool() const
        {
            return is_valid();
        }

      public:
        void bind_context(ScriptContext& context)
        {
            _owner_world = context.get_world_id();
            _owner_entity = context.get_entity_id();
            _resolver = std::ref(context.get_resolver());
        }

      private:
        Script* try_get_script() const
        {
            if (!_resolver.has_value() || !script.is_valid())
                return nullptr;

            return _resolver->get().try_get_script(
                ScriptLookup {
                    .world = _owner_world,
                    .entity = entity.is_valid() ? entity : _owner_entity,
                    .script = script,
                    .binding_id = binding_id,
                });
        }

      private:
        Uuid _owner_world = {};
        Uuid _owner_entity = {};
        std::optional<std::reference_wrapper<IScriptResolver>> _resolver = std::nullopt;
    };

    template <typename TScript>
        requires std::derived_from<TScript, Script>
    inline void bind_script_field(ScriptRef<TScript>& script_ref, ScriptContext& context)
    {
        script_ref.bind_context(context);
    }

    template <typename TScript>
        requires std::derived_from<TScript, Script>
    inline void serialize(Json& json, const ScriptRef<TScript>& value)
    {
        serialize(json, static_cast<const ScriptRefIdentity&>(value));
    }

    template <typename TScript>
        requires std::derived_from<TScript, Script>
    inline void deserialize(const Json& json, ScriptRef<TScript>& value)
    {
        deserialize(json, static_cast<ScriptRefIdentity&>(value));
    }
}
