#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include <memory>

namespace Tbx
{
    enum class AppStatus
    {
        None = 0,
        Initializing,
        Running,
        Reloading,
        Exiting,
        Closed,
        Error
    };

    class App : public IPlugin, public HasLayers, public HasWindows, std::enable_shared_from_this<App>
    {
    public:
        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

        EXPORT void OnLoad() override;
        EXPORT void OnUnload() override;

        EXPORT void Launch();
        EXPORT void Update();
        EXPORT void Close();

        EXPORT virtual void OnLaunch() = 0;
        EXPORT virtual void OnUpdate() = 0;
        EXPORT virtual void OnShutdown() = 0;

        EXPORT const AppStatus& GetStatus() const;
        EXPORT const std::string& GetName() const;

        EXPORT std::shared_ptr<IWindow> GetMainWindow() const;
        EXPORT std::shared_ptr<WorldLayer> GetWorld() const;
        EXPORT std::shared_ptr<RenderingLayer> GetRendering() const;

        EXPORT void SetSettings(const Settings& settings);
        EXPORT const Settings& GetSettings() const;

    private:
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);

        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        Uid _mainWindowId = Invalid::Uid;
    };
}
