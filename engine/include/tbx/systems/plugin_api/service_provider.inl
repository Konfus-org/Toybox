#pragma once

namespace tbx
{
    struct ServiceProvider::ServiceEntryBase
    {
        virtual ~ServiceEntryBase() noexcept = default;

        virtual long service_use_count() const = 0;
        virtual std::type_index service_type() const = 0;
    };

    template <typename TService>
    struct ServiceProvider::ServiceEntry final : ServiceEntryBase
    {
        ServiceEntry(std::shared_ptr<TService> value)
            : service(std::move(value))
        {
        }

        long service_use_count() const override
        {
            return service.use_count();
        }

        std::type_index service_type() const override
        {
            return std::type_index(typeid(TService));
        }

        std::shared_ptr<TService> service = nullptr;
    };

    template <typename TService, typename TImplementation>
        requires std::derived_from<TImplementation, TService>
    void ServiceProvider::register_service(std::shared_ptr<TImplementation> service)
    {
        TBX_ASSERT(service != nullptr, "Cannot register a null service instance.");
        if (!service)
            return;

        std::type_index key(typeid(TService));
        deregister_service(key);
        _registration_order.push_back(key);
        std::shared_ptr<TService> casted_service = std::move(service);
        _entries[key] = std::make_unique<ServiceEntry<TService>>(std::move(casted_service));

        active_plugin_runtime().add_service(key);
    }

    template <typename TService>
    bool ServiceProvider::has_service() const
    {
        return !try_get_service<TService>().expired();
    }

    template <typename TService>
    std::weak_ptr<TService> ServiceProvider::get_service()
    {
        auto service = try_get_service<TService>();
        TBX_ASSERT(
            !service.expired(),
            "Requested service type is not registered: {}",
            typeid(TService).name());
        return service;
    }

    template <typename TService>
    std::weak_ptr<const TService> ServiceProvider::get_service() const
    {
        const auto service = try_get_service<TService>();
        TBX_ASSERT(
            !service.expired(),
            "Requested service type is not registered: {}",
            typeid(TService).name());
        return service;
    }

    template <typename TService>
    std::weak_ptr<TService> ServiceProvider::try_get_service()
    {
        const auto& self = static_cast<const ServiceProvider&>(*this);
        auto service = self.try_get_service<TService>();
        if (service.expired())
            return {};

        return std::const_pointer_cast<TService>(service.lock());
    }

    template <typename TService>
    std::weak_ptr<const TService> ServiceProvider::try_get_service() const
    {
        std::type_index key(typeid(TService));
        const auto it = _entries.find(key);
        if (it == _entries.end() || !it->second)
            return {};

        const auto& entry = static_cast<const ServiceEntry<TService>&>(*it->second);
        if (!entry.service)
            return {};

        return std::static_pointer_cast<const TService>(entry.service);
    }

    template <typename TService>
    void ServiceProvider::deregister_service()
    {
        std::type_index key(typeid(TService));
        deregister_service(key);
    }
}
