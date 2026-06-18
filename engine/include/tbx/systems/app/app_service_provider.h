#pragma once
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Owns the application's ServiceProvider and the get-or-create-and-register policy
    /// used to build core services during startup, keeping that wiring out of Application proper.
    /// @details
    /// Ownership: Owns the ServiceProvider via shared_ptr and hands out shares to systems that must
    /// outlive a single lookup (e.g. PluginManager, ScriptSystem).
    /// Thread Safety: Not thread-safe; intended for single-threaded startup and teardown.
    class TBX_API AppServiceProvider
    {
      public:
        /// @brief
        /// Purpose: Creates the underlying ServiceProvider if one does not exist yet.
        /// @details
        /// Idempotent so a re-entered initialize reuses the existing provider — along with any
        /// services a host already injected into it.
        void initialize();

        /// @brief
        /// Purpose: Returns the service already registered for TService, or builds one via factory
        /// and registers it.
        /// @details
        /// Calling ensure in dependency order keeps each factory seeing its prerequisites already
        /// registered, and reuses any instance a host (e.g. a test or the editor) injected.
        template <typename TService, typename TFactory>
        std::shared_ptr<TService> ensure(TFactory&& factory)
        {
            if (auto existing = _provider->try_get_service<TService>().lock())
                return existing;

            std::shared_ptr<TService> created = factory();
            _provider->register_service<TService>(created);
            return created;
        }

        /// @brief
        /// Purpose: Reports whether the underlying provider has been created.
        bool is_valid() const noexcept;

        /// @brief
        /// Purpose: Shares ownership of the underlying provider for systems that store it.
        const std::shared_ptr<ServiceProvider>& shared() const noexcept;

        /// @brief
        /// Purpose: Accesses the underlying provider for direct registration and lookup.
        ServiceProvider& get() noexcept;
        const ServiceProvider& get() const noexcept;

        /// @brief
        /// Purpose: Drops ownership of the provider during teardown.
        void reset() noexcept;

      private:
        std::shared_ptr<ServiceProvider> _provider = {};
    };
}
