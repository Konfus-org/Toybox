#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Owns runtime services and exposes typed lookup for plugins and systems.
    /// @details
    /// Ownership: Owns every registered service instance via std::shared_ptr and exposes weak
    /// lookups to callers.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API ServiceProvider
    {
      public:
        ServiceProvider() = default;
        ~ServiceProvider() noexcept;

      public:
        ServiceProvider(const ServiceProvider&) = delete;
        ServiceProvider& operator=(const ServiceProvider&) = delete;
        ServiceProvider(ServiceProvider&&) = delete;
        ServiceProvider& operator=(ServiceProvider&&) = delete;

      public:
        template <typename TService, typename TImplementation = TService>
            requires std::derived_from<TImplementation, TService>
        void register_service(std::shared_ptr<TImplementation> service);

        template <typename TService>
        bool has_service() const;

        template <typename TService>
        std::weak_ptr<TService> get_service();

        template <typename TService>
        std::weak_ptr<const TService> get_service() const;

        template <typename TService>
        std::weak_ptr<TService> try_get_service();

        template <typename TService>
        std::weak_ptr<const TService> try_get_service() const;

        template <typename TService>
        void deregister_service();

        void deregister_service(std::type_index service_type);
        void clear();

      private:
        struct ServiceEntryBase;
        template <typename TService>
        struct ServiceEntry;
        using Entries = std::unordered_map<std::type_index, std::unique_ptr<ServiceEntryBase>>;

      private:
        void erase_service(std::type_index service_type);
        void forget_registration_order(std::type_index service_type);
        void remember_registration_order(std::type_index service_type);

      private:
        Entries _entries = {};
        std::vector<std::type_index> _registration_order = {};
    };

    TBX_API void register_default_services(ServiceProvider& service_provider);
}

#include "tbx/systems/plugin_api/service_provider.inl"
