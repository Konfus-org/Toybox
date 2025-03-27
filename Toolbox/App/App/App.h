#pragma once
#include <TbxCore.h>

namespace Tbx
{
    // TODO: this should be a singleton! Then maybe turn window manager to window stack and have this own it (so it doesn't have to be a singleton) and game should be layer plugin? So we can hot reload!!!
    // TODO: fix plugins... I've changed a lot and moved a ton of stuff to runtime so I have to restructure plugins to account for that. I've also flattened the GLFW input and window plugins into one GLFW plugin...
    class App
    {
    public:
        TBX_API explicit(false) App(const std::string_view& name);
        TBX_API virtual ~App();

        TBX_API void Launch(bool headless = false);
        TBX_API void Update();
        TBX_API void Close();

        TBX_API virtual void OnStart() = 0;
        TBX_API virtual void OnUpdate() = 0;
        TBX_API virtual void OnShutdown() = 0;

        TBX_API void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const;

        TBX_API void PushLayer(const std::shared_ptr<Layer>& layer);
        TBX_API void PushOverlay(const std::shared_ptr<Layer>& layer);

        TBX_API bool IsRunning() const;
        TBX_API const std::string& GetName() const;
        TBX_API std::weak_ptr<IWindow> GetMainWindow() const;

    private:
        bool _isRunning = false;
        bool _isHeadless = false;
        std::string _name = "App";
        LayerStack _layerStack;

        UID _windowResizeEventId;
        UID _windowClosedEventId;

        void ShutdownSystems();

        void OnWindowResize(const WindowResizedEvent& e);

        void OnWindowClosed(WindowClosedEvent& e);
    };
}
