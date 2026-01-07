#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
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
        ~Application();

        // Starts the application main loop. Returns process exit code.
        int run();

        const std::string& get_name() const;
        AppSettings& get_settings();
        IMessageDispatcher& get_dispatcher();
        IFileSystem& get_filesystem();

      private:
        void initialize(const std::vector<std::string>& requested_plugins);
        void update(DeltaTimer timer);
        void shutdown();
        void recieve_message(const Message& msg);

      private:
        bool _should_exit = false;
        std::string _name = "App";
        AppMessageCoordinator _msg_coordinator = {};
        std::vector<LoadedPlugin> _loaded = {};
        AppSettings _settings;
        Window _main_window;
        FileSystem _filesystem;
    };
}
