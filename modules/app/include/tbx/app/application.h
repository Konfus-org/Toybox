#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entities.h"
#include "tbx/files/filesystem.h"
#include "tbx/graphics/graphics_api.h"
#include "tbx/graphics/window.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/time/delta_time.h"
#include <string>
#include <vector>

namespace tbx
{
    struct TBX_API AppSettings
    {
        AppSettings(IMessageDispatcher& dispatcher, bool vsync, GraphicsApi api, Size resolution);

        Observable<AppSettings, bool> vsync_enabled;
        Observable<AppSettings, GraphicsApi> graphics_api;
        Observable<AppSettings, Size> resolution;
    };

    class TBX_API Application
    {
      public:
        Application(const AppDescription& desc);
        ~Application() noexcept;

        /// <summary>
        /// Purpose: Runs the application main loop and returns the process exit code.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership of application resources.
        /// Thread Safety: Not thread-safe; call from the main thread.
        /// </remarks>
        int run();

        const std::string& get_name() const;
        AppSettings& get_settings();
        IMessageDispatcher& get_dispatcher();
        ECS& get_ecs();
        IFileSystem& get_filesystem();

        /// <summary>
        /// Purpose: Returns the application-owned asset manager.
        /// </summary>
        /// <remarks>
        /// Ownership: The application retains ownership; callers receive a reference.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        AssetManager& get_asset_manager();

      private:
        void initialize(const std::vector<std::string>& requested_plugins);
        void update(DeltaTimer timer);
        void shutdown();
        void recieve_message(const Message& msg);

      private:
        bool _should_exit = false;
        std::string _name = "App";
        ECS _ecs = {};
        AppMessageCoordinator _msg_coordinator = {};
        std::vector<LoadedPlugin> _loaded = {};
        AppSettings _settings;
        Window _main_window;
        FileSystem _filesystem;
        AssetManager _asset_manager = {};
    };
}
