#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/graphics_api.h"
#include "tbx/graphics/window.h"
#include "tbx/input/input_manager.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/plugin_api/plugin_host.h"
#include "tbx/time/delta_time.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Stores mutable settings and fixed runtime paths for the application.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all stored settings values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    /// </remarks>
    struct TBX_API AppSettings
    {
        /// <summary>
        /// Purpose: Initializes observable graphics settings defaults.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the dispatcher reference.
        /// Thread Safety: Not thread-safe; initialize on a single thread.
        /// </remarks>
        AppSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0});

        /// <summary>
        /// Purpose: Enables or disables vertical sync at runtime.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the latest value internally.
        /// Thread Safety: Not thread-safe; observe or mutate with external synchronization.
        /// </remarks>
        Observable<AppSettings, bool> vsync_enabled;

        /// <summary>
        /// Purpose: Selects the active graphics API.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the latest value internally.
        /// Thread Safety: Not thread-safe; observe or mutate with external synchronization.
        /// </remarks>
        Observable<AppSettings, GraphicsApi> graphics_api;

        /// <summary>
        /// Purpose: Stores the active render resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the latest value internally.
        /// Thread Safety: Not thread-safe; observe or mutate with external synchronization.
        /// </remarks>
        Observable<AppSettings, Size> resolution;

        /// <summary>
        /// Purpose: Defines the working directory used for relative path resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a path value copied at startup and not expected to change.
        /// Thread Safety: Not thread-safe; treat as immutable after initialization.
        /// </remarks>
        std::filesystem::path working_directory = {};

        /// <summary>
        /// Purpose: Defines the directory used for log file output.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a path value copied at startup and not expected to change.
        /// Thread Safety: Not thread-safe; treat as immutable after initialization.
        /// </remarks>
        std::filesystem::path logs_directory = {};
    };

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
        /// Purpose: Returns the application input manager instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        InputManager& get_input_manager() override;

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

      private:
        void initialize(const std::vector<std::string>& requested_plugins);
        void update(DeltaTimer& timer);
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
    };
}
