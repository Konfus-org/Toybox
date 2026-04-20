#pragma once
#include "tbx/app/description.h"
#include "tbx/app/message_coordinator.h"
#include "tbx/app/settings.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/assets/manager.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/ecs/entity.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/window.h"
#include "tbx/input/manager.h"
#include "tbx/plugin_api/plugin_manager.h"
#include "tbx/plugin_api/service_provider.h"
#include "tbx/time/delta_time.h"
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

        ServiceProvider& get_service_provider();
        const ServiceProvider& get_service_provider() const;

      private:
        void add_default_asset_directory();
        void initialize(const std::vector<std::string>& requested_plugins);
        void update(DeltaTimer& timer);
        void fixed_update(const DeltaTime& dt);
        void shutdown();
        void recieve_message(Message& msg);

      private:
        bool _should_exit = false;
        std::string _name = "App";
        ServiceProvider _service_provider = {};
        PluginManager _plugin_manager;
        Window _main_window;

        uint _update_count = 0;
        double _time_running = 0;

        double _performance_sample_elapsed_seconds = 0.0;
        uint _performance_sample_frame_count = 0U;
        double _performance_sample_min_frame_time_ms = 0.0;
        double _performance_sample_max_frame_time_ms = 0.0;
        bool _performance_sample_has_data = false;

        double _asset_unload_elapsed_seconds = 0.0;
        double _fixed_update_accumulator_seconds = 0.0;
    };
}
