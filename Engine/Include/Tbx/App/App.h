#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Windowing/WindowManager.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// High level lifecycle states the application can transition through while running.
    /// </summary>
    enum class AppStatus
    {
        None = 0,
        Initializing,
        Running,
        Reloading,
        Closing,
        Closed,
        Error
    };

    /// <summary>
    /// Coordinates engine services, manages layers, and drives the lifetime of a Toybox application instance.
    /// </summary>
    class TBX_EXPORT App : std::enable_shared_from_this<App>
    {
    public:
        explicit(false) App(const std::string_view& name);
        virtual ~App();

        /// <summary>
        /// Starts the application run loop and executes until shutdown is requested.
        /// </summary>
        void Run();

        /// <summary>
        /// Requests that the application terminate after the current frame.
        /// </summary>
        void Close();

        /// <summary>
        /// Applies the provided settings to the running application instance.
        /// </summary>
        void SetSettings(const Settings& settings);

        /// <summary>
        /// Retrieves the current application settings.
        /// </summary>
        const Settings& GetSettings() const;

        /// <summary>
        /// Returns the status associated with the application's lifecycle.
        /// </summary>
        const AppStatus& GetStatus() const;

        /// <summary>
        /// Provides the name used to identify the application instance.
        /// </summary>
        const std::string& GetName() const;

        /// <summary>
        /// Exposes the event bus used to publish and subscribe to engine events.
        /// </summary>
        Ref<EventBus> GetEventBus() const;

        /// <summary>
        /// Provides access to the asset server responsible for content streaming.
        /// </summary>
        Ref<WindowManager> GetWindowManager() const;

        /// <summary>
        /// Returns the plugin server powering extensible engine systems.
        /// </summary>
        Ref<PluginServer> GetPluginServer() const;

        /// <summary>
        /// Provides access to the asset server responsible for content streaming.
        /// </summary>
        Ref<AssetServer> GetAssetServer() const;

        /// <summary>
        /// Registers a new layer with the application-managed layer manager.
        /// </summary>
        template <typename TLayer, typename... TArgs>
        Uid AddLayer(TArgs&&... args)
        {
            return _layerStack.Push<TLayer>(std::forward<TArgs>(args)...);
        }

        /// <summary>
        /// Removes a specific layer instance from the layer manager.
        /// </summary>
        void RemoveLayer(const Uid& layer);

    protected:
        virtual void OnLaunch() {};
        virtual void OnUpdate() {};
        virtual void OnShutdown() {};

    private:
        void Initialize();
        void Update();
        void Shutdown();
        void OnWindowClosed(const WindowClosedEvent& e);

    private:
        std::string _name = "";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        LayerStack _layerStack = {};
        Ref<EventBus> _eventBus = nullptr;
        Ref<WindowManager> _windowManager = nullptr;
        Ref<PluginServer> _pluginServer = nullptr;
        Ref<AssetServer> _assetServer = nullptr;
    };
}
