#pragma once
#include "Input/InputAPI.h"
#include "Renderer/RendererAPI.h"
#include "Windowing/WindowingAPI.h"
#include "Layers/LayersAPI.h"
#include "Plugins/PluginsAPI.h"
#include <Tbx/Core/Core.h>

namespace Tbx
{
    // TODO: this should be a singleton! Then maybe turn window manager to window stack and have this own it (so it doesn't have to be a singleton) and game should be layer plugin? So we can hot reload!!!
    // TODO: fix plugins... I've changed a lot and moved a ton of stuff to runtime so I have to restructure plugins to account for that. I've also flattened the GLFW input and window plugins into one GLFW plugin...
    class App
    {
    public:
        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT virtual ~App();

        EXPORT void Launch(bool headless = false);
        EXPORT void Update();
        EXPORT void Close();

        EXPORT virtual void OnStart() = 0;
        EXPORT virtual void OnUpdate() = 0;
        EXPORT virtual void OnShutdown() = 0;

        EXPORT void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const;

        EXPORT void PushLayer(const std::shared_ptr<Layer>& layer);
        EXPORT void PushOverlay(const std::shared_ptr<Layer>& layer);

        EXPORT bool IsRunning() const;
        EXPORT const std::string& GetName() const;
        EXPORT std::weak_ptr<IWindow> GetMainWindow() const;

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
