#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/IRuntime.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Layers/LayerManager.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Windowing/WindowManager.h"
#include <memory>
#include <vector>

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
    class EXPORT App : std::enable_shared_from_this<App>
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
        void AddLayer(Ref<Layer> layer) const;

        /// <summary>
        /// Removes a layer by name from the layer manager.
        /// </summary>
        void RemoveLayer(const std::string& name) const;

        /// <summary>
        /// Removes a specific layer instance from the layer manager.
        /// </summary>
        void RemoveLayer(Ref<Layer> layer) const;

        /// <summary>
        /// Retrieves a layer by name from the layer manager.
        /// </summary>
        Tbx::Ref<Layer> GetLayer(const std::string& name) const;

        /// <summary>
        /// Returns the set of layers currently managed by the application.
        /// </summary>
        std::vector<Ref<Layer>> GetLayers() const;

        /// <summary>
        /// Opens a new window with the provided description and returns its unique identifier.
        /// </summary>
        Uid OpenWindow(const std::string& name, const WindowMode& mode = WindowMode::Fullscreen, const Size& size = Size(1920, 1080)) const;

        /// <summary>
        /// Closes the window associated with the supplied identifier.
        /// </summary>
        void CloseWindow(const Uid& id) const;

        /// <summary>
        /// Closes all open windows owned by the application.
        /// </summary>
        void CloseAllWindows() const;

        /// <summary>
        /// Returns a collection of currently open windows.
        /// </summary>
        std::vector<Ref<IWindow>> GetOpenWindows() const;

        /// <summary>
        /// Retrieves a specific window by its identifier.
        /// </summary>
        Ref<IWindow> GetWindow(const Uid& id) const;

        /// <summary>
        /// Returns the main window used by the application.
        /// </summary>
        Ref<IWindow> GetMainWindow() const;

        /// <summary>
        /// Registers a runtime with the application so it participates in updates.
        /// </summary>
        void AddRuntime(Ref<IRuntime> runtime) const;

        /// <summary>
        /// Removes a runtime from the application.
        /// </summary>
        void RemoveRuntime(Ref<IRuntime> runtime) const;

        /// <summary>
        /// Provides the runtimes currently registered with the application.
        /// </summary>
        std::vector<Ref<IRuntime>> GetRuntimes() const;

    protected:
        virtual void OnLaunch() {};
        virtual void OnUpdate() {};
        virtual void OnShutdown() {};

    private:
        void Initialize();
        void Update();
        void Shutdown();
        void OnWindowClosed(const WindowClosedEvent& e);
        Ref<WindowManager> GetWindowManager() const;

    private:
        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        Ref<EventBus> _eventBus = nullptr;
        Ref<LayerManager> _layerManager = nullptr;
        Ref<PluginServer> _pluginServer = nullptr;
        Ref<AssetServer> _assetServer = nullptr;

    };
}
