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

    class App : std::enable_shared_from_this<App>
    {
    public:
        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT virtual ~App();

        EXPORT void Run();
        EXPORT void Close();

        EXPORT void SetSettings(const Settings& settings);
        EXPORT const Settings& GetSettings() const;

        EXPORT const AppStatus& GetStatus() const;
        EXPORT const std::string& GetName() const;

        EXPORT std::shared_ptr<EventBus> GetEventBus();
        EXPORT std::shared_ptr<LayerManager> GetLayerManager();
        EXPORT std::shared_ptr<WindowManager> GetWindowManager();
        EXPORT std::shared_ptr<PluginServer> GetPluginServer();
        EXPORT std::shared_ptr<AssetServer> GetAssetServer();

        EXPORT void AddRuntime(const std::shared_ptr<IRuntime>& runtime);
        EXPORT void RemoveRuntime(const std::shared_ptr<IRuntime>& runtime);
        EXPORT std::vector<std::shared_ptr<IRuntime>> GetRuntimes() const;

    protected:
        EXPORT virtual void OnLaunch() {};
        EXPORT virtual void OnUpdate() {};
        EXPORT virtual void OnShutdown() {};

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
