#pragma once
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/ref.h"
#include <memory>
#include <utility>

namespace tbx
{
    class ScriptContext;

    /// @brief
    /// Purpose: Stores an injected weak reference to a runtime service.
    /// @details
    /// Ownership: Does not keep services alive. Script injection rebinds this field when the
    /// runtime script instance is created.
    template <typename TService>
    class ServiceRef final : public IRef<TService>
    {
      public:
        ServiceRef() = default;

        ServiceRef(std::weak_ptr<TService> service)
            : _service(std::move(service))
        {
        }

      public:
        TService* try_get() const override
        {
            auto service = _service.lock();
            return service ? service.get() : nullptr;
        }

        explicit operator bool() const
        {
            return this->is_resolved();
        }

      private:
        std::weak_ptr<TService> _service = {};
    };

    template <typename TValue>
    inline void bind_script_field(TValue&, ScriptContext&)
    {
    }

    template <typename TService>
    inline void bind_script_field(ServiceRef<TService>& service, ScriptContext& context);
}
