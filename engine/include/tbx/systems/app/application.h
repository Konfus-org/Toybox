#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/app/description.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/streamer.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/time/delta_time.h"
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    class TBX_API Application
    {
      public:
        Application(const AppDescription& desc);
        ~Application() noexcept;

      public:
        /// @brief
        /// Purpose: Runs the application main loop and returns the process exit code.
        /// @details
        /// Ownership: Does not transfer ownership of application resources.
        /// Thread Safety: Not thread-safe; call from the main thread.
        int run();

        /// @brief
        /// Purpose: Returns the application name.
        /// @details
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        const std::string& get_name() const;

        /// @brief
        /// Purpose: Returns the primary application window handle.
        /// @details
        /// Ownership: Returns a value owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        const Window& get_main_window() const;

        /// @brief
        /// Purpose: Returns the service provider.
        /// @details
        /// Ownership: Returns a value owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        ServiceProvider& get_service_provider();

        /// @brief
        /// Purpose: Returns the const service provider.
        /// @details
        /// Ownership: Returns a value owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        const ServiceProvider& get_service_provider() const;

      private:
        void initialize(const std::vector<std::string>& requested_plugins);
        void fixed_update(const DeltaTime& dt);
        void update(DeltaTimer& timer);
        void shutdown();

      private:
        bool _should_exit = false;
        std::string _name = "App";

        ServiceProvider _service_provider = {};
        PluginManager _plugin_manager;
        std::weak_ptr<IMessageCoordinator> _msg_coordinator = {};
        std::weak_ptr<AppSettings> _settings = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<EntityStreamer> _entity_streamer = {};
        std::weak_ptr<ThreadManager> _thread_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        std::weak_ptr<IInputManager> _input_manager = {};
        std::weak_ptr<Physics> _physics = {};
        std::weak_ptr<Rendering> _rendering = {};
        std::weak_ptr<ScriptSystem> _script_system = {};

        uint64 _update_count = 0;
        double _time_running = 0;

        double _fixed_update_accumulator_seconds = 0.0;
    };
}
