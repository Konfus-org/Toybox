#pragma once
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Plugin API/PluginInterfaces.h"
#include "Tbx/Graphics/GraphicsSettings.h"

namespace Tbx
{
    enum class AppStatus
    {
        None = 0,
        Initializing,
        Running,
        Reloading,
        Restarting,
        Exiting,
        Closed,
        Error
    };

    class App : public IPlugin
    {
    public:

        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

        EXPORT static std::shared_ptr<App> GetInstance();

        EXPORT void Launch();
        EXPORT void Update();
        EXPORT void Close();

        EXPORT virtual void OnLaunch() = 0;
        EXPORT virtual void OnUpdate() = 0;
        EXPORT virtual void OnShutdown() = 0;

        EXPORT void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const;
        EXPORT void PushLayer(const std::shared_ptr<Layer>& layer);

        EXPORT const AppStatus& GetStatus() const;
        EXPORT const std::string& GetName() const;
        EXPORT std::weak_ptr<IWindow> GetMainWindow() const;

        EXPORT void SetGraphicsSettings(const GraphicsSettings& settings);
        EXPORT const GraphicsSettings& GetGraphicsSettings() const;

    private:
        void ShutdownSystems();
        void OnWindowClosed(const WindowClosedEvent& e);

        static std::shared_ptr<App> _instance;

        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        GraphicsSettings _graphicsSettings = {};
        LayerStack _layerStack = {};
        UID _windowClosedEventId = -1;
    };
}
