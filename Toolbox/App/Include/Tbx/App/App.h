#pragma once
#include <Tbx/App/Events/WindowEvents.h>
#include <Tbx/App/Layers/LayerStack.h>
#include <Tbx/Core/Plugins/Plugin.h>

namespace Tbx
{
    class App : public Plugin<App>
    {
    public:
        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

        EXPORT void Launch(bool headless = false);
        EXPORT void Update();
        EXPORT void Close();

        EXPORT virtual void OnLaunch() = 0;
        EXPORT virtual void OnUpdate() = 0;
        EXPORT virtual void OnShutdown() = 0;

        EXPORT void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const;

        EXPORT void PushLayer(const std::shared_ptr<Layer>& layer);
        EXPORT void PushOverlay(const std::shared_ptr<Layer>& layer);

        EXPORT bool IsRunning() const;
        EXPORT const std::string& GetName() const;
        EXPORT std::weak_ptr<IWindow> GetMainWindow() const;

        EXPORT static std::weak_ptr<App> GetInstance() { return _instance; }

    private:
        static std::shared_ptr<App> _instance;

        bool _isRunning = false;
        bool _isHeadless = false;
        std::string _name = "App";
        LayerStack _layerStack;
        UID _windowClosedEventId;

        void ShutdownSystems();

        void OnWindowClosed(const WindowClosedEvent& e);
    };
}
