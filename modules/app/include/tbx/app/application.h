#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entities.h"
#include "tbx/files/filesystem.h"
#include "tbx/graphics/graphics_api.h"
#include "tbx/graphics/window.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/plugin_api/plugin_host.h"
#include "tbx/time/delta_time.h"
#include <cstddef>
#include <string>
#include <vector>

namespace tbx
{
    struct TBX_API AppSettings
    {
        AppSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = true,
            GraphicsApi api = GraphicsApi::OpenGL,
            Size resolution = {0, 0});

        Observable<AppSettings, bool> vsync_enabled;
        Observable<AppSettings, GraphicsApi> graphics_api;
        Observable<AppSettings, Size> resolution;
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
        /// Purpose: Returns the application filesystem service.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a reference owned by the application.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        IFileSystem& get_filesystem() override;

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
        EntityRegistry _entity_registry = {};
        AppMessageCoordinator _msg_coordinator = {};
        std::vector<LoadedPlugin> _loaded = {};
        AppSettings _settings;
        Window _main_window;
        FileSystem _filesystem;
        AssetManager _asset_manager;
        size_t _update_count = 0;
    };
}
