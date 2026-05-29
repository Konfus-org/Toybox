#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/service_ref.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/uuid.h"
#include <optional>

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

    class IScriptResolver
    {
      public:
        virtual ~IScriptResolver() noexcept = default;

        virtual Script* try_get_script(const ScriptLookup& lookup) = 0;
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
            World* world,
            ServiceProvider* services,
            IScriptResolver* resolver);

      public:
        Entity& entity() const;
        Uuid entity_id() const;
        IScriptResolver& resolver() const;
        ServiceProvider& services() const;
        World& world() const;
        Uuid world_id() const;

      private:
        Uuid _world_id = {};
        Entity _entity = {};
        // TODO: RAW POINTERS BAD! USE WEAK POINTERS!
        World* _world = nullptr;
        ServiceProvider* _services = nullptr;
        IScriptResolver* _resolver = nullptr;
    };

    template <typename TService>
    inline void bind_script_field(ServiceRef<TService>& service, ScriptContext& context)
    {
        service = ServiceRef<TService>(context.services().try_get_service<TService>());
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
        void bind_context(ScriptContext context);

        virtual void on_destroy() {}
        virtual void on_fixed_update(const DeltaTime&) {}
        virtual void on_start() {}
        virtual void on_update(const DeltaTime&) {}

      protected:
        Entity& entity() const;
        ServiceProvider& services() const;
        World& world() const;

      private:
        std::optional<ScriptContext> _context = std::nullopt;
    };
}
