#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/IRuntime.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Layers/LayerStack.h"
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

        void Run();
        void Close();

        void SetSettings(const Settings& settings);
        const Settings& GetSettings() const;

        const AppStatus& GetStatus() const;
        const std::string& GetName() const;

        Tbx::Ref<EventBus> GetEventBus();

        // TODO: Get rid of window manager and make the app fully own windows.
        // They should be behind some methods like layer: OpenNewWindow(name, mode, size=default), GetWindow(id or name), etc..
        Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080));
        void CloseWindow(const Uid& id);
        void CloseAllWindows();
        std::vector<Tbx::Ref<IWindow>> GetOpenWindows() const;
        Tbx::Ref<IWindow> GetWindow(const Uid& id) const;
        Tbx::Ref<IWindow> GetMainWindow() const;

        bool AddLayer(const Tbx::Ref<Layer>& layer);

        bool RemoveLayer(const std::string& name);
        bool RemoveLayer(const Tbx::Ref<Layer>& layer);

        Tbx::Ref<Layer> GetLayer(const std::string& name) const;
        std::vector<Tbx::Ref<Layer>> GetLayers() const;

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

        Tbx::Ref<WindowManager> GetWindowManager() const;

    private:
        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        Tbx::Ref<EventBus> _eventBus = nullptr;
        LayerStack _layers = {};
        Tbx::Ref<PluginServer> _pluginServer = nullptr;
        Tbx::Ref<AssetServer> _assetServer = nullptr;

    };
}
