#pragma once
#include "tbx/debugging/macros.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Owns runtime services and exposes typed lookup for plugins and systems.
    /// @details
    /// Ownership: Owns every registered service instance via std::unique_ptr.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API ServiceProvider
    {
      public:
        ServiceProvider() = default;
        ~ServiceProvider() noexcept = default;

      public:
        ServiceProvider(const ServiceProvider&) = delete;
        ServiceProvider& operator=(const ServiceProvider&) = delete;
        ServiceProvider(ServiceProvider&&) noexcept = default;
        ServiceProvider& operator=(ServiceProvider&&) noexcept = default;

      public:
        template <typename TService, typename TImplementation = TService>
            requires std::derived_from<TImplementation, TService>
        void register_service(std::unique_ptr<TImplementation> service);

        template <typename TService>
        bool has_service() const;

        template <typename TService>
        TService& get_service();

        template <typename TService>
        const TService& get_service() const;

        template <typename TService>
        TService* try_get_service();

        template <typename TService>
        const TService* try_get_service() const;

        template <typename TService>
        void deregister_service();

        void clear();

      private:
        struct ServiceEntryBase
        {
            virtual ~ServiceEntryBase() noexcept = default;
        };

        template <typename TService>
        struct ServiceEntry final : ServiceEntryBase
        {
            ServiceEntry(std::unique_ptr<TService> value)
                : service(std::move(value))
            {
            }

            std::unique_ptr<TService> service = nullptr;
        };

      private:
        std::unordered_map<std::type_index, std::unique_ptr<ServiceEntryBase>> _entries = {};
    };
}

#include "tbx/plugin_api/service_provider.inl"
