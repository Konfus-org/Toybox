#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/systems/app/app_service_provider.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <optional>

namespace tbx
{
    /// @brief
    /// Purpose: The runtime services the application caches as weak members. Any field stays null
    /// when its plugin backend was not loaded (e.g. a headless run creates neither rendering nor
    /// input).
    struct AppRuntimeServices
    {
        std::shared_ptr<IWindowManager> window_manager = {};
        std::shared_ptr<Physics> physics = {};
        std::shared_ptr<Gizmos> gizmos = {};
        std::shared_ptr<Rendering> rendering = {};
        std::shared_ptr<InputManager> input_manager = {};
    };

    /// @brief
    /// Purpose: Builds the app-owned runtime services (windowing/physics/rendering/input) whose
    /// plugin backends are present, on a service provider during startup.
    /// @details
    /// Ownership: Borrows the provider it is constructed with; the provider owns the created
    /// services. Core services are resolved from the provider, so the core factory must run first.
    /// Thread Safety: Not thread-safe; intended for single-threaded startup.
    class TBX_API AppRuntimeServiceFactory
    {
      public:
        explicit AppRuntimeServiceFactory(AppServiceProvider& services);

        /// @brief
        /// Purpose: Creates the runtime services whose backends exist and registers them on the
        /// service provider, returning the handles the application caches.
        /// @details
        /// Returns nullopt when a graphics backend exists but no window service does — rendering
        /// cannot run without a window — which the caller treats as a fatal startup error.
        std::optional<AppRuntimeServices> create(const PhysicsSettings& physics_settings);

      private:
        AppServiceProvider& _services;
    };
}
