#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/app/description.h"
#include "tbx/systems/app/message_coordinator.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/time/delta_time.h"
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
#if defined(TBX_DEBUG)
        void update_debug_main_window_title(const DeltaTime& dt);
#endif
        void shutdown();
        void recieve_message(Message& msg);

      private:
        bool _should_exit = false;
        std::string _name = "App";
        ServiceProvider _service_provider = {};
        PluginManager _plugin_manager;
        Window _main_window = {};
        Rendering _rendering = {};
        std::string _main_window_base_title = {};

        uint _update_count = 0;
        double _time_running = 0;

        double _performance_sample_elapsed_seconds = 0.0;
        uint _performance_sample_frame_count = 0U;
        double _performance_sample_min_frame_time_ms = 0.0;
        double _performance_sample_max_frame_time_ms = 0.0;
        bool _performance_sample_has_data = false;

        double _asset_unload_elapsed_seconds = 0.0;
        double _fixed_update_accumulator_seconds = 0.0;

#if defined(TBX_DEBUG)
        std::string _debug_main_window_title = {};
        double _debug_window_title_elapsed_seconds = 0.0;
        uint _debug_window_title_frame_count = 0U;
#endif
    };
}
