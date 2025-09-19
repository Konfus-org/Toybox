#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/IRuntime.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Layers/LayerManager.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Memory/Refs.h"
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

        Tbx::Ref<EventBus> GetEventBus();

        // TODO: hide behind some methods AddLayer<LayerType>(args...), RemoveLayer(name), GetLayer(name), GetLayers<Type>(), etc...
        // TODO: ensure each layer has a unique name.
        Tbx::Ref<LayerManager> GetLayerManager();

        // TODO: Get rid of window manager and make the app fully own windows.
        // They should be behind some methods like layer: OpenNewWindow(name, mode, size=default), GetWindow(id or name), etc..
        Tbx::Ref<WindowManager> GetWindowManager();

        Tbx::Ref<PluginServer> GetPluginServer();
        Tbx::Ref<AssetServer> GetAssetServer();

        void AddRuntime(const Tbx::Ref<IRuntime>& runtime);
        void RemoveRuntime(const Tbx::Ref<IRuntime>& runtime);
        std::vector<Tbx::Ref<IRuntime>> GetRuntimes() const;

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
        Tbx::Ref<EventBus> _eventBus = nullptr;
        Tbx::Ref<LayerManager> _layerManager = nullptr;
        Tbx::Ref<PluginServer> _pluginServer = nullptr;
        Tbx::Ref<AssetServer> _assetServer = nullptr;

    };
}
