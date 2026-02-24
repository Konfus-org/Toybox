#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/app/app_settings.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/window.h"
#include "tbx/input/input_manager.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/plugin_api/plugin_host.h"
#include "tbx/time/delta_time.h"
#include <string>
#include <vector>

namespace tbx
{
    class TBX_API Application : public IPluginHost
    {
      public:
        Application(const AppDescription& desc);
        ~Application() noexcept override;

        /// <summary>
        /// Purpose: Runs the application main loop and returns the process exit code.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership of application resources.
        /// Thread Safety: Not thread-safe; call from the main thread.
        /// </remarks>
        int run();

        /// <summary>
        /// Purpose: Returns the application name.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        const std::string& get_name() const override;

        /// <summary>
        /// Purpose: Returns the immutable startup icon handle used for native windows.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        const Handle& get_icon_handle() const override;

        /// <summary>
        /// Purpose: Returns the mutable application settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        AppSettings& get_settings() override;

        /// <summary>
        /// Purpose: Returns the application message coordinator.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        IMessageCoordinator& get_message_coordinator() override;

        /// <summary>
        /// Purpose: Returns the application entity manager instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        EntityRegistry& get_entity_registry() override;

        /// <summary>
        /// Purpose: Returns the application-owned asset manager.
        /// </summary>
        /// <remarks>
        /// Ownership: The application retains ownership; callers receive a reference.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        AssetManager& get_asset_manager() override;

        /// <summary>
        /// Purpose: Returns the application job manager for asynchronous scheduling.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Thread-safe according to JobSystem guarantees.
        /// </remarks>
        JobSystem& get_job_system() override;

        /// <summary>
        /// Purpose: Returns the application input manager instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        InputManager& get_input_manager() override;

      private:
        void initialize(const std::vector<std::string>& requested_plugins);
        void update(DeltaTimer& timer);
        void fixed_update(const DeltaTime& dt);
        void shutdown();
        void recieve_message(Message& msg);

      private:
        bool _should_exit = false;
        std::string _name = "App";
        Handle _icon_handle = box_icon;
        EntityRegistry _entity_registry = {};
        AppMessageCoordinator _msg_coordinator = {};
        InputManager _input_manager;
        std::vector<LoadedPlugin> _loaded = {};
        AppSettings _settings;
        JobSystem _job_manager;
        Window _main_window;
        AssetManager _asset_manager;
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
