#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/IRuntime.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Layers/LayerManager.h"
#include "Tbx/Plugins/PluginServer.h"
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

        void Run();
        void Close();

        void SetSettings(const Settings& settings);
        const Settings& GetSettings() const;

        const AppStatus& GetStatus() const;
        const std::string& GetName() const;

        std::shared_ptr<EventBus> GetEventBus();
        std::shared_ptr<LayerManager> GetLayerManager();
        std::shared_ptr<WindowManager> GetWindowManager();
        std::shared_ptr<PluginServer> GetPluginServer();
        std::shared_ptr<AssetServer> GetAssetServer();

        void AddRuntime(const std::shared_ptr<IRuntime>& runtime);
        void RemoveRuntime(const std::shared_ptr<IRuntime>& runtime);
        std::vector<std::shared_ptr<IRuntime>> GetRuntimes() const;

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
        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        std::shared_ptr<EventBus> _eventBus = nullptr;
        std::shared_ptr<LayerManager> _layerManager = nullptr;
        std::shared_ptr<PluginServer> _pluginServer = nullptr;
        std::shared_ptr<AssetServer> _assetServer = nullptr;

    };
}
