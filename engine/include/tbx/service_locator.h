#pragma once

#include "tbx/tbx_api.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace tbx
{
    // Central registry of application-wide services. Services are stored by
    // their concrete type and can be retrieved on-demand by plugins or other
    // engine systems.
    class TBX_API ServiceLocator
    {
       public:
        ServiceLocator() = default;
        ~ServiceLocator() = default;

        ServiceLocator(const ServiceLocator&) = delete;
        ServiceLocator& operator=(const ServiceLocator&) = delete;
        ServiceLocator(ServiceLocator&&) = default;
        ServiceLocator& operator=(ServiceLocator&&) = default;

        // Removes all registered services.
        void clear() noexcept { _services.clear(); }

        // Constructs a service in-place and stores it in the locator. If a
        // service of the same type already exists, it is replaced with the new
        // instance.
        template <typename Service, typename... Args>
        Service& emplace(Args&&... args)
        {
            auto service = std::make_shared<Service>(std::forward<Args>(args)...);
            auto& entry = _services[TypeId(typeid(Service))];
            entry = std::move(service);
            return *static_cast<Service*>(entry.get());
        }

        // Registers an existing shared pointer instance as a service. If a
        // service of the same type already exists, it is replaced.
        template <typename Service>
        void add(std::shared_ptr<Service> service)
        {
            if (!service)
            {
                throw std::invalid_argument("Cannot register null service instance");
            }

            _services[TypeId(typeid(Service))] = service;
        }

        // Returns true if a service of the given type is registered.
        template <typename Service>
        [[nodiscard]] bool contains() const
        {
            return _services.contains(TypeId(typeid(Service)));
        }

        // Returns a pointer to the registered service or nullptr if it is not
        // present.
        template <typename Service>
        [[nodiscard]] Service* try_get() const
        {
            auto it = _services.find(TypeId(typeid(Service)));
            if (it == _services.end())
                return nullptr;
            return static_cast<Service*>(it->second.get());
        }

        // Returns a reference to the registered service. Throws
        // std::runtime_error if the service is not present.
        template <typename Service>
        [[nodiscard]] Service& get() const
        {
            auto* service = try_get<Service>();
            if (!service)
            {
                throw std::runtime_error(
                    std::string("Service not registered: ") + typeid(Service).name());
            }
            return *service;
        }

       private:
        using TypeId = std::type_index;
        std::unordered_map<TypeId, std::shared_ptr<void>> _services = {};
    };
}
