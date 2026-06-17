#pragma once
#include "tbx/systems/plugin_api/service_provider.h"
#include <memory>

namespace tbx
{
    class ScriptContext;

    template <typename TValue>
    inline void bind_service_field(TValue&, ServiceProvider&)
    {
    }

    template <typename TService>
    inline void bind_service_field(std::weak_ptr<TService>& service, ServiceProvider& services)
    {
        service = services.try_get_service<TService>();
    }

    template <typename TValue>
    inline void bind_plugin_field(TValue& value, ServiceProvider& services)
    {
        bind_service_field(value, services);
    }

    template <typename TValue>
    inline void bind_runtime_fields(TValue& value, ServiceProvider& services)
    {
        if constexpr (requires { tbx_bind_runtime(value, services); })
            tbx_bind_runtime(value, services);
    }

    template <typename TValue>
    inline void register_runtime_services(TValue& value, ServiceProvider& services)
    {
        if constexpr (requires { tbx_register_services(value, services); })
            tbx_register_services(value, services);
    }

    template <typename TValue>
    inline void bind_script_field(TValue&, ScriptContext&)
    {
    }

    template <typename TService>
    inline void bind_script_field(std::weak_ptr<TService>& service, ScriptContext& context);
}
