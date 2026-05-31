#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

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
        ~ServiceProvider() noexcept = default;

      public:
        ServiceProvider(const ServiceProvider&) = delete;
        ServiceProvider& operator=(const ServiceProvider&) = delete;
        ServiceProvider(ServiceProvider&&) noexcept;
        ServiceProvider& operator=(ServiceProvider&&) noexcept;

      public:
        template <typename TService, typename TImplementation = TService>
            requires std::derived_from<TImplementation, TService>
        void register_service(std::unique_ptr<TImplementation> service);

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
        Entries _entries = {};
    };

    /// @brief
    /// Purpose: Creates the default Toybox runtime service graph.
    TBX_API ServiceProvider create_default_service_provider();
}

#include "tbx/systems/plugin_api/service_provider.inl"
