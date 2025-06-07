#pragma once
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Layers/LayerStack.h"
#include "Tbx/Runtime/App/GraphicsSettings.h"
#include <Tbx/Core/Plugins/Plugin.h>

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

    class App : public Plugin
    {
    public:
        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

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

    private:
        void ShutdownSystems();
        void OnWindowClosed(const WindowClosedEvent& e);

        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        GraphicsSettings _graphicsSettings = {};
        LayerStack _layerStack = {};
        UID _windowClosedEventId = -1;
    };
}
